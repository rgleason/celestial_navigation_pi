#include "LunarSessionEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace lunar_session {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double WrapLongitude(double value) {
  while (value > 180.0) value -= 360.0;
  while (value < -180.0) value += 360.0;
  return value;
}

lunar_distance::GeographicPoint Destination(
    const lunar_distance::GeographicPoint& start, double course_deg,
    double distance_nm) {
  const double angular = distance_nm / 60.0 * kDegToRad;
  const double bearing = course_deg * kDegToRad;
  const double latitude = start.latitude_deg * kDegToRad;
  const double longitude = start.longitude_deg * kDegToRad;
  const double destination_latitude =
      std::asin(std::sin(latitude) * std::cos(angular) +
                std::cos(latitude) * std::sin(angular) * std::cos(bearing));
  const double destination_longitude =
      longitude +
      std::atan2(std::sin(bearing) * std::sin(angular) * std::cos(latitude),
                 std::cos(angular) -
                     std::sin(latitude) * std::sin(destination_latitude));
  return {destination_latitude / kDegToRad,
          WrapLongitude(destination_longitude / kDegToRad)};
}

struct ModelResidual {
  std::size_t observation = 0;
  int component = 0;
  double raw_arcmin = 0.0;
  double sigma_arcmin = 1.0;
  double standardized = 0.0;
};

struct Evaluation {
  bool valid = false;
  std::string error;
  std::vector<ModelResidual> residuals;
};

struct Parameters {
  double correction = 0.0;
  double latitude = 0.0;
  double longitude = 0.0;
  double bias = 0.0;
};

Evaluation Evaluate(const std::vector<SessionObservation>& observations,
                    const Options& options, const Parameters& parameters) {
  Evaluation result;
  for (std::size_t index = 0; index < observations.size(); ++index) {
    if (options.cancel_requested && options.cancel_requested()) {
      result.error = "The lunar-sequence calculation was cancelled";
      return result;
    }
    const SessionObservation& entry = observations[index];
    if (!entry.enabled) continue;
    lunar_distance::GeographicPoint position(parameters.latitude,
                                             parameters.longitude);
    if (options.moving_observer && options.speed_knots != 0.0) {
      position = Destination(
          position, options.course_true_deg,
          options.speed_knots * entry.epoch_offset_seconds / 3600.0);
    }
    lunar_distance::Observation settings = entry.settings;
    settings.index_error_arcmin += parameters.bias;
    double cached_seconds = std::numeric_limits<double>::quiet_NaN();
    lunar_distance::EphemerisSample cached_sample;
    auto shifted_ephemeris = [&entry, &cached_seconds, &cached_sample](
                                 double relative_seconds,
                                 lunar_distance::EphemerisSample* sample,
                                 std::string* error) {
      const double requested = relative_seconds + entry.epoch_offset_seconds;
      if (requested == cached_seconds) {
        *sample = cached_sample;
        return true;
      }
      if (!entry.ephemeris(requested, sample, error)) return false;
      cached_seconds = requested;
      cached_sample = *sample;
      return true;
    };
    const auto predicted = lunar_distance::PredictTimeTaggedObservation(
        settings, shifted_ephemeris, parameters.correction, position);
    if (!predicted.valid) {
      result.error = predicted.error;
      return result;
    }
    const std::array<double, 3> observed = {settings.raw_distance_deg,
                                            settings.moon_altitude_deg,
                                            settings.body_altitude_deg};
    const std::array<double, 3> model = {predicted.raw_distance_deg,
                                         predicted.moon_altitude_deg,
                                         predicted.body_altitude_deg};
    const std::array<double, 3> sigma = {
        std::max(0.05, settings.distance_uncertainty_arcmin),
        std::max(0.05, settings.moon_altitude_uncertainty_arcmin),
        std::max(0.05, settings.body_altitude_uncertainty_arcmin)};
    for (int component = 0; component < 3; ++component) {
      ModelResidual residual;
      residual.observation = index;
      residual.component = component;
      residual.raw_arcmin = (observed[component] - model[component]) * 60.0;
      residual.sigma_arcmin = sigma[component];
      residual.standardized = residual.raw_arcmin / sigma[component];
      result.residuals.push_back(residual);
    }
  }
  result.valid = !result.residuals.empty();
  if (!result.valid) result.error = "No enabled lunar observations";
  return result;
}

double HuberWeight(double standardized, bool robust) {
  if (!robust) return 1.0;
  const double magnitude = std::fabs(standardized);
  constexpr double threshold = 1.5;
  return magnitude <= threshold ? 1.0 : threshold / magnitude;
}

double Cost(const Evaluation& evaluation, bool robust) {
  double value = 0.0;
  for (const auto& residual : evaluation.residuals) {
    const double a = std::fabs(residual.standardized);
    if (!robust || a <= 1.5)
      value += residual.standardized * residual.standardized;
    else
      value += 2.0 * 1.5 * a - 1.5 * 1.5;
  }
  return value;
}

bool SolveLinear(std::vector<std::vector<double>> matrix,
                 std::vector<double> rhs, std::vector<double>* solution,
                 std::vector<std::vector<double>>* inverse = nullptr) {
  const int size = static_cast<int>(rhs.size());
  std::vector<std::vector<double>> augmented(
      size, std::vector<double>(size * 2 + 1, 0.0));
  for (int row = 0; row < size; ++row) {
    for (int column = 0; column < size; ++column)
      augmented[row][column] = matrix[row][column];
    augmented[row][size + row] = 1.0;
    augmented[row][size * 2] = rhs[row];
  }
  for (int column = 0; column < size; ++column) {
    int pivot = column;
    for (int row = column + 1; row < size; ++row)
      if (std::fabs(augmented[row][column]) >
          std::fabs(augmented[pivot][column]))
        pivot = row;
    if (std::fabs(augmented[pivot][column]) < 1e-12) return false;
    std::swap(augmented[pivot], augmented[column]);
    const double divisor = augmented[column][column];
    for (double& value : augmented[column]) value /= divisor;
    for (int row = 0; row < size; ++row) {
      if (row == column) continue;
      const double multiplier = augmented[row][column];
      for (std::size_t entry = 0; entry < augmented[row].size(); ++entry)
        augmented[row][entry] -= multiplier * augmented[column][entry];
    }
  }
  solution->assign(size, 0.0);
  if (inverse) inverse->assign(size, std::vector<double>(size, 0.0));
  for (int row = 0; row < size; ++row) {
    (*solution)[row] = augmented[row][size * 2];
    if (inverse)
      for (int column = 0; column < size; ++column)
        (*inverse)[row][column] = augmented[row][size + column];
  }
  return true;
}

Parameters FromVector(const std::vector<double>& values,
                      const Options& options) {
  Parameters result;
  int index = 0;
  result.correction = values[index++] * 3600.0;
  if (options.solve_position) {
    result.latitude = values[index++];
    result.longitude = WrapLongitude(values[index++]);
  } else {
    result.latitude = options.known_or_initial_position.latitude_deg;
    result.longitude = options.known_or_initial_position.longitude_deg;
  }
  if (options.estimate_common_index_bias) result.bias = values[index++];
  return result;
}

std::vector<double> ToVector(const Parameters& parameters,
                             const Options& options) {
  std::vector<double> result = {parameters.correction / 3600.0};
  if (options.solve_position) {
    result.push_back(parameters.latitude);
    result.push_back(parameters.longitude);
  }
  if (options.estimate_common_index_bias) result.push_back(parameters.bias);
  return result;
}

struct Fit {
  bool valid = false;
  Parameters parameters;
  Evaluation evaluation;
  double cost = std::numeric_limits<double>::infinity();
  std::vector<std::vector<double>> normal;
};

Fit Optimise(const std::vector<SessionObservation>& observations,
             const Options& options, const Parameters& seed) {
  Fit fit;
  std::vector<double> values = ToVector(seed, options);
  double damping = 1e-3;
  Evaluation current =
      Evaluate(observations, options, FromVector(values, options));
  if (!current.valid) return fit;
  double current_cost = Cost(current, options.robust_fit);
  const std::vector<double> finite_step =
      options.solve_position
          ? (options.estimate_common_index_bias
                 ? std::vector<double>{1.0 / 3600.0, 0.001, 0.001, 0.01}
                 : std::vector<double>{1.0 / 3600.0, 0.001, 0.001})
          : (options.estimate_common_index_bias
                 ? std::vector<double>{1.0 / 3600.0, 0.01}
                 : std::vector<double>{1.0 / 3600.0});

  for (int iteration = 0; iteration < options.maximum_iterations; ++iteration) {
    if (options.cancel_requested && options.cancel_requested()) return Fit();
    const int rows = static_cast<int>(current.residuals.size());
    const int columns = static_cast<int>(values.size());
    std::vector<std::vector<double>> jacobian(rows,
                                              std::vector<double>(columns));
    bool derivative_ok = true;
    for (int column = 0; column < columns; ++column) {
      if (options.cancel_requested && options.cancel_requested()) return Fit();
      std::vector<double> perturbed = values;
      perturbed[column] += finite_step[column];
      const Evaluation next =
          Evaluate(observations, options, FromVector(perturbed, options));
      if (!next.valid || next.residuals.size() != current.residuals.size()) {
        derivative_ok = false;
        break;
      }
      for (int row = 0; row < rows; ++row)
        jacobian[row][column] = (next.residuals[row].standardized -
                                 current.residuals[row].standardized) /
                                finite_step[column];
    }
    if (!derivative_ok) break;
    std::vector<std::vector<double>> normal(columns,
                                            std::vector<double>(columns, 0.0));
    std::vector<double> rhs(columns, 0.0);
    for (int row = 0; row < rows; ++row) {
      const double weight =
          HuberWeight(current.residuals[row].standardized, options.robust_fit);
      for (int a = 0; a < columns; ++a) {
        rhs[a] -=
            weight * jacobian[row][a] * current.residuals[row].standardized;
        for (int b = 0; b < columns; ++b)
          normal[a][b] += weight * jacobian[row][a] * jacobian[row][b];
      }
    }
    std::vector<std::vector<double>> damped = normal;
    for (int column = 0; column < columns; ++column)
      damped[column][column] += damping * std::max(1.0, normal[column][column]);
    std::vector<double> increment;
    if (!SolveLinear(damped, rhs, &increment)) break;
    double step_norm = 0.0;
    std::vector<double> trial = values;
    for (int column = 0; column < columns; ++column) {
      trial[column] += increment[column];
      step_norm += increment[column] * increment[column];
    }
    Parameters trial_parameters = FromVector(trial, options);
    if (std::fabs(trial_parameters.latitude) > 89.8) {
      damping *= 10.0;
      continue;
    }
    const Evaluation trial_evaluation =
        Evaluate(observations, options, trial_parameters);
    const double trial_cost = trial_evaluation.valid
                                  ? Cost(trial_evaluation, options.robust_fit)
                                  : std::numeric_limits<double>::infinity();
    if (trial_cost < current_cost) {
      values = trial;
      current = trial_evaluation;
      current_cost = trial_cost;
      fit.normal = normal;
      damping = std::max(1e-9, damping * 0.3);
      if (step_norm < 1e-12) break;
    } else {
      damping *= 10.0;
      if (damping > 1e12) break;
    }
  }
  fit.valid = current.valid;
  fit.parameters = FromVector(values, options);
  fit.evaluation = current;
  fit.cost = current_cost;
  return fit;
}

double MatrixInfinityNorm(const std::vector<std::vector<double>>& matrix) {
  double result = 0.0;
  for (const auto& row : matrix) {
    double sum = 0.0;
    for (double value : row) sum += std::fabs(value);
    result = std::max(result, sum);
  }
  return result;
}

Candidate MakeCandidate(const Fit& fit, const Options& options,
                        const std::vector<SessionObservation>& observations) {
  Candidate candidate;
  candidate.clock_correction_seconds = fit.parameters.correction;
  candidate.reference_position = {fit.parameters.latitude,
                                  WrapLongitude(fit.parameters.longitude)};
  candidate.common_index_bias_arcmin = fit.parameters.bias;
  double angular_square_sum = 0.0;
  for (const auto& residual : fit.evaluation.residuals)
    angular_square_sum += residual.raw_arcmin * residual.raw_arcmin;
  candidate.angular_rms_arcmin =
      std::sqrt(angular_square_sum /
                std::max<std::size_t>(1, fit.evaluation.residuals.size()));
  candidate.weighted_rms = std::sqrt(
      fit.cost / std::max<std::size_t>(1, fit.evaluation.residuals.size()));

  candidate.residuals.resize(observations.size());
  for (std::size_t index = 0; index < observations.size(); ++index)
    candidate.residuals[index].label = observations[index].label;
  for (const auto& residual : fit.evaluation.residuals) {
    ReadingResidual& output = candidate.residuals[residual.observation];
    if (residual.component == 0) output.distance_arcmin = residual.raw_arcmin;
    if (residual.component == 1)
      output.moon_altitude_arcmin = residual.raw_arcmin;
    if (residual.component == 2)
      output.body_altitude_arcmin = residual.raw_arcmin;
    output.standardized_max =
        std::max(output.standardized_max, std::fabs(residual.standardized));
  }
  for (auto& residual : candidate.residuals)
    residual.possible_outlier = residual.standardized_max > 3.0;

  if (!fit.normal.empty()) {
    std::vector<double> unused;
    std::vector<std::vector<double>> inverse;
    if (SolveLinear(fit.normal, std::vector<double>(fit.normal.size(), 0.0),
                    &unused, &inverse)) {
      candidate.condition_number =
          MatrixInfinityNorm(fit.normal) * MatrixInfinityNorm(inverse);
      const int degrees_of_freedom =
          std::max(1, static_cast<int>(fit.evaluation.residuals.size()) -
                          static_cast<int>(fit.normal.size()));
      const double variance_scale = fit.cost / degrees_of_freedom;
      candidate.time_uncertainty_seconds =
          3600.0 * std::sqrt(std::max(0.0, inverse[0][0] * variance_scale));
      if (options.solve_position && inverse.size() >= 3) {
        const double lat_nm =
            60.0 * std::sqrt(std::max(0.0, inverse[1][1] * variance_scale));
        const double lon_nm =
            60.0 * std::cos(fit.parameters.latitude * kDegToRad) *
            std::sqrt(std::max(0.0, inverse[2][2] * variance_scale));
        candidate.position_uncertainty_nm = std::hypot(lat_nm, lon_nm);
      }
    }
  }
  return candidate;
}

}  // namespace

Result Solve(const std::vector<SessionObservation>& observations,
             const Options& options) {
  Result result;
  if (options.cancel_requested && options.cancel_requested()) {
    result.error = "The lunar-sequence calculation was cancelled";
    return result;
  }
  int enabled = 0;
  double earliest_offset = std::numeric_limits<double>::infinity();
  double latest_offset = -std::numeric_limits<double>::infinity();
  for (const auto& observation : observations) {
    if (!observation.enabled) continue;
    ++enabled;
    earliest_offset =
        std::min(earliest_offset, observation.epoch_offset_seconds);
    latest_offset = std::max(latest_offset, observation.epoch_offset_seconds);
  }
  if (options.maximum_observations > 0 &&
      static_cast<std::size_t>(enabled) > options.maximum_observations) {
    result.error =
        "Too many observations for one lunar watch/session; select a coherent "
        "subset";
    return result;
  }
  if (enabled > 0 && options.maximum_session_span_seconds > 0.0 &&
      latest_offset - earliest_offset > options.maximum_session_span_seconds) {
    result.error =
        "The lunar observations span more than one permitted watch/session";
    return result;
  }
  const int unknowns = 1 + (options.solve_position ? 2 : 0) +
                       (options.estimate_common_index_bias ? 1 : 0);
  if (enabled * 3 <= unknowns) {
    result.error =
        "The session is not overdetermined; add another lunar observation";
    return result;
  }
  if (!(options.start_correction_seconds < options.end_correction_seconds)) {
    result.error = "The watch-correction search interval is invalid";
    return result;
  }

  std::vector<PositionSeed> position_seeds = options.position_seeds;
  if (!options.solve_position) position_seeds.clear();
  if (position_seeds.empty())
    position_seeds.push_back({options.known_or_initial_position.latitude_deg,
                              options.known_or_initial_position.longitude_deg});
  if (options.maximum_position_seeds > 0 &&
      position_seeds.size() > options.maximum_position_seeds)
    position_seeds.resize(options.maximum_position_seeds);
  std::vector<Fit> fits;
  const double seed_step =
      std::max(300.0, options.correction_seed_step_seconds);
  std::vector<double> correction_seeds = options.correction_seeds;
  for (double correction = options.start_correction_seconds;
       correction <= options.end_correction_seconds + 0.1;
       correction += seed_step)
    correction_seeds.push_back(correction);
  std::sort(correction_seeds.begin(), correction_seeds.end());
  correction_seeds.erase(
      std::unique(correction_seeds.begin(), correction_seeds.end(),
                  [](double first, double second) {
                    return std::fabs(first - second) < 30.0;
                  }),
      correction_seeds.end());
  correction_seeds.erase(
      std::remove_if(correction_seeds.begin(), correction_seeds.end(),
                     [&options](double correction) {
                       return correction < options.start_correction_seconds ||
                              correction > options.end_correction_seconds;
                     }),
      correction_seeds.end());
  if (options.maximum_correction_seeds > 0 &&
      correction_seeds.size() > options.maximum_correction_seeds) {
    std::vector<double> bounded;
    bounded.reserve(options.maximum_correction_seeds);
    if (options.maximum_correction_seeds == 1) {
      bounded.push_back(correction_seeds[correction_seeds.size() / 2]);
    } else {
      for (std::size_t index = 0; index < options.maximum_correction_seeds;
           ++index) {
        const std::size_t source = static_cast<std::size_t>(std::lround(
            static_cast<double>(index) *
            static_cast<double>(correction_seeds.size() - 1) /
            static_cast<double>(options.maximum_correction_seeds - 1)));
        bounded.push_back(correction_seeds[source]);
      }
    }
    correction_seeds.swap(bounded);
  }
  const std::size_t total_starts =
      correction_seeds.size() * position_seeds.size();
  std::size_t completed_starts = 0;
  if (options.progress) options.progress(0, total_starts);
  for (double correction : correction_seeds) {
    for (const auto& position : position_seeds) {
      if (options.cancel_requested && options.cancel_requested()) {
        result.error = "The lunar-sequence calculation was cancelled";
        return result;
      }
      Parameters seed;
      seed.correction = correction;
      seed.latitude = position.latitude_deg;
      seed.longitude = position.longitude_deg;
      const Fit fit = Optimise(observations, options, seed);
      ++completed_starts;
      if (options.progress) options.progress(completed_starts, total_starts);
      if (options.cancel_requested && options.cancel_requested()) {
        result.error = "The lunar-sequence calculation was cancelled";
        return result;
      }
      if (!fit.valid ||
          fit.parameters.correction < options.start_correction_seconds - 1.0 ||
          fit.parameters.correction > options.end_correction_seconds + 1.0)
        continue;
      bool duplicate = false;
      for (const auto& existing : fits) {
        if (std::fabs(existing.parameters.correction -
                      fit.parameters.correction) < 2.0 &&
            (!options.solve_position ||
             lunar_distance::GreatCircleDistanceNm(
                 {existing.parameters.latitude, existing.parameters.longitude},
                 {fit.parameters.latitude, fit.parameters.longitude}) < 1.0)) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) fits.push_back(fit);
    }
  }
  std::sort(fits.begin(), fits.end(), [](const Fit& first, const Fit& second) {
    return first.cost < second.cost;
  });
  if (fits.empty()) {
    result.error = "No converged joint solution in the selected time interval";
    return result;
  }
  const double best_cost = fits.front().cost;
  for (const auto& fit : fits) {
    if (result.candidates.size() >= 8 || fit.cost > best_cost + 25.0) break;
    result.candidates.push_back(MakeCandidate(fit, options, observations));
  }
  result.valid = true;
  const Candidate& best = result.candidates.front();
  if (result.candidates.size() > 1 &&
      result.candidates[1].weighted_rms < best.weighted_rms * 1.25)
    result.warnings.push_back(
        "The observations admit a competitive alternate time/position "
        "solution");
  if (best.condition_number > 1e8)
    result.warnings.push_back(
        "The solution geometry is ill-conditioned; formal uncertainties are "
        "unreliable");
  int outliers = 0;
  for (const auto& residual : best.residuals)
    if (residual.possible_outlier) ++outliers;
  if (outliers)
    result.warnings.push_back(
        "One or more readings exceed three stated standard uncertainties; "
        "inspect rather than deleting them automatically");
  if (options.estimate_common_index_bias && enabled < 3)
    result.warnings.push_back(
        "A common index bias is weakly separated from watch error with fewer "
        "than three lunar observations");
  return result;
}

}  // namespace lunar_session

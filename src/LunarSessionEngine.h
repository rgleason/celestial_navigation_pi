#ifndef CELESTIAL_NAVIGATION_LUNAR_SESSION_ENGINE_H
#define CELESTIAL_NAVIGATION_LUNAR_SESSION_ENGINE_H

#include "LunarDistanceEngine.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace lunar_session {

struct SessionObservation {
  std::string label;
  lunar_distance::Observation settings;
  lunar_distance::EphemerisFunction ephemeris;
  // Watch interval from the session reference epoch to the distance sight.
  double epoch_offset_seconds = 0.0;
  bool enabled = true;
};

struct PositionSeed {
  double latitude_deg = 0.0;
  double longitude_deg = 0.0;
  PositionSeed() {}
  PositionSeed(double latitude, double longitude)
      : latitude_deg(latitude), longitude_deg(longitude) {}
};

struct Options {
  double start_correction_seconds = -43200.0;
  double end_correction_seconds = 43200.0;
  double correction_seed_step_seconds = 3600.0;
  std::vector<double> correction_seeds;
  bool solve_position = true;
  lunar_distance::GeographicPoint known_or_initial_position;
  std::vector<PositionSeed> position_seeds;
  bool moving_observer = false;
  double course_true_deg = 0.0;
  double speed_knots = 0.0;
  bool robust_fit = true;
  bool estimate_common_index_bias = false;
  int maximum_iterations = 80;
  std::size_t maximum_observations = 12;
  double maximum_session_span_seconds = 86400.0;
  // Multi-start fitting is useful for ambiguous lunar solutions, but saved
  // sight files can contain hundreds of historic candidates.  Keep the work
  // deterministic and bounded rather than multiplying every historic seed by
  // every position seed.
  std::size_t maximum_correction_seeds = 24;
  std::size_t maximum_position_seeds = 6;
  std::function<bool()> cancel_requested;
  std::function<void(std::size_t completed, std::size_t total)> progress;
};

struct ReadingResidual {
  std::string label;
  double distance_arcmin = 0.0;
  double moon_altitude_arcmin = 0.0;
  double body_altitude_arcmin = 0.0;
  double standardized_max = 0.0;
  bool possible_outlier = false;
};

struct Candidate {
  double clock_correction_seconds = 0.0;
  lunar_distance::GeographicPoint reference_position;
  double common_index_bias_arcmin = 0.0;
  double weighted_rms = 0.0;
  double angular_rms_arcmin = 0.0;
  double time_uncertainty_seconds = 0.0;
  double position_uncertainty_nm = 0.0;
  double condition_number = 0.0;
  std::vector<ReadingResidual> residuals;
};

struct Result {
  bool valid = false;
  std::string error;
  std::vector<std::string> warnings;
  std::vector<Candidate> candidates;
};

// Jointly estimates the common watch correction and, optionally, the
// observer position at the session reference epoch from two or more timed
// lunar triples.  Raw observations are never modified.  Robust fitting only
// downweights possible outliers; every residual remains visible in Result.
Result Solve(const std::vector<SessionObservation>& observations,
             const Options& options);

}  // namespace lunar_session

#endif

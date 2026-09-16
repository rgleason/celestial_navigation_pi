// Headless Sun-Moon adapter for the DE440 path in Sight::RecomputeLunar at
// 85029280. Deliberately no analytical fallback: a missing kernel is an error.
// This adapter is part of each snapshot, so it can also be refined independently.
#include "LunarDistanceEngine.h"
#include "LunarSessionEngine.h"
#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"
extern "C" {
#include "erfa.h"
}
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace {
constexpr double pi = 3.1415926535897932384626433832795;
double degrees(double radians) { return radians * 180.0 / pi; }
using Params = std::map<std::string, double>;
double get(const Params& p, const std::string& key, double fallback) {
  const auto it = p.find(key);
  return it == p.end() ? fallback : it->second;
}
void emit(const std::string& key, double value) {
  // Report unavailable covariance explicitly, never emit invalid JSON upstream.
  std::cout << key << '\t' << std::setprecision(17) << value << '\n';
}
void require(bool ok, const std::string& error) {
  if (!ok) throw std::runtime_error(error);
}

class Ephemeris {
 public:
  Ephemeris(const std::string& path, const std::string& timestamp) {
    std::string error;
    require(kernel_.Open(path, &error), error);
    std::smatch m;
    require(std::regex_match(timestamp, m,
        std::regex(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2}(?:\.\d{1,3})?)Z)")),
        "Expected explicit UTC YYYY-MM-DDTHH:MM:SS[.sss]Z");
    eclipse::CalendarDateTime date;
    date.year = std::stoi(m[1]); date.month = std::stoi(m[2]);
    date.day = std::stoi(m[3]);
    const int hour = std::stoi(m[4]), minute = std::stoi(m[5]);
    const double second = std::stod(m[6]);
    require(date.year >= 1972 && hour < 24 && minute < 60 && second < 60,
            "Lab supports post-1972 UTC, excluding leap-second instants");
    require(eclipse::CalendarToJulianDate(date, &midnight_, &error), error);
    const auto check = eclipse::JulianDateToCalendar(midnight_);
    require(check.year == date.year && check.month == date.month && check.day == date.day,
            "Invalid calendar date");
    millis_ = std::llround((hour * 3600 + minute * 60 + second) * 1000);
  }

  lunar_distance::EphemerisFunction function(double epoch = 0) {
    return [this, epoch](double offset, lunar_distance::EphemerisSample* out,
                        std::string* error) {
      try { *out = sample(epoch + offset); return true; }
      catch (const std::exception& e) { if (error) *error = e.what(); return false; }
    };
  }

  lunar_distance::EphemerisSample sample(double offset, bool report = false) {
    // Match production's integer-millisecond offset semantics without wxDateTime
    // or dependence on the process's local timezone.
    const long long total = millis_ + std::llround(offset * 1000);
    const auto days = static_cast<long long>(std::floor(total / 86400000.0));
    long long remainder = total - days * 86400000;
    auto date = eclipse::JulianDateToCalendar(midnight_ + days);
    date.hour = remainder / 3600000; remainder %= 3600000;
    date.minute = remainder / 60000; remainder %= 60000;
    date.second = remainder / 1000.0;
    double jd;
    std::string error;
    require(eclipse::CalendarToJulianDate(date, &jd, &error), error);
    double tai = eclipse::TaiMinusUtcSeconds(date);
    if (!std::isfinite(tai)) tai = 37.0;  // frozen production behaviour
    const double tt = jd + (tai + 32.184) / 86400;
    const double tdb = tt + eclipse::TdbMinusTtSeconds(tt, jd) / 86400;
    const double et = (tdb - 2451545.0) * 86400;
    eclipse::Vector3 moon, sun;
    require(eclipse::ApparentGeocentricPosition(kernel_, 301, et, &moon, &error), error);
    require(eclipse::ApparentGeocentricPosition(kernel_, 10, et, &sun, &error), error);
    lunar_distance::EphemerisSample s;
    s.predicted_distance_deg = degrees(std::acos(std::clamp(
        eclipse::Dot(moon, sun) / (moon.Norm() * sun.Norm()), -1.0, 1.0)));
    s.moon_horizontal_parallax_deg = degrees(std::asin(6378.137 / moon.Norm()));
    s.moon_semidiameter_deg = degrees(std::asin(1737.4 / moon.Norm()));
    s.body_horizontal_parallax_deg = degrees(std::asin(6378.137 / sun.Norm()));
    s.body_semidiameter_deg = degrees(std::asin(695700.0 / sun.Norm()));
    eclipse::EarthOrientation orientation;
    orientation.tt_jd = tt; orientation.ut1_jd = jd;
    const auto mf = eclipse::IcrfToEarthFixed(moon, orientation);
    const auto sf = eclipse::IcrfToEarthFixed(sun, orientation);
    s.moon_geographic_latitude_deg = degrees(std::atan2(mf.z, std::hypot(mf.x, mf.y)));
    s.moon_geographic_longitude_deg = degrees(std::atan2(mf.y, mf.x));
    s.body_geographic_latitude_deg = degrees(std::atan2(sf.z, std::hypot(sf.x, sf.y)));
    s.body_geographic_longitude_deg = degrees(std::atan2(sf.y, sf.x));
    if (report) {
      double matrix[3][3];
      eraPnm06a(2451545.0, tt - 2451545.0, matrix);
      for (const auto& item : {std::make_pair("moon", moon), std::make_pair("sun", sun)}) {
        double v[] = {item.second.x, item.second.y, item.second.z}, a[3];
        eraRxp(matrix, v, a);
        emit(std::string(item.first) + "_ra_deg", std::fmod(degrees(std::atan2(a[1], a[0])) + 360, 360));
        emit(std::string(item.first) + "_dec_deg", degrees(std::atan2(a[2], std::hypot(a[0], a[1]))));
      }
      emit("separation_deg", s.predicted_distance_deg);
      emit("moon_gp_lat", s.moon_geographic_latitude_deg);
      emit("moon_gp_lon", s.moon_geographic_longitude_deg);
      emit("sun_gp_lat", s.body_geographic_latitude_deg);
      emit("sun_gp_lon", s.body_geographic_longitude_deg);
      emit("moon_hp_deg", s.moon_horizontal_parallax_deg);
      emit("moon_sd_deg", s.moon_semidiameter_deg);
      emit("sun_hp_deg", s.body_horizontal_parallax_deg);
      emit("sun_sd_deg", s.body_semidiameter_deg);
    }
    return s;
  }
 private:
  eclipse::SpkKernel kernel_;
  double midnight_ = 0;
  long long millis_ = 0;
};

lunar_distance::Observation settings(const Params& p) {
  lunar_distance::Observation o;
  o.use_ellipsoid = get(p, "ellipsoid", 1);
  o.raw_distance_deg = get(p, "distance", 0);
  o.moon_altitude_deg = get(p, "moon_alt", 0);
  o.body_altitude_deg = get(p, "sun_alt", 0);
  o.moon_altitude_limb = static_cast<lunar_distance::AltitudeLimb>(int(get(p, "moon_limb", 2)));
  o.body_altitude_limb = static_cast<lunar_distance::AltitudeLimb>(int(get(p, "sun_limb", 0)));
  o.moon_contact = static_cast<lunar_distance::DistanceContact>(int(get(p, "moon_contact", 0)));
  o.body_contact = static_cast<lunar_distance::DistanceContact>(int(get(p, "sun_contact", 0)));
  o.index_error_arcmin = get(p, "index_error", 0);
  o.eye_height_m = get(p, "eye_height", 6.1);
  o.pressure_hpa = get(p, "pressure", 1019.3);
  o.temperature_c = get(p, "temperature", 23.9);
  o.artificial_horizon = get(p, "artificial_horizon", 0);
  o.separate_times = get(p, "separate_times", 0);
  o.moon_time_offset_seconds = get(p, "moon_dt", 0);
  o.body_time_offset_seconds = get(p, "sun_dt", 0);
  o.moving_observer = get(p, "moving", 0);
  o.course_true_deg = get(p, "course", 0);
  o.speed_knots = get(p, "speed", 0);
  o.distance_uncertainty_arcmin = get(p, "distance_sigma", 0.2);
  o.moon_altitude_uncertainty_arcmin = get(p, "moon_sigma", 0.2);
  o.body_altitude_uncertainty_arcmin = get(p, "sun_sigma", 0.2);
  return o;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    require(argc >= 6, "Usage: lunar-lab KERNEL UTC MODE LAT LON [key=value ...]");
    Ephemeris ephemeris(argv[1], argv[2]);
    const std::string mode = argv[3];
    const lunar_distance::GeographicPoint position(std::stod(argv[4]), std::stod(argv[5]));
    require(std::isfinite(position.latitude_deg) && std::abs(position.latitude_deg) <= 90 &&
            std::isfinite(position.longitude_deg) && std::abs(position.longitude_deg) <= 180,
            "Invalid observer coordinates");
    const std::vector<std::string> keys = {"ellipsoid", "distance", "moon_alt", "sun_alt",
      "moon_limb", "sun_limb", "moon_contact", "sun_contact", "index_error", "eye_height",
      "pressure", "temperature", "artificial_horizon", "separate_times", "moon_dt", "sun_dt",
      "moving", "course", "speed", "distance_sigma", "moon_sigma", "sun_sigma", "offset",
      "solve_position"};
    Params p;
    for (int i = 6; i < argc; ++i) {
      const std::string arg = argv[i];
      const auto equals = arg.find('=');
      require(equals != std::string::npos, "Expected key=value");
      const auto key = arg.substr(0, equals);
      require(std::find(keys.begin(), keys.end(), key) != keys.end(), "Unknown parameter: " + key);
      std::size_t used = 0;
      const auto text = arg.substr(equals + 1);
      const double value = std::stod(text, &used);
      require(used == text.size() && std::isfinite(value), "Invalid number: " + arg);
      require(p.emplace(key, value).second, "Duplicate parameter: " + key);
    }
    auto o = settings(p);
    for (auto key : {"moon_limb", "sun_limb", "moon_contact", "sun_contact"}) {
      const double value = get(p, key, 0);
      require(value >= 0 && value <= 2 && value == std::floor(value), "Invalid limb/contact");
    }
    const auto function = ephemeris.function();
    const double offset = get(p, "offset", 0);
    if (mode == "ephemeris") { ephemeris.sample(offset, true); return 0; }
    if (mode == "predict" || mode == "reference") {
      ephemeris.sample(offset, true);
      if (mode == "reference") {
        o.pressure_hpa = 0; o.artificial_horizon = true; o.index_error_arcmin = 0;
        o.moon_altitude_limb = o.body_altitude_limb = lunar_distance::AltitudeLimb::Center;
        o.moon_contact = o.body_contact = lunar_distance::DistanceContact::Center;
      }
      const auto r = lunar_distance::PredictTimeTaggedObservation(o, function, offset, position);
      require(r.valid, r.error);
      emit("distance_deg", r.raw_distance_deg);
      emit("moon_alt_deg", r.moon_altitude_deg / (mode == "reference" ? 2 : 1));
      emit("sun_alt_deg", r.body_altitude_deg / (mode == "reference" ? 2 : 1));
    } else if (mode == "solve") {
      lunar_distance::SolveOptions options;
      options.start_offset_seconds = -60; options.end_offset_seconds = 60;
      options.scan_step_seconds = 10;
      const auto r = o.separate_times
          ? lunar_distance::SolveTimeTagged(o, function, options)
          : lunar_distance::SolveTime(o, function, options);
      require(r.valid, r.error);
      emit("candidate_count", r.candidates.size());
      for (std::size_t i = 0; i < r.candidates.size(); ++i) {
        const auto prefix = "candidate_" + std::to_string(i) + "_";
        const auto& c = r.candidates[i];
        emit(prefix + "seconds", c.offset_seconds);
        emit(prefix + "time_sigma", c.time_uncertainty_seconds);
        emit(prefix + "position_sigma", c.position_uncertainty_nm);
        emit(prefix + "position_count", c.positions.size());
        for (std::size_t j = 0; j < c.positions.size(); ++j) {
          emit(prefix + std::to_string(j) + "_lat", c.positions[j].latitude_deg);
          emit(prefix + std::to_string(j) + "_lon", c.positions[j].longitude_deg);
        }
      }
    } else if (mode == "position") {
      const auto r = lunar_distance::PositionAtTime(o, function, offset);
      require(r.valid, r.error);
      emit("candidate_count", r.candidates.size());
      for (std::size_t i = 0; i < r.candidates.size(); ++i) {
        emit("candidate_" + std::to_string(i) + "_lat", r.candidates[i].latitude_deg);
        emit("candidate_" + std::to_string(i) + "_lon", r.candidates[i].longitude_deg);
      }
    } else if (mode == "session") {
      // Each line is a triple; shared raw readings carry identical IDs and
      // absolute epochs. There is no averaging or rewriting of raw readings.
      std::vector<lunar_session::SessionObservation> observations;
      std::string line;
      while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream stream(line);
        lunar_session::SessionObservation entry;
        entry.settings = o; entry.settings.separate_times = true;
        require(bool(stream >> entry.epoch_offset_seconds >> entry.settings.raw_distance_deg
          >> entry.settings.moon_altitude_deg >> entry.settings.body_altitude_deg
          >> entry.settings.moon_time_offset_seconds >> entry.settings.body_time_offset_seconds
          >> entry.reading_ids[0] >> entry.reading_ids[1] >> entry.reading_ids[2]), "Invalid session row");
        std::string extra;
        require(!(stream >> extra), "Extra session field");
        entry.label = entry.reading_ids[0];
        // SessionEngine adds epoch_offset_seconds itself: its callback is
        // relative to the common watch epoch, not this triple's distance time.
        entry.ephemeris = ephemeris.function();
        observations.push_back(entry);
      }
      lunar_session::Options options;
      options.start_correction_seconds = -60; options.end_correction_seconds = 60;
      options.correction_seeds = {-30, 0, 30};
      options.solve_position = get(p, "solve_position", 1);
      options.known_or_initial_position = position;
      options.position_seeds = {{position.latitude_deg, position.longitude_deg}};
      options.moving_observer = o.moving_observer;
      options.course_true_deg = o.course_true_deg; options.speed_knots = o.speed_knots;
      const auto r = lunar_session::Solve(observations, options);
      require(r.valid, r.error);
      emit("candidate_count", r.candidates.size());
      emit("warning_count", r.warnings.size());
      for (std::size_t i = 0; i < r.candidates.size(); ++i) {
        const auto prefix = "candidate_" + std::to_string(i) + "_";
        const auto& c = r.candidates[i];
        emit(prefix + "seconds", c.clock_correction_seconds);
        emit(prefix + "lat", c.reference_position.latitude_deg);
        emit(prefix + "lon", c.reference_position.longitude_deg);
        emit(prefix + "time_sigma", c.time_uncertainty_seconds);
        emit(prefix + "position_sigma", c.position_uncertainty_nm);
        emit(prefix + "angular_rms", c.angular_rms_arcmin);
        emit(prefix + "weighted_rms", c.weighted_rms);
        for (std::size_t j = 0; j < c.residuals.size(); ++j) {
          const auto name = prefix + "residual_" + std::to_string(j) + "_";
          emit(name + "distance", c.residuals[j].distance_arcmin);
          emit(name + "moon", c.residuals[j].moon_altitude_arcmin);
          emit(name + "sun", c.residuals[j].body_altitude_arcmin);
        }
      }
    } else throw std::runtime_error("Unknown mode: " + mode);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}

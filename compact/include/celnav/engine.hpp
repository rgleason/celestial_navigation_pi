#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <limits>

namespace celnav {
struct Request {
  std::string body;
  std::string utc;  // ISO calendar UTC, seconds [0,60); leap instant rejected.
  double latitude_deg = 0, longitude_deg = 0, height_m = 0;
  double dut1_seconds = std::numeric_limits<double>::quiet_NaN();
  double tai_minus_utc = std::numeric_limits<double>::quiet_NaN();
  double polar_x_arcsec = 0, polar_y_arcsec = 0;
  bool venus_phase = false;  // Physical centre by default; navigation convention explicit.
};
struct Epoch {
  double utc_jd = 0, tt_jd = 0, tdb_jd = 0, ut1_jd = 0;
  double dut1_seconds = 0, tai_minus_utc = 0;
  bool dut1_available = false;
  std::vector<std::string> warnings;
};
struct Result {
  std::string body, source;
  Epoch epoch;
  double ra_deg = 0, declination_deg = 0, gha_deg = 0, aries_gha_deg = 0;
  double icrf_ra_deg = 0, icrf_declination_deg = 0;
  double observer_icrf_ra_deg = 0, observer_icrf_declination_deg = 0;
  double geometric_hc_deg = 0, airless_altitude_deg = 0, azimuth_deg = 0;
  double distance_km = 0, horizontal_parallax_deg = 0;
  double geocentric_semidiameter_deg = 0, observer_semidiameter_deg = 0;
  bool azimuth_defined = true;
};
struct Options {
  std::string data_directory;
  bool full_series = false;  // Developer-only comparison, requires .work full packs.
  bool legacy_orbits = false;  // Modern astrometry with old coefficients: ablation.
  int lunar_fit = 1;  // Published DE405-fit ELP/MPP02; 0=published LLR fit.
  bool cache_epoch_context = true;  // Bounded, per-instance, exact-epoch cache.
};
struct Coverage {
  bool supported = false;
  std::string reason;
};
class Engine {
 public:
  explicit Engine(Options options);
  ~Engine();
  Engine(Engine&&) noexcept;
  Engine& operator=(Engine&&) noexcept;
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Result Evaluate(const Request& request) const;
  // All results or an exception; share one Engine across a batch/search.
  std::vector<Result> EvaluateMany(const std::vector<Request>& requests) const;
  Epoch ResolveEpoch(const Request& request) const;
  // Choose a provider for the WHOLE search interval before evaluating it.
  // Malformed/reversed dates throw invalid_argument, never request fallback.
  Coverage CheckCoverage(const std::string& body, const std::string& first_utc,
                         const std::string& last_utc) const;
  static const char* Version() noexcept;
  std::vector<std::string> Bodies() const;
  std::array<double, 3> PlanetEcliptic(int planet, double tdb_jd) const;
  std::array<double, 3> MoonEcliptic(double tdb_jd) const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
#if defined(COMPACT_WITH_BASELINE) && COMPACT_WITH_BASELINE
Result EvaluateBaseline(const Request&, const Epoch&, const std::string& vsop_path);
#endif
}

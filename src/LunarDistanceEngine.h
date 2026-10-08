#ifndef CELESTIAL_NAVIGATION_LUNAR_DISTANCE_ENGINE_H
#define CELESTIAL_NAVIGATION_LUNAR_DISTANCE_ENGINE_H

#include <functional>
#include <string>
#include <vector>

namespace lunar_distance {

enum class AltitudeLimb { Lower, Center, Upper };
enum class DistanceContact { Near, Center, Far };

struct Observation {
  // WGS84 geodetic observer and exact vector parallax. False retains the
  // historical spherical reduction for independent comparison/tests.
  bool use_ellipsoid = false;
  double raw_distance_deg = 0.0;
  double moon_altitude_deg = 0.0;
  double body_altitude_deg = 0.0;
  AltitudeLimb moon_altitude_limb = AltitudeLimb::Lower;
  AltitudeLimb body_altitude_limb = AltitudeLimb::Center;
  DistanceContact moon_contact = DistanceContact::Near;
  DistanceContact body_contact = DistanceContact::Center;
  double index_error_arcmin = 0.0;
  double eye_height_m = 2.0;
  double pressure_hpa = 1013.0;
  double temperature_c = 10.0;
  bool artificial_horizon = false;
  bool dip_short = false;
  double dip_short_distance_m = 0.0;
  double distance_uncertainty_arcmin = 0.2;
  double moon_altitude_uncertainty_arcmin = 0.2;
  double body_altitude_uncertainty_arcmin = 0.2;
  bool separate_times = false;
  double moon_time_offset_seconds = 0.0;
  double body_time_offset_seconds = 0.0;
  bool moving_observer = false;
  double course_true_deg = 0.0;
  double speed_knots = 0.0;
};

struct EphemerisSample {
  // Observer-aware airless centre/semidiameter provider. Inputs are geodetic
  // latitude/longitude (deg), ellipsoidal height (m), and Moon selection.
  // False means the observation cannot be evaluated; never silently fall back.
  std::function<bool(double, double, double, bool, double*, double*, double*)>
      observer_direction;
  bool dut1_available = false;
  bool dut1_from_update = false;
  double dut1_seconds = 0;
  char dut1_quality = '?';
  double predicted_distance_deg = 0.0;
  double moon_semidiameter_deg = 0.0;
  double moon_horizontal_parallax_deg = 0.0;
  double body_semidiameter_deg = 0.0;
  double body_horizontal_parallax_deg = 0.0;
  double moon_geographic_latitude_deg = 0.0;
  double moon_geographic_longitude_deg = 0.0;
  double body_geographic_latitude_deg = 0.0;
  double body_geographic_longitude_deg = 0.0;
};

struct GeographicPoint {
  double latitude_deg;
  double longitude_deg;
  GeographicPoint(double latitude = 0.0, double longitude = 0.0)
      : latitude_deg(latitude), longitude_deg(longitude) {}
};

struct PositionGeometry {
  double moon_azimuth_deg = 0.0;
  double body_azimuth_deg = 0.0;
  // The smaller angular separation between the two true azimuths (0..180).
  double azimuth_separation_deg = 0.0;
  // The acute crossing angle of the two local altitude constraints (0..90).
  double effective_crossing_angle_deg = 0.0;
};

GeographicPoint AdvanceObserver(const GeographicPoint& reference,
                                const Observation& observation,
                                double relative_seconds);

struct PositionResult {
  bool valid = false;
  std::string error;
  std::vector<GeographicPoint> candidates;
  // Aligned with candidates. Kept per branch because the geometry can differ
  // at the two mathematical intersections.
  std::vector<PositionGeometry> geometry;
  double circle_crossing_angle_deg = 0.0;
};

struct Clearance {
  bool valid = false;
  std::string error;
  double index_correction_deg = 0.0;
  double dip_correction_deg = 0.0;
  double moon_apparent_limb_altitude_deg = 0.0;
  double body_apparent_limb_altitude_deg = 0.0;
  double apparent_distance_deg = 0.0;
  double cleared_distance_deg = 0.0;
  double moon_semidiameter_deg = 0.0;
  double moon_topocentric_semidiameter_deg = 0.0;
  double body_semidiameter_deg = 0.0;
  double moon_distance_limb_correction_deg = 0.0;
  double body_distance_limb_correction_deg = 0.0;
  double moon_altitude_limb_correction_deg = 0.0;
  double body_altitude_limb_correction_deg = 0.0;
  double moon_apparent_center_altitude_deg = 0.0;
  double body_apparent_center_altitude_deg = 0.0;
  double moon_topocentric_altitude_deg = 0.0;
  double body_topocentric_altitude_deg = 0.0;
  double moon_geocentric_altitude_deg = 0.0;
  double body_geocentric_altitude_deg = 0.0;
  double moon_horizontal_parallax_deg = 0.0;
  double body_horizontal_parallax_deg = 0.0;
  double moon_refraction_x = 0.0;
  double body_refraction_x = 0.0;
  double moon_refraction_deg = 0.0;
  double body_refraction_deg = 0.0;
  double moon_parallax_in_altitude_deg = 0.0;
  double body_parallax_in_altitude_deg = 0.0;
  double relative_azimuth_cosine = 0.0;
  double relative_azimuth_deg = 0.0;
  double cleared_distance_cosine = 0.0;
};

enum class MatchTracePhase { Scan, Refinement, Candidate };

// One auditable evaluation made by the UTC matcher.  Direct-triangle mode
// compares the ephemeris centre distance with the independently cleared
// observed distance.  Time-tagged mode compares the forward-modelled raw
// distance with the observed raw distance.
struct MatchTraceEntry {
  MatchTraceEntry() = default;
  MatchTraceEntry(MatchTracePhase trace_phase, int trace_branch,
                  int trace_iteration, double trace_offset_seconds,
                  double trace_bracket_start_seconds,
                  double trace_bracket_end_seconds,
                  double trace_model_distance_deg,
                  double trace_target_distance_deg,
                  double trace_residual_arcmin)
      : phase(trace_phase),
        branch(trace_branch),
        iteration(trace_iteration),
        offset_seconds(trace_offset_seconds),
        bracket_start_seconds(trace_bracket_start_seconds),
        bracket_end_seconds(trace_bracket_end_seconds),
        model_distance_deg(trace_model_distance_deg),
        target_distance_deg(trace_target_distance_deg),
        residual_arcmin(trace_residual_arcmin) {}
  MatchTracePhase phase = MatchTracePhase::Scan;
  int branch = 0;
  int iteration = 0;
  double offset_seconds = 0.0;
  double bracket_start_seconds = 0.0;
  double bracket_end_seconds = 0.0;
  double model_distance_deg = 0.0;
  double target_distance_deg = 0.0;
  double residual_arcmin = 0.0;
};

struct TimeCandidate {
  double offset_seconds = 0.0;
  double cleared_distance_deg = 0.0;
  double predicted_distance_deg = 0.0;
  double slope_arcmin_per_hour = 0.0;
  // A valid root can survive unavailable local diagnostics. Non-finite
  // diagnostic values must be displayed as unavailable, never as certainty.
  bool local_slope_available = false;
  bool used_one_sided_slope = false;
  bool uncertainty_available = false;
  double angular_uncertainty_arcmin = 0.0;
  double distance_uncertainty_contribution_arcmin = 0.0;
  double moon_altitude_uncertainty_contribution_arcmin = 0.0;
  double body_altitude_uncertainty_contribution_arcmin = 0.0;
  double time_uncertainty_seconds = 0.0;
  std::vector<GeographicPoint> positions;
  // Aligned with positions.
  std::vector<PositionGeometry> position_geometry;
  double position_uncertainty_nm = 0.0;
  double circle_crossing_angle_deg = 0.0;
};

struct SolveOptions {
  double start_offset_seconds = -43200.0;
  double end_offset_seconds = 43200.0;
  double scan_step_seconds = 300.0;
  double root_tolerance_seconds = 0.05;
};

struct SolveResult {
  bool valid = false;
  std::string error;
  std::vector<std::string> warnings;
  std::vector<TimeCandidate> candidates;
  std::vector<MatchTraceEntry> match_trace;
  double closest_offset_seconds = 0.0;
  double closest_residual_arcmin = 0.0;
};

struct PredictedObservation {
  bool valid = false;
  std::string error;
  double raw_distance_deg = 0.0;
  double moon_altitude_deg = 0.0;
  double body_altitude_deg = 0.0;
};

// Independent sight check, with position and UTC supplied rather than fitted
// from this observation. Clear the measured distance by subtracting the
// forward model's net limb/index/refraction/parallax correction at this
// geometry. This is model-based clearing, not Direct Triangle clearing from
// the measured altitudes. Positive error means the measured LD is too large.
struct DistanceCheck {
  bool valid = false;
  std::string error;
  double true_distance_deg = 0.0;
  double predicted_raw_distance_deg = 0.0;
  double cleared_distance_deg = 0.0;
  double lunar_error_arcmin = 0.0;
};

// Providers must return a consistent sample for a given epoch throughout one
// solve, including an immutable observer_direction closure. Exact epochs and
// provider failures are cached within that solve only. Updates take effect on
// the next solve; provider failure never triggers an implicit fallback.
using EphemerisFunction = std::function<bool(
    double offset_seconds, EphemerisSample* sample, std::string* error)>;

DistanceCheck CheckDistanceAtPosition(const Observation& observation,
                                      const EphemerisFunction& ephemeris,
                                      const GeographicPoint& known_position);

Clearance ClearDistance(const Observation& observation,
                        const EphemerisSample& ephemeris);

SolveResult SolveTime(const Observation& observation,
                      const EphemerisFunction& ephemeris,
                      const SolveOptions& options);

// Solve a genuinely sequential lunar observation. The lunar-distance reading
// is the reference epoch; Moon/body altitude time offsets are watch intervals
// and therefore remain valid even when the watch has an unknown constant UTC
// offset. Latitude/longitude at the reference epoch and the clock correction
// are solved together. Optional COG/SOG propagates the observer between the
// three readings.
SolveResult SolveTimeTagged(const Observation& observation,
                            const EphemerisFunction& ephemeris,
                            const SolveOptions& options);

// Forward model used by the joint solver and exposed for independent
// regression fixtures. The supplied position is at the lunar-distance epoch.
PredictedObservation PredictTimeTaggedObservation(
    const Observation& settings, const EphemerisFunction& ephemeris,
    double clock_correction_seconds, const GeographicPoint& reference_position);

PositionResult IntersectAltitudeCircles(
    const GeographicPoint& moon_geographic_position,
    double moon_observed_altitude_deg,
    const GeographicPoint& body_geographic_position,
    double body_observed_altitude_deg);

PositionGeometry CalculatePositionGeometry(
    const GeographicPoint& observer,
    const GeographicPoint& moon_geographic_position,
    const GeographicPoint& body_geographic_position);

PositionResult PositionAtTime(const Observation& observation,
                              const EphemerisFunction& ephemeris,
                              double correction_seconds);

double GreatCircleDistanceNm(const GeographicPoint& first,
                             const GeographicPoint& second);

}  // namespace lunar_distance

#endif

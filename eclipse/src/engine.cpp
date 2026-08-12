#include "eclipse/engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclipse {
namespace {

double AngularSeparation(const SolarLunarState& state) {
  const double denominator = state.sun_from_earth_km.Norm() *
                             state.moon_from_earth_km.Norm();
  const double cosine = std::max(-1.0, std::min(1.0,
      Dot(state.sun_from_earth_km, state.moon_from_earth_km) / denominator));
  return std::acos(cosine);
}

}  // namespace

const char* EclipseTypeName(EclipseType type) {
  switch (type) {
    case kPartialEclipse: return "partial";
    case kAnnularEclipse: return "annular";
    case kTotalEclipse: return "total";
    case kHybridEclipse: return "hybrid";
  }
  return "unknown";
}

bool EclipseEngine::OpenEphemeris(const std::string& path,
                                  std::string* error) {
  return kernel_.Open(path, error);
}

bool EclipseEngine::State(double tt_jd, double delta_t_seconds,
                          SolarLunarState* state,
                          EarthOrientation* orientation,
                          std::string* error) const {
  if (!state || !orientation) return false;
  orientation->tt_jd = tt_jd;
  orientation->ut1_jd = tt_jd - delta_t_seconds / 86400.0;
  orientation->polar_motion_x_rad = 0.0;
  orientation->polar_motion_y_rad = 0.0;
  const double tdb_jd = tt_jd +
      TdbMinusTtSeconds(tt_jd, orientation->ut1_jd) / 86400.0;
  const double et = (tdb_jd - 2451545.0) * 86400.0;
  return AstrometricPosition(kernel_, 301, 399, et,
                             &state->moon_from_earth_km, error) &&
         AstrometricPosition(kernel_, 10, 399, et,
                             &state->sun_from_earth_km, error);
}

bool EclipseEngine::Footprint(double tt_jd, double delta_t_seconds,
                              int angular_samples,
                              ShadowFootprint* footprint,
                              std::string* error) const {
  if (!footprint) return false;
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error)) return false;
  *footprint = CentralShadowFootprint(state, orientation, ellipsoid_,
                                      constants_, angular_samples);
  return true;
}

bool EclipseEngine::Local(double tt_jd, double delta_t_seconds,
                          const GeoPoint& observer, double height_metres,
                          LocalCircumstances* circumstances,
                          std::string* error) const {
  if (!circumstances) return false;
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error)) return false;
  *circumstances = EvaluateLocalCircumstances(
      state, orientation, observer, height_metres, ellipsoid_, constants_);
  return true;
}

double EclipseEngine::AxisDistance(double tt_jd, double delta_t_seconds,
                                   std::string* error) const {
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error))
    return std::numeric_limits<double>::quiet_NaN();
  const Vector3 direction = Normalize(state.moon_from_earth_km -
                                      state.sun_from_earth_km);
  return Cross(state.moon_from_earth_km, direction).Norm();
}

bool EclipseEngine::Classify(double maximum_tt_jd, double delta_t_seconds,
                             EclipseEvent* event, std::string* error) const {
  if (!event) return false;
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(maximum_tt_jd, delta_t_seconds, &state, &orientation, error))
    return false;
  event->maximum_tt_jd = maximum_tt_jd;
  event->delta_t_seconds = delta_t_seconds;
  const Vector3 away = Normalize(state.moon_from_earth_km -
                                 state.sun_from_earth_km);
  event->axis_distance_km = Cross(state.moon_from_earth_km, away).Norm();
  const double z = Dot(state.moon_from_earth_km * -1.0, away);
  const double separation = (state.moon_from_earth_km -
                             state.sun_from_earth_km).Norm();
  const double tangent = (constants_.sun_radius_km +
                          constants_.moon_radius_km) /
      std::sqrt(separation * separation - std::pow(
          constants_.sun_radius_km + constants_.moon_radius_km, 2));
  const double penumbral_radius = constants_.moon_radius_km + z * tangent;
  if (event->axis_distance_km > constants_.earth_equatorial_radius_km +
                                      penumbral_radius) {
    return false;
  }

  const ShadowFootprint footprint = CentralShadowFootprint(
      state, orientation, ellipsoid_, constants_, 720);
  if (footprint.central) {
    event->greatest_position = footprint.axis;
    event->type = footprint.total ? kTotalEclipse : kAnnularEclipse;
    LocalCircumstances local = EvaluateLocalCircumstances(
        state, orientation, footprint.axis, 0.0, ellipsoid_, constants_);
    event->magnitude = local.magnitude;
  } else {
    event->type = kPartialEclipse;
    event->magnitude = std::max(0.0,
        (constants_.earth_equatorial_radius_km + penumbral_radius -
         event->axis_distance_km) / (2.0 * penumbral_radius));
  }
  return true;
}

bool EclipseEngine::FindEvents(double start_tt_jd, double end_tt_jd,
                               std::vector<EclipseEvent>* events,
                               std::string* error,
                               double delta_t_override_seconds) const {
  if (!events || end_tt_jd <= start_tt_jd) return false;
  events->clear();
  const double step = 0.5;
  SolarLunarState previous_state;
  EarthOrientation previous_orientation;
  SolarLunarState current_state;
  EarthOrientation current_orientation;
  SolarLunarState next_state;
  EarthOrientation next_orientation;
  const double initial_delta = delta_t_override_seconds > 0.0
      ? delta_t_override_seconds
      : ModelDeltaTSeconds(DecimalYear(JulianDateToCalendar(start_tt_jd)));
  if (!State(start_tt_jd, initial_delta, &previous_state,
             &previous_orientation, error) ||
      !State(start_tt_jd + step, initial_delta, &current_state,
             &current_orientation, error)) return false;
  double previous = AngularSeparation(previous_state);
  double current = AngularSeparation(current_state);

  for (double time = start_tt_jd + step; time + step <= end_tt_jd;
       time += step) {
    const double delta_t = delta_t_override_seconds > 0.0
        ? delta_t_override_seconds
        : ModelDeltaTSeconds(DecimalYear(JulianDateToCalendar(time)));
    if (!State(time + step, delta_t, &next_state, &next_orientation, error))
      return false;
    const double next = AngularSeparation(next_state);
    if (current < previous && current <= next && current < DegreesToRadians(2.0)) {
      double left = time - step;
      double right = time + step;
      const double golden = 0.6180339887498948482;
      double x1 = right - golden * (right - left);
      double x2 = left + golden * (right - left);
      double f1 = AxisDistance(x1, delta_t, error);
      double f2 = AxisDistance(x2, delta_t, error);
      if (!std::isfinite(f1) || !std::isfinite(f2)) return false;
      for (int iteration = 0; iteration < 70; ++iteration) {
        if (f1 > f2) {
          left = x1; x1 = x2; f1 = f2;
          x2 = left + golden * (right - left);
          f2 = AxisDistance(x2, delta_t, error);
        } else {
          right = x2; x2 = x1; f2 = f1;
          x1 = right - golden * (right - left);
          f1 = AxisDistance(x1, delta_t, error);
        }
      }
      EclipseEvent event;
      if (Classify(0.5 * (left + right), delta_t, &event, error))
        events->push_back(event);
    }
    previous = current;
    current = next;
    previous_state = current_state;
    current_state = next_state;
  }
  return true;
}

bool EclipseEngine::BuildCentralPath(const EclipseEvent& event,
                                     double interval_seconds,
                                     std::vector<PathPoint>* path,
                                     std::string* error) const {
  if (!path || interval_seconds <= 0.0) return false;
  path->clear();
  const double step = interval_seconds / 86400.0;
  const double start = event.maximum_tt_jd - 4.5 / 24.0;
  const double end = event.maximum_tt_jd + 4.5 / 24.0;
  for (double time = start; time <= end + step * 0.25; time += step) {
    ShadowFootprint footprint;
    if (!Footprint(time, event.delta_t_seconds, 1440, &footprint, error))
      return false;
    if (!footprint.central || footprint.boundary.empty()) continue;

    GeoPoint before;
    GeoPoint after;
    for (int direction = -1; direction <= 1; direction += 2) {
      SolarLunarState adjacent_state;
      EarthOrientation adjacent_orientation;
      if (!State(time + direction / 86400.0, event.delta_t_seconds,
                 &adjacent_state, &adjacent_orientation, error)) return false;
      GeoPoint* output = direction < 0 ? &before : &after;
      if (!ShadowAxisPosition(adjacent_state, adjacent_orientation,
                              ellipsoid_, output)) return false;
    }
    PathPoint point;
    point.tt_jd = time;
    point.central_line = footprint.axis;
    if (!SelectCrossTrackLimits(footprint, before, after,
                                &point.northern_limit,
                                &point.southern_limit)) return false;
    if (point.northern_limit.latitude_deg < point.southern_limit.latitude_deg)
      std::swap(point.northern_limit, point.southern_limit);
    point.width_km = SurfaceDistanceKm(point.northern_limit,
                                       point.southern_limit);
    LocalCircumstances local;
    if (!Local(time, event.delta_t_seconds, point.central_line, 0.0,
               &local, error)) return false;
    point.magnitude = local.magnitude;
    path->push_back(point);
  }
  return true;
}

double EclipseEngine::ContactFunction(
    double tt_jd, double delta_t_seconds, const GeoPoint& observer,
    double height_metres, bool internal, LocalCircumstances* circumstances,
    std::string* error) const {
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error))
    return std::numeric_limits<double>::quiet_NaN();
  PhysicalConstants contact_constants = constants_;
  contact_constants.moon_radius_km =
      (internal ? 0.272281 : 0.272488) *
      contact_constants.earth_equatorial_radius_km;
  const LocalCircumstances local = EvaluateLocalCircumstances(
      state, orientation, observer, height_metres, ellipsoid_,
      contact_constants);
  if (circumstances) *circumstances = local;
  if (internal) {
    return local.separation_rad - std::fabs(
        local.moon_semidiameter_rad - local.sun_semidiameter_rad);
  }
  return local.separation_rad -
      (local.moon_semidiameter_rad + local.sun_semidiameter_rad);
}

bool EclipseEngine::SolveLocalContacts(const EclipseEvent& event,
                                       const GeoPoint& observer,
                                       double height_metres,
                                       LocalContacts* contacts,
                                       std::string* error) const {
  if (!contacts) return false;
  *contacts = LocalContacts();
  const double scan_start = event.maximum_tt_jd - 6.0 / 24.0;
  const double scan_end = event.maximum_tt_jd + 6.0 / 24.0;
  const double scan_step = 60.0 / 86400.0;

  std::vector<double> external_roots;
  std::vector<double> internal_roots;
  for (int mode = 0; mode < 2; ++mode) {
    const bool internal = mode == 1;
    std::vector<double>& roots = internal ? internal_roots : external_roots;
    double left = scan_start;
    double left_value = ContactFunction(left, event.delta_t_seconds, observer,
                                        height_metres, internal, NULL, error);
    if (!std::isfinite(left_value)) return false;
    for (double right = left + scan_step; right <= scan_end;
         right += scan_step) {
      const double right_value = ContactFunction(
          right, event.delta_t_seconds, observer, height_metres, internal,
          NULL, error);
      if (!std::isfinite(right_value)) return false;
      if ((left_value > 0.0 && right_value <= 0.0) ||
          (left_value <= 0.0 && right_value > 0.0)) {
        double a = left;
        double b = right;
        double fa = left_value;
        for (int iteration = 0; iteration < 45; ++iteration) {
          const double middle = 0.5 * (a + b);
          const double fm = ContactFunction(middle, event.delta_t_seconds,
              observer, height_metres, internal, NULL, error);
          if ((fa > 0.0 && fm <= 0.0) || (fa <= 0.0 && fm > 0.0)) {
            b = middle;
          } else {
            a = middle;
            fa = fm;
          }
        }
        roots.push_back(0.5 * (a + b));
      }
      left = right;
      left_value = right_value;
    }
  }
  if (external_roots.size() < 2) return true;

  const auto fill_contact = [&](double tt_jd, ContactTime* contact) -> bool {
    LocalCircumstances local;
    if (!Local(tt_jd, event.delta_t_seconds, observer, height_metres,
               &local, error)) return false;
    contact->valid = true;
    contact->tt_jd = tt_jd;
    contact->sun_altitude_deg = local.sun_altitude_deg;
    contact->sun_azimuth_deg = local.sun_azimuth_deg;
    return true;
  };
  if (!fill_contact(external_roots.front(), &contacts->c1) ||
      !fill_contact(external_roots.back(), &contacts->c4)) return false;

  // Minimize the apparent centre separation. Obscuration has a flat plateau
  // during totality, so using it as the objective can return an arbitrary
  // instant between C2 and C3 instead of the conventional local maximum.
  double left = external_roots.front();
  double right = external_roots.back();
  const double golden = 0.6180339887498948482;
  double x1 = right - golden * (right - left);
  double x2 = left + golden * (right - left);
  LocalCircumstances local1;
  LocalCircumstances local2;
  if (!Local(x1, event.delta_t_seconds, observer, height_metres, &local1,
             error) ||
      !Local(x2, event.delta_t_seconds, observer, height_metres, &local2,
             error)) return false;
  for (int iteration = 0; iteration < 60; ++iteration) {
    if (local1.separation_rad > local2.separation_rad) {
      left = x1; x1 = x2; local1 = local2;
      x2 = left + golden * (right - left);
      if (!Local(x2, event.delta_t_seconds, observer, height_metres, &local2,
                 error)) return false;
    } else {
      right = x2; x2 = x1; local2 = local1;
      x1 = right - golden * (right - left);
      if (!Local(x1, event.delta_t_seconds, observer, height_metres, &local1,
                 error)) return false;
    }
  }
  const double maximum_time = 0.5 * (left + right);
  LocalCircumstances maximum;
  if (!Local(maximum_time, event.delta_t_seconds, observer, height_metres,
             &maximum, error) ||
      !fill_contact(maximum_time, &contacts->maximum)) return false;
  contacts->magnitude = maximum.magnitude;
  contacts->obscuration = maximum.obscuration;
  contacts->type = maximum.total ? kTotalEclipse :
                   maximum.annular ? kAnnularEclipse : kPartialEclipse;
  if (internal_roots.size() >= 2) {
    if (!fill_contact(internal_roots.front(), &contacts->c2) ||
        !fill_contact(internal_roots.back(), &contacts->c3)) return false;
    contacts->central_duration_seconds =
        (contacts->c3.tt_jd - contacts->c2.tt_jd) * 86400.0;
  }
  return true;
}

}  // namespace eclipse

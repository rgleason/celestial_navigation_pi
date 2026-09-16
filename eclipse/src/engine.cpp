#include "eclipse/engine.h"
#include "eclipse/lunar_limb.h"
#include "eclipse/pck.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eclipse {
namespace {

double AngularSeparation(const SolarLunarState& state) {
  const double denominator =
      state.sun_from_earth_km.Norm() * state.moon_from_earth_km.Norm();
  const double cosine = std::max(
      -1.0,
      std::min(1.0, Dot(state.sun_from_earth_km, state.moon_from_earth_km) /
                        denominator));
  return std::acos(cosine);
}

// Find the point on the illuminated terrestrial limb with the greatest
// central-eclipse margin. This covers NASA's non-central T-/T+/A-/A+ events:
// the umbra or antumbra touches Earth even though its axis misses the WGS 84
// ellipsoid. A local pattern search is sufficient because the closest-axis
// direction supplies a good analytic starting point and the state is fixed,
// so each trial is inexpensive.
bool SurfaceMaximumNearShadowAxis(const SolarLunarState& state,
                                  const EarthOrientation& orientation,
                                  const ReferenceEllipsoid& ellipsoid,
                                  const PhysicalConstants& constants,
                                  GeoPoint* point,
                                  LocalCircumstances* circumstances) {
  if (!point || !circumstances) return false;
  const EarthFixedSolarLunarState fixed = ToEarthFixed(state, orientation);
  const Vector3 axis =
      Normalize(fixed.moon_from_earth_km - fixed.sun_from_earth_km);
  const Vector3 nearest =
      fixed.moon_from_earth_km - axis * Dot(fixed.moon_from_earth_km, axis);
  if (nearest.Norm() == 0.0) return false;
  const Vector3 direction = Normalize(nearest);
  const double polar_ratio_squared =
      ellipsoid.polar_ratio() * ellipsoid.polar_ratio();
  GeoPoint best(RadiansToDegrees(std::atan2(
                    direction.z, polar_ratio_squared *
                                     std::hypot(direction.x, direction.y))),
                NormalizeLongitude(
                    RadiansToDegrees(std::atan2(direction.y, direction.x))));

  const auto evaluate = [&](const GeoPoint& candidate,
                            LocalCircumstances* local) -> double {
    *local = EvaluateLocalCircumstancesFixed(fixed, candidate, 0.0, ellipsoid,
                                             constants);
    if (local->sun_altitude_deg < -1.0) return -1e6 + local->sun_altitude_deg;
    return (local->moon_semidiameter_rad + local->sun_semidiameter_rad -
            local->separation_rad) /
           (2.0 * local->sun_semidiameter_rad);
  };
  LocalCircumstances best_local;
  double best_value = evaluate(best, &best_local);
  for (double step = 2.0; step >= 0.00005; step *= 0.5) {
    bool improved = true;
    while (improved) {
      improved = false;
      const double longitude_step =
          step / std::max(0.15, std::cos(DegreesToRadians(best.latitude_deg)));
      GeoPoint selected = best;
      LocalCircumstances selected_local = best_local;
      double selected_value = best_value;
      for (int latitude_direction = -1; latitude_direction <= 1;
           ++latitude_direction) {
        for (int longitude_direction = -1; longitude_direction <= 1;
             ++longitude_direction) {
          if (latitude_direction == 0 && longitude_direction == 0) continue;
          GeoPoint candidate(
              std::max(-89.9999,
                       std::min(89.9999,
                                best.latitude_deg + latitude_direction * step)),
              NormalizeLongitude(best.longitude_deg +
                                 longitude_direction * longitude_step));
          LocalCircumstances local;
          const double value = evaluate(candidate, &local);
          if (value > selected_value) {
            selected = candidate;
            selected_local = local;
            selected_value = value;
            improved = true;
          }
        }
      }
      best = selected;
      best_local = selected_local;
      best_value = selected_value;
    }
  }
  *point = best;
  *circumstances = best_local;
  return true;
}

}  // namespace

const char* EclipseTypeName(EclipseType type) {
  switch (type) {
    case kPartialEclipse:
      return "partial";
    case kAnnularEclipse:
      return "annular";
    case kTotalEclipse:
      return "total";
    case kHybridEclipse:
      return "hybrid";
  }
  return "unknown";
}

bool EclipseEngine::OpenEphemeris(const std::string& path, std::string* error) {
  return kernel_.Open(path, error);
}

bool EclipseEngine::State(double tt_jd, double delta_t_seconds,
                          SolarLunarState* state, EarthOrientation* orientation,
                          std::string* error) const {
  if (!state || !orientation) return false;
  orientation->tt_jd = tt_jd;
  orientation->ut1_jd = tt_jd - delta_t_seconds / 86400.0;
  orientation->polar_motion_x_rad = 0.0;
  orientation->polar_motion_y_rad = 0.0;
  const double tdb_jd =
      tt_jd + TdbMinusTtSeconds(tt_jd, orientation->ut1_jd) / 86400.0;
  const double et = (tdb_jd - 2451545.0) * 86400.0;
  return AstrometricPosition(kernel_, 301, 399, et, &state->moon_from_earth_km,
                             error) &&
         AstrometricPosition(kernel_, 10, 399, et, &state->sun_from_earth_km,
                             error);
}

bool EclipseEngine::Footprint(double tt_jd, double delta_t_seconds,
                              int angular_samples, ShadowFootprint* footprint,
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
  const Vector3 direction =
      Normalize(state.moon_from_earth_km - state.sun_from_earth_km);
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
  const Vector3 away =
      Normalize(state.moon_from_earth_km - state.sun_from_earth_km);
  event->axis_distance_km = Cross(state.moon_from_earth_km, away).Norm();
  const double z = Dot(state.moon_from_earth_km * -1.0, away);
  const double separation =
      (state.moon_from_earth_km - state.sun_from_earth_km).Norm();
  const double tangent =
      (constants_.sun_radius_km + constants_.moon_radius_km) /
      std::sqrt(
          separation * separation -
          std::pow(constants_.sun_radius_km + constants_.moon_radius_km, 2));
  const double penumbral_radius = constants_.moon_radius_km + z * tangent;
  if (event->axis_distance_km >
      constants_.earth_equatorial_radius_km + penumbral_radius) {
    return false;
  }

  const ShadowFootprint footprint =
      CentralShadowFootprint(state, orientation, ellipsoid_, constants_, 720);
  if (footprint.central) {
    event->greatest_position = footprint.axis;
    LocalCircumstances local = EvaluateLocalCircumstances(
        state, orientation, footprint.axis, 0.0, ellipsoid_, constants_);
    event->type = local.total ? kTotalEclipse : kAnnularEclipse;
    event->magnitude = local.magnitude;
    bool saw_total = local.total;
    bool saw_annular = local.annular;
    for (int minute = -180; minute <= 180; minute += 10) {
      SolarLunarState sampled_state;
      EarthOrientation sampled_orientation;
      if (!State(maximum_tt_jd + minute / 1440.0, delta_t_seconds,
                 &sampled_state, &sampled_orientation, error))
        return false;
      const ShadowFootprint sampled = CentralShadowFootprint(
          sampled_state, sampled_orientation, ellipsoid_, constants_, 36);
      if (!sampled.central) continue;
      const LocalCircumstances sampled_local =
          EvaluateLocalCircumstances(sampled_state, sampled_orientation,
                                     sampled.axis, 0.0, ellipsoid_, constants_);
      saw_total = saw_total || sampled_local.total;
      saw_annular = saw_annular || sampled_local.annular;
    }
    // Hybrid annularity can last only seconds near first or last central
    // contact, between the coarse samples above. Examine both path endpoints
    // from the central side whenever the apparent sizes are close enough for
    // a transition to be possible.
    if (!(saw_total && saw_annular) && event->magnitude > 0.97 &&
        event->magnitude < 1.03) {
      const auto central_kind = [&](double time) -> int {
        SolarLunarState edge_state;
        EarthOrientation edge_orientation;
        if (!State(time, delta_t_seconds, &edge_state, &edge_orientation,
                   error))
          return -1;
        GeoPoint edge_axis;
        if (!ShadowAxisPosition(edge_state, edge_orientation, ellipsoid_,
                                &edge_axis))
          return 0;
        const LocalCircumstances edge_local =
            EvaluateLocalCircumstances(edge_state, edge_orientation, edge_axis,
                                       0.0, ellipsoid_, constants_);
        return edge_local.total ? 1 : edge_local.annular ? 2 : 0;
      };
      for (int direction = -1; direction <= 1; direction += 2) {
        double inside = maximum_tt_jd;
        double outside = inside;
        for (int minute = 1; minute <= 360; ++minute) {
          const double candidate = maximum_tt_jd + direction * minute / 1440.0;
          const int kind = central_kind(candidate);
          if (kind < 0) return false;
          if (kind == 0) {
            outside = candidate;
            break;
          }
          inside = candidate;
        }
        if (outside == inside) continue;
        for (int iteration = 0; iteration < 40; ++iteration) {
          const double middle = 0.5 * (inside + outside);
          const int kind = central_kind(middle);
          if (kind < 0) return false;
          if (kind == 0)
            outside = middle;
          else
            inside = middle;
        }
        const int edge_kind = central_kind(inside);
        saw_total = saw_total || edge_kind == 1;
        saw_annular = saw_annular || edge_kind == 2;
      }
    }
    if (saw_total && saw_annular) event->type = kHybridEclipse;
  } else {
    GeoPoint surface_maximum;
    LocalCircumstances surface_local;
    if (!SurfaceMaximumNearShadowAxis(state, orientation, ellipsoid_,
                                      constants_, &surface_maximum,
                                      &surface_local))
      return false;
    if (surface_local.total || surface_local.annular) {
      event->greatest_position = surface_maximum;
      event->type = surface_local.total ? kTotalEclipse : kAnnularEclipse;
      event->magnitude =
          (surface_local.moon_semidiameter_rad +
           surface_local.sun_semidiameter_rad - surface_local.separation_rad) /
          (2.0 * surface_local.sun_semidiameter_rad);
    } else {
      event->type = kPartialEclipse;
      event->greatest_position = surface_maximum;
      event->magnitude =
          std::max(0.0, (surface_local.moon_semidiameter_rad +
                         surface_local.sun_semidiameter_rad -
                         surface_local.separation_rad) /
                            (2.0 * surface_local.sun_semidiameter_rad));
    }
  }
  return true;
}

bool EclipseEngine::EvaluateEvent(double maximum_tt_jd, double delta_t_seconds,
                                  EclipseEvent* event,
                                  std::string* error) const {
  return Classify(maximum_tt_jd, delta_t_seconds, event, error);
}

bool EclipseEngine::FindEvents(double start_tt_jd, double end_tt_jd,
                               std::vector<EclipseEvent>* events,
                               std::string* error,
                               double delta_t_override_seconds) const {
  if (!events || end_tt_jd <= start_tt_jd) return false;
  events->clear();
  // Six-hour sampling guarantees a conjunction is sampled within three
  // hours; the Moon can move more than two degrees in six hours, so a
  // twelve-hour cadence can miss an eclipse falling midway between samples.
  const double step = 0.25;
  SolarLunarState previous_state;
  EarthOrientation previous_orientation;
  SolarLunarState current_state;
  EarthOrientation current_orientation;
  SolarLunarState next_state;
  EarthOrientation next_orientation;
  const double initial_delta =
      delta_t_override_seconds > 0.0
          ? delta_t_override_seconds
          : ModelDeltaTSeconds(DecimalYear(JulianDateToCalendar(start_tt_jd)));
  if (!State(start_tt_jd, initial_delta, &previous_state, &previous_orientation,
             error) ||
      !State(start_tt_jd + step, initial_delta, &current_state,
             &current_orientation, error))
    return false;
  double previous = AngularSeparation(previous_state);
  double current = AngularSeparation(current_state);

  for (double time = start_tt_jd + step; time + step <= end_tt_jd;
       time += step) {
    const double delta_t =
        delta_t_override_seconds > 0.0
            ? delta_t_override_seconds
            : ModelDeltaTSeconds(DecimalYear(JulianDateToCalendar(time)));
    if (!State(time + step, delta_t, &next_state, &next_orientation, error))
      return false;
    const double next = AngularSeparation(next_state);
    // The Moon can be about 5.2 degrees from the Sun at a non-eclipsing new
    // moon, and the nearest six-hour sample can add about 1.7 degrees of
    // longitudinal separation. Seven degrees therefore admits every possible
    // conjunction to the exact shadow-axis test below. The former two-degree
    // filter missed grazing polar partials and, at an unlucky sample phase,
    // the high-latitude total eclipse of 2072-09-12.
    if (current < previous && current <= next &&
        current < DegreesToRadians(7.0)) {
      // Axis distance is not guaranteed to be unimodal across the full day
      // around conjunction for high-gamma eclipses. First isolate its lowest
      // coarse interval, then apply the golden-section minimizer locally.
      const int coarse_intervals = 48;
      const double coarse_step = (2.0 * step) / coarse_intervals;
      double best_time = time;
      double best_value = AxisDistance(time, delta_t, error);
      for (int sample = 0; sample <= coarse_intervals; ++sample) {
        const double sample_time = time - step + sample * coarse_step;
        const double sample_value = AxisDistance(sample_time, delta_t, error);
        if (!std::isfinite(sample_value)) return false;
        if (sample_value < best_value) {
          best_value = sample_value;
          best_time = sample_time;
        }
      }
      double left = std::max(time - step, best_time - coarse_step);
      double right = std::min(time + step, best_time + coarse_step);
      const double golden = 0.6180339887498948482;
      double x1 = right - golden * (right - left);
      double x2 = left + golden * (right - left);
      double f1 = AxisDistance(x1, delta_t, error);
      double f2 = AxisDistance(x2, delta_t, error);
      if (!std::isfinite(f1) || !std::isfinite(f2)) return false;
      for (int iteration = 0; iteration < 70; ++iteration) {
        if (f1 > f2) {
          left = x1;
          x1 = x2;
          f1 = f2;
          x2 = left + golden * (right - left);
          f2 = AxisDistance(x2, delta_t, error);
        } else {
          right = x2;
          x2 = x1;
          f2 = f1;
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
                 &adjacent_state, &adjacent_orientation, error))
        return false;
      GeoPoint* output = direction < 0 ? &before : &after;
      if (!ShadowAxisPosition(adjacent_state, adjacent_orientation, ellipsoid_,
                              output))
        return false;
    }
    PathPoint point;
    point.tt_jd = time;
    point.central_line = footprint.axis;
    if (!SelectCrossTrackLimits(footprint, before, after, &point.northern_limit,
                                &point.southern_limit))
      return false;
    if (point.northern_limit.latitude_deg < point.southern_limit.latitude_deg)
      std::swap(point.northern_limit, point.southern_limit);
    point.width_km =
        SurfaceDistanceKm(point.northern_limit, point.southern_limit);
    LocalCircumstances local;
    if (!Local(time, event.delta_t_seconds, point.central_line, 0.0, &local,
               error))
      return false;
    point.magnitude = local.magnitude;
    point.total = local.total;
    path->push_back(point);
  }
  return true;
}

double EclipseEngine::ContactFunction(double tt_jd, double delta_t_seconds,
                                      const GeoPoint& observer,
                                      double height_metres, bool internal,
                                      LocalCircumstances* circumstances,
                                      std::string* error) const {
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error))
    return std::numeric_limits<double>::quiet_NaN();
  PhysicalConstants contact_constants = constants_;
  contact_constants.moon_radius_km =
      (internal ? 0.272281 : 0.272488) *
      contact_constants.earth_equatorial_radius_km;
  const LocalCircumstances local =
      EvaluateLocalCircumstances(state, orientation, observer, height_metres,
                                 ellipsoid_, contact_constants);
  if (circumstances) *circumstances = local;
  if (internal) {
    return local.separation_rad -
           std::fabs(local.moon_semidiameter_rad - local.sun_semidiameter_rad);
  }
  return local.separation_rad -
         (local.moon_semidiameter_rad + local.sun_semidiameter_rad);
}

double EclipseEngine::ContactFunctionRadius(double tt_jd,
                                            double delta_t_seconds,
                                            const GeoPoint& observer,
                                            double height_metres, bool internal,
                                            double moon_radius_km,
                                            std::string* error) const {
  SolarLunarState state;
  EarthOrientation orientation;
  if (!State(tt_jd, delta_t_seconds, &state, &orientation, error))
    return std::numeric_limits<double>::quiet_NaN();
  PhysicalConstants contact_constants = constants_;
  contact_constants.moon_radius_km = moon_radius_km;
  const LocalCircumstances local =
      EvaluateLocalCircumstances(state, orientation, observer, height_metres,
                                 ellipsoid_, contact_constants);
  return internal
             ? local.separation_rad - std::fabs(local.moon_semidiameter_rad -
                                                local.sun_semidiameter_rad)
             : local.separation_rad -
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
      const double right_value =
          ContactFunction(right, event.delta_t_seconds, observer, height_metres,
                          internal, NULL, error);
      if (!std::isfinite(right_value)) return false;
      if ((left_value > 0.0 && right_value <= 0.0) ||
          (left_value <= 0.0 && right_value > 0.0)) {
        double a = left;
        double b = right;
        double fa = left_value;
        for (int iteration = 0; iteration < 45; ++iteration) {
          const double middle = 0.5 * (a + b);
          const double fm =
              ContactFunction(middle, event.delta_t_seconds, observer,
                              height_metres, internal, NULL, error);
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
    if (!Local(tt_jd, event.delta_t_seconds, observer, height_metres, &local,
               error))
      return false;
    contact->valid = true;
    contact->tt_jd = tt_jd;
    contact->sun_altitude_deg = local.sun_altitude_deg;
    contact->sun_azimuth_deg = local.sun_azimuth_deg;
    return true;
  };
  if (!fill_contact(external_roots.front(), &contacts->c1) ||
      !fill_contact(external_roots.back(), &contacts->c4))
    return false;

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
             error))
    return false;
  for (int iteration = 0; iteration < 60; ++iteration) {
    if (local1.separation_rad > local2.separation_rad) {
      left = x1;
      x1 = x2;
      local1 = local2;
      x2 = left + golden * (right - left);
      if (!Local(x2, event.delta_t_seconds, observer, height_metres, &local2,
                 error))
        return false;
    } else {
      right = x2;
      x2 = x1;
      local2 = local1;
      x1 = right - golden * (right - left);
      if (!Local(x1, event.delta_t_seconds, observer, height_metres, &local1,
                 error))
        return false;
    }
  }
  const double maximum_time = 0.5 * (left + right);
  LocalCircumstances maximum;
  if (!Local(maximum_time, event.delta_t_seconds, observer, height_metres,
             &maximum, error) ||
      !fill_contact(maximum_time, &contacts->maximum))
    return false;
  contacts->magnitude = maximum.magnitude;
  contacts->obscuration = maximum.obscuration;
  contacts->type = maximum.total     ? kTotalEclipse
                   : maximum.annular ? kAnnularEclipse
                                     : kPartialEclipse;
  if (internal_roots.size() >= 2) {
    if (!fill_contact(internal_roots.front(), &contacts->c2) ||
        !fill_contact(internal_roots.back(), &contacts->c3))
      return false;
    contacts->central_duration_seconds =
        (contacts->c3.tt_jd - contacts->c2.tt_jd) * 86400.0;
  }
  return true;
}

bool EclipseEngine::RefineContactsWithLola(
    const EclipseEvent& event, const GeoPoint& observer, double height_metres,
    const PckKernel& lunar_orientation, const LunarLimbGrid& limb_grid,
    LocalContacts* contacts, std::string* error) const {
  if (!contacts) return false;
  if (!contacts->c1.valid &&
      !SolveLocalContacts(event, observer, height_metres, contacts, error))
    return false;
  ContactTime* contact_times[4] = {&contacts->c1, &contacts->c2, &contacts->c3,
                                   &contacts->c4};
  const bool internal_flags[4] = {false, true, true, false};
  for (int contact_index = 0; contact_index < 4; ++contact_index) {
    ContactTime* contact = contact_times[contact_index];
    if (!contact->valid) continue;
    SolarLunarState state;
    EarthOrientation orientation;
    if (!State(contact->tt_jd, event.delta_t_seconds, &state, &orientation,
               error))
      return false;
    const double tdb_jd =
        contact->tt_jd +
        TdbMinusTtSeconds(contact->tt_jd, orientation.ut1_jd) / 86400.0;
    const double et = (tdb_jd - 2451545.0) * 86400.0;
    const Vector3 observer_icrf =
        ObserverPositionIcrf(observer, height_metres, orientation, ellipsoid_,
                             constants_.earth_equatorial_radius_km);
    Vector3 view_body;
    Vector3 sun_body;
    if (!lunar_orientation.TransformIcrf(
            et, observer_icrf - state.moon_from_earth_km, &view_body, error) ||
        !lunar_orientation.TransformIcrf(
            et, state.sun_from_earth_km - state.moon_from_earth_km, &sun_body,
            error))
      return false;
    view_body = Normalize(view_body);
    const Vector3 limb_direction =
        Normalize(sun_body - view_body * Dot(sun_body, view_body));
    double radius_km = 0.0;
    if (!limb_grid.SupportRadiusKm(limb_direction, &radius_km, error))
      return false;

    const double scan_step = 2.0 / 86400.0;
    const double scan_start = contact->tt_jd - 180.0 / 86400.0;
    const double scan_end = contact->tt_jd + 180.0 / 86400.0;
    double previous_time = scan_start;
    double previous_value = ContactFunctionRadius(
        previous_time, event.delta_t_seconds, observer, height_metres,
        internal_flags[contact_index], radius_km, error);
    double best_a = 0.0, best_b = 0.0;
    double best_distance = std::numeric_limits<double>::infinity();
    for (double time = previous_time + scan_step; time <= scan_end;
         time += scan_step) {
      const double value = ContactFunctionRadius(
          time, event.delta_t_seconds, observer, height_metres,
          internal_flags[contact_index], radius_km, error);
      if (!std::isfinite(value)) return false;
      if ((previous_value <= 0.0 && value > 0.0) ||
          (previous_value > 0.0 && value <= 0.0)) {
        const double distance =
            std::fabs(0.5 * (previous_time + time) - contact->tt_jd);
        if (distance < best_distance) {
          best_distance = distance;
          best_a = previous_time;
          best_b = time;
        }
      }
      previous_time = time;
      previous_value = value;
    }
    if (!std::isfinite(best_distance)) {
      if (error) *error = "Unable to bracket LOLA-refined contact";
      return false;
    }
    double a = best_a;
    double b = best_b;
    double fa =
        ContactFunctionRadius(a, event.delta_t_seconds, observer, height_metres,
                              internal_flags[contact_index], radius_km, error);
    for (int iteration = 0; iteration < 45; ++iteration) {
      const double middle = 0.5 * (a + b);
      const double fm = ContactFunctionRadius(
          middle, event.delta_t_seconds, observer, height_metres,
          internal_flags[contact_index], radius_km, error);
      if ((fa <= 0.0 && fm > 0.0) || (fa > 0.0 && fm <= 0.0)) {
        b = middle;
      } else {
        a = middle;
        fa = fm;
      }
    }
    contact->tt_jd = 0.5 * (a + b);
    LocalCircumstances refined;
    if (!Local(contact->tt_jd, event.delta_t_seconds, observer, height_metres,
               &refined, error))
      return false;
    contact->sun_altitude_deg = refined.sun_altitude_deg;
    contact->sun_azimuth_deg = refined.sun_azimuth_deg;
  }
  if (contacts->c2.valid && contacts->c3.valid)
    contacts->central_duration_seconds =
        (contacts->c3.tt_jd - contacts->c2.tt_jd) * 86400.0;
  contacts->limb_adjusted = true;
  return true;
}

}  // namespace eclipse

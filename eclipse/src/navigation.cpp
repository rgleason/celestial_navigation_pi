#include "eclipse/navigation.h"
#include "eclipse/spk.h"

extern "C" {
#include "erfa.h"
}

#include <algorithm>
#include <cmath>

namespace eclipse {
namespace {
constexpr double kJ2000 = 2451545.0;
constexpr double kSecondsPerDay = 86400.0;
constexpr double kEarthEquatorialRadiusKm = 6378.137;
constexpr double kMoonMeanRadiusKm = 1737.4;
constexpr double kSunRadiusKm = 695700.0;
constexpr double kAuKm = 149597870.7;
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDegreesPerRadian = 180.0 / 3.1415926535897932384626433832795;

double Wrap360(double angle) {
  angle = std::fmod(angle, 360.0);
  return angle < 0.0 ? angle + 360.0 : angle;
}

bool Fail(const char* message, std::string* error) {
  if (error) *error = message;
  return false;
}

// The navigational Venus position refers to the illuminated centre of light.
// Carry forward the analytical engine's phase convention while changing only
// the underlying Sun/Earth/Venus ephemerides; compare it separately to USNO.
}  // namespace

bool VenusCentreOfLightPosition(const Vector3& venus, const Vector3& sun,
                                Vector3* light, std::string* error) {
  if (!light) return Fail("Missing Venus centre-of-light output", error);
  const double planet_range = venus.Norm();
  const double solar_range = sun.Norm();
  const Vector3 sun_to_planet = venus - sun;
  const double sun_planet_range = sun_to_planet.Norm();
  if (!(planet_range > 0.0) || !(solar_range > 0.0) ||
      !(sun_planet_range > 0.0))
    return Fail("Invalid Venus phase geometry", error);
  const Vector3 observer_to_planet_unit = venus / planet_range;
  const Vector3 sun_to_planet_unit = sun_to_planet / sun_planet_range;
  const Vector3 offset =
      observer_to_planet_unit * Dot(observer_to_planet_unit,
                                    sun_to_planet_unit) - sun_to_planet_unit;
  if (!(offset.Norm() > 0.0)) {
    *light = venus;
    return true;
  }
  const double illuminated = std::max(0.0, std::min(1.0,
      ((sun_planet_range + planet_range) *
       (sun_planet_range + planet_range) - solar_range * solar_range) /
      (4.0 * sun_planet_range * planet_range)));
  // 8.41" at 1 au is the value used by the existing analytical Venus model.
  const double angular_radius = 8.41 / 3600.0 / kDegreesPerRadian *
                                kAuKm / planet_range;
  const double coefficient =
      8.0 * angular_radius * (1.0 - illuminated) / (3.0 * kPi);
  *light = Normalize(observer_to_planet_unit +
                     Normalize(offset) * coefficient) * planet_range;
  return true;
}

bool ApparentNavigationPosition(const SpkKernel& kernel, std::int32_t target,
                                double et_seconds, Vector3* apparent,
                                std::string* error) {
  if (!apparent || !De440sNavigationTarget(target))
    return Fail("Unsupported navigation body centre", error);
  if (!ApparentGeocentricPosition(kernel, target, et_seconds, apparent, error))
    return false;
  if (target != 299) return true;
  Vector3 sun;
  if (!ApparentGeocentricPosition(kernel, 10, et_seconds, &sun, error))
    return false;
  return VenusCentreOfLightPosition(*apparent, sun, apparent, error);
}

bool De440sNavigationTarget(std::int32_t target) {
  return target == 10 || target == 301 || target == 199 || target == 299;
}

bool MakeNavigationEpoch(const CalendarDateTime& utc, double dut1_seconds,
                         double tai_minus_utc_seconds,
                         double polar_motion_x_rad,
                         double polar_motion_y_rad,
                         NavigationEpoch* epoch, std::string* error) {
  if (!epoch || !std::isfinite(dut1_seconds) ||
      !std::isfinite(tai_minus_utc_seconds) ||
      !std::isfinite(polar_motion_x_rad) ||
      !std::isfinite(polar_motion_y_rad))
    return Fail("Invalid navigation epoch input", error);
  double utc_jd = 0.0;
  if (!CalendarToJulianDate(utc, &utc_jd, error)) return false;
  NavigationEpoch value;
  value.orientation.tt_jd =
      utc_jd + (tai_minus_utc_seconds + 32.184) / kSecondsPerDay;
  value.orientation.ut1_jd = utc_jd + dut1_seconds / kSecondsPerDay;
  value.orientation.polar_motion_x_rad = polar_motion_x_rad;
  value.orientation.polar_motion_y_rad = polar_motion_y_rad;
  const double tdb_minus_tt =
      TdbMinusTtSeconds(value.orientation.tt_jd, value.orientation.ut1_jd);
  value.et_seconds =
      (value.orientation.tt_jd - kJ2000) * kSecondsPerDay + tdb_minus_tt;
  if (!std::isfinite(value.et_seconds))
    return Fail("Invalid navigation dynamical time", error);
  *epoch = value;
  return true;
}

bool GeocentricNavigationState(const SpkKernel& kernel, std::int32_t target,
                               const NavigationEpoch& epoch,
                               NavigationGeocentricState* state,
                               std::string* error) {
  if (!state) return Fail("Missing navigation state output", error);
  if (!De440sNavigationTarget(target))
    return Fail("DE440s does not provide this body centre", error);
  if (!std::isfinite(epoch.et_seconds) ||
      !std::isfinite(epoch.orientation.tt_jd) ||
      !std::isfinite(epoch.orientation.ut1_jd))
    return Fail("Invalid navigation epoch", error);

  Vector3 apparent;
  if (!ApparentGeocentricPosition(kernel, target, epoch.et_seconds, &apparent,
                                 error))
    return false;
  const double range = apparent.Norm();
  if (!(range > kEarthEquatorialRadiusKm) || !std::isfinite(range))
    return Fail("Invalid apparent body range", error);
  const Vector3 centre_fixed = IcrfToEarthFixed(apparent, epoch.orientation);
  NavigationGeocentricState value;
  value.target = target;
  value.distance_km = range;
  value.centre_gha_deg =
      Wrap360(-std::atan2(centre_fixed.y, centre_fixed.x) * kDegreesPerRadian);
  value.centre_declination_deg =
      std::atan2(centre_fixed.z, std::hypot(centre_fixed.x, centre_fixed.y)) *
      kDegreesPerRadian;
  if (target == 299) {
    Vector3 light;
    Vector3 sun;
    if (!ApparentGeocentricPosition(kernel, 10, epoch.et_seconds, &sun, error) ||
        !VenusCentreOfLightPosition(apparent, sun, &light, error))
      return false;
    apparent = light;
    value.phase_corrected = true;
  }
  const Vector3 fixed = IcrfToEarthFixed(apparent, epoch.orientation);
  value.gha_deg = Wrap360(-std::atan2(fixed.y, fixed.x) * kDegreesPerRadian);
  value.declination_deg =
      std::atan2(fixed.z, std::hypot(fixed.x, fixed.y)) * kDegreesPerRadian;
  value.gha_aries_deg = Wrap360(
      eraGst06a(kJ2000, epoch.orientation.ut1_jd - kJ2000,
                kJ2000, epoch.orientation.tt_jd - kJ2000) *
      kDegreesPerRadian);
  value.horizontal_parallax_deg =
      std::asin(std::min(1.0, kEarthEquatorialRadiusKm / range)) *
      kDegreesPerRadian;
  const double body_radius =
      target == 301 ? kMoonMeanRadiusKm :
      target == 10 ? kSunRadiusKm : 0.0;
  if (body_radius != 0.0)
    value.semidiameter_deg =
        std::asin(std::min(1.0, body_radius / range)) * kDegreesPerRadian;
  *state = value;
  return true;
}

}  // namespace eclipse

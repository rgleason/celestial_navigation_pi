#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
extern "C" {
#include "erfa.h"
}
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace eclipse {
namespace {
constexpr double pi = 3.1415926535897932384626433832795;
double degrees(double radians) { return radians*180.0/pi; }
void require(bool ok, const std::string& error) {
  if (!ok) throw std::runtime_error(error);
}
}
bool ObserverApparentDirection(const SpkKernel& kernel, double et,
    const EarthOrientation& orientation, double lat_deg, double lon_deg,
    double height, bool moon, double* alt, double* az, double* sd,
    std::string* error) {
  try {
    require(alt && az && sd, "Missing observer direction output");
    require(std::isfinite(et) && std::isfinite(orientation.tt_jd) &&
            std::isfinite(orientation.ut1_jd) && std::isfinite(lat_deg) &&
            std::abs(lat_deg)<=90 && std::isfinite(lon_deg) &&
            std::abs(lon_deg)<=180 && std::isfinite(height) && height>-6370000,
            "Invalid observer or epoch");

    // Recompute for each trial station. Parallax and observer-specific light
    // time precede aberration; subtracting a station from an already aberrated
    // geocentric vector is not equivalent for a nearby body such as the Moon.
    const double lat = lat_deg*pi/180, lon = lon_deg*pi/180;
    double rc2i[3][3], pv[2][3], station[3], velocity[3];
    eraC2i06a(2451545.0, orientation.tt_jd-2451545.0, rc2i);
    eraPvtob(lon, lat, height, orientation.polar_motion_x_rad, orientation.polar_motion_y_rad,
             eraSp00(2451545.0, orientation.tt_jd-2451545.0),
             eraEra00(2451545.0, orientation.ut1_jd-2451545.0), pv);
    eraTrxp(rc2i, pv[0], station);
    eraTrxp(rc2i, pv[1], velocity);
    eclipse::Vector3 earth, before, after, sun, target;
    std::string error;
    require(kernel.Position(399, 0, et, &earth, &error), error);
    require(kernel.Position(399, 0, et-30, &before, &error), error);
    require(kernel.Position(399, 0, et+30, &after, &error), error);
    require(kernel.Position(10, 0, et, &sun, &error), error);
    const auto observer = earth + eclipse::Vector3(station[0],station[1],station[2]) * 0.001;
    constexpr double c = 299792.458;
    const auto beta = ((after-before)*(1.0/60) +
                      eclipse::Vector3(velocity[0],velocity[1],velocity[2])*0.001)*(1.0/c);
    double transmission = et;
    eclipse::Vector3 relative;
    bool converged = false;
    for (int i=0; i<12; ++i) {
      require(kernel.Position(moon ? 301 : 10, 0, transmission, &target, &error), error);
      relative = target-observer;
      const double next = et-relative.Norm()/c;
      if (std::abs(next-transmission) < 1e-7) { converged = true; break; }
      transmission = next;
    }
    require(converged, "Observer light-time iteration did not converge");
    const double range = relative.Norm();
    require(range > 0, "Invalid observer range");
    double natural[] = {relative.x/range,relative.y/range,relative.z/range};
    double v[] = {beta.x,beta.y,beta.z}, apparent[3];
    eraAb(natural, v, (sun-observer).Norm()/149597870.7,
          std::sqrt(1-eclipse::Dot(beta,beta)), apparent);
    const auto terrestrial = eclipse::IcrfToEarthFixed(
        eclipse::Vector3(apparent[0],apparent[1],apparent[2]), orientation);
    const eclipse::Vector3 up(std::cos(lat)*std::cos(lon), std::cos(lat)*std::sin(lon), std::sin(lat));
    const eclipse::Vector3 east(-std::sin(lon), std::cos(lon), 0);
    const eclipse::Vector3 north(-std::sin(lat)*std::cos(lon), -std::sin(lat)*std::sin(lon), std::cos(lat));
    *alt = degrees(std::asin(std::max(-1.0,std::min(1.0,eclipse::Dot(terrestrial,up)))));
    *az = degrees(std::atan2(eclipse::Dot(terrestrial,east),eclipse::Dot(terrestrial,north)));
    // Geometric apparent angular radius from retarded observer range, matching
    // the existing spherical-disc convention (not a resolved limb ray trace).
    *sd = degrees(std::asin((moon ? 1737.4 : 695700.0)/range));

    return true;
  } catch (const std::exception& e) {
    if (error) *error=e.what();
    return false;
  }
}
}

#ifndef CELESTIAL_ECLIPSE_ASTRONOMY_H
#define CELESTIAL_ECLIPSE_ASTRONOMY_H

#include "eclipse/besselian.h"
#include "eclipse/vector.h"

#include <cstdint>
#include <string>

namespace eclipse {

struct EarthOrientation {
  double tt_jd;
  double ut1_jd;
  double polar_motion_x_rad;
  double polar_motion_y_rad;

  EarthOrientation()
      : tt_jd(0.0), ut1_jd(0.0), polar_motion_x_rad(0.0),
        polar_motion_y_rad(0.0) {}
};

struct SolarLunarState {
  Vector3 sun_from_earth_km;
  Vector3 moon_from_earth_km;
};

class SpkKernel;

// Astrometric target vector at reception time: target at iterated light-time
// epoch minus observer at reception epoch. Stellar aberration and deflection
// are deliberately separate concerns.
bool AstrometricPosition(const SpkKernel& kernel, std::int32_t target,
                         std::int32_t observer, double reception_et,
                         Vector3* position_km, std::string* error);

Vector3 IcrfToEarthFixed(const Vector3& icrf,
                         const EarthOrientation& orientation);

bool ShadowAxisPosition(const SolarLunarState& state,
                        const EarthOrientation& orientation,
                        const ReferenceEllipsoid& ellipsoid,
                        GeoPoint* position);

}  // namespace eclipse

#endif

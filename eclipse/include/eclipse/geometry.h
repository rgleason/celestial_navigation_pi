#ifndef CELESTIAL_ECLIPSE_GEOMETRY_H
#define CELESTIAL_ECLIPSE_GEOMETRY_H

#include "eclipse/astronomy.h"

#include <vector>

namespace eclipse {

struct PhysicalConstants {
  double earth_equatorial_radius_km;
  double sun_radius_km;
  double moon_radius_km;

  PhysicalConstants()
      : earth_equatorial_radius_km(6378.137),
        sun_radius_km(696000.0),
        // Conventional eclipse umbral radius k2=0.272281 Earth radii. A
        // separate k1/terrain profile is used for exterior contacts later.
        moon_radius_km(0.272281 * 6378.137) {}
};

struct ShadowFootprint {
  bool central;
  bool total;
  GeoPoint axis;
  std::vector<GeoPoint> boundary;

  ShadowFootprint() : central(false), total(false) {}
};

// Intersects the umbral/antumbral cone with WGS 84. Boundary points are in
// azimuth order around the cone and may cross the anti-meridian.
ShadowFootprint CentralShadowFootprint(
    const SolarLunarState& state, const EarthOrientation& orientation,
    const ReferenceEllipsoid& ellipsoid, const PhysicalConstants& constants,
    int angular_samples);

bool SelectCrossTrackLimits(const ShadowFootprint& footprint,
                            const GeoPoint& axis_before,
                            const GeoPoint& axis_after,
                            GeoPoint* left_limit, GeoPoint* right_limit);

double SurfaceDistanceKm(const GeoPoint& first, const GeoPoint& second);

struct LocalCircumstances {
  double separation_rad;
  double sun_semidiameter_rad;
  double moon_semidiameter_rad;
  double magnitude;
  double obscuration;
  double sun_altitude_deg;
  double sun_azimuth_deg;
  bool partial;
  bool total;
  bool annular;

  LocalCircumstances()
      : separation_rad(0.0), sun_semidiameter_rad(0.0),
        moon_semidiameter_rad(0.0), magnitude(0.0), obscuration(0.0),
        sun_altitude_deg(0.0), sun_azimuth_deg(0.0), partial(false),
        total(false), annular(false) {}
};

LocalCircumstances EvaluateLocalCircumstances(
    const SolarLunarState& state, const EarthOrientation& orientation,
    const GeoPoint& observer, double height_metres,
    const ReferenceEllipsoid& ellipsoid, const PhysicalConstants& constants);

}  // namespace eclipse

#endif

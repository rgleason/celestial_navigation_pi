#ifndef CELESTIAL_ECLIPSE_NAVIGATION_H
#define CELESTIAL_ECLIPSE_NAVIGATION_H

#include "eclipse/astronomy.h"
#include "eclipse/time.h"

#include <cstdint>
#include <string>

namespace eclipse {

// The DE440s kernel has body centres for these four navigational targets.
// A planetary-system barycentre must never be substituted for a planet centre.
bool De440sNavigationTarget(std::int32_t target);

// Shift a Venus apparent vector from centre of figure to the illuminated
// centre used for navigational sights. Both input vectors share an observer
// origin, so the same convention works geocentrically and topocentrically.
bool VenusCentreOfLightPosition(const Vector3& venus, const Vector3& sun,
                                Vector3* light, std::string* error);

bool ApparentNavigationPosition(const SpkKernel& kernel, std::int32_t target,
                                double et_seconds, Vector3* apparent,
                                std::string* error);

struct NavigationEpoch {
  double et_seconds = 0.0;  // TDB seconds from J2000, for SPK interpolation
  EarthOrientation orientation;
};

// The caller supplies Earth rotation and clock data explicitly. This avoids
// silently treating an estimated DUT1 or future leap-second offset as measured.
bool MakeNavigationEpoch(const CalendarDateTime& utc, double dut1_seconds,
                         double tai_minus_utc_seconds,
                         double polar_motion_x_rad,
                         double polar_motion_y_rad,
                         NavigationEpoch* epoch, std::string* error);

struct NavigationGeocentricState {
  std::int32_t target = 0;
  double gha_deg = 0.0;
  double declination_deg = 0.0;
  double centre_gha_deg = 0.0;
  double centre_declination_deg = 0.0;
  bool phase_corrected = false;
  double gha_aries_deg = 0.0;
  double distance_km = 0.0;
  double horizontal_parallax_deg = 0.0;
  // Geocentric mean spherical-disc semidiameter, not a resolved lunar limb.
  // The observational upper/lower-limb correction must instead use the
  // observer-specific range and the selected limb, exactly once.
  double semidiameter_deg = 0.0;  // Sun/Moon only; planets use centre of light
};

// Apparent, airless geocentric navigational position, expressed in the
// terrestrial frame. Venus is shifted from centre of figure to centre of light
// using the same phase model as the existing analytical engine. The unshifted
// centre is returned separately. Refraction and limb corrections are excluded.
// DE440s gives the Moon's centre of mass, not its figure, terrain or local
// horizon profile; no optional eclipse topography pack is needed here.
bool GeocentricNavigationState(const SpkKernel& kernel, std::int32_t target,
                               const NavigationEpoch& epoch,
                               NavigationGeocentricState* state,
                               std::string* error);

}  // namespace eclipse

#endif

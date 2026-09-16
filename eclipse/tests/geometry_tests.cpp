#include "eclipse/geometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

// This translation unit extends the dependency-free test binary through a
// static initializer. It intentionally contains only invariant geometry tests;
// DE440-backed checks remain in besselian_tests.cpp where failures are counted.
namespace {

struct GeometryInvariantTests {
  GeometryInvariantTests() {
    const eclipse::GeoPoint observer(0.0, 0.0);
    eclipse::SolarLunarState state;
    state.sun_from_earth_km = eclipse::Vector3(150000000.0, 0.0, 0.0);
    state.moon_from_earth_km = eclipse::Vector3(360000.0, 0.0, 0.0);
    eclipse::EarthOrientation orientation;
    orientation.tt_jd = 2451545.0;
    orientation.ut1_jd = 2451545.0;
    const eclipse::LocalCircumstances local =
        eclipse::EvaluateLocalCircumstances(state, orientation, observer, 0.0,
                                            eclipse::ReferenceEllipsoid(),
                                            eclipse::PhysicalConstants());
    if (!std::isfinite(local.magnitude) || !std::isfinite(local.obscuration)) {
      std::cerr << "FAIL geometry invariant returned non-finite result\n";
      std::exit(EXIT_FAILURE);
    }
  }
};

GeometryInvariantTests geometry_invariant_tests;

}  // namespace

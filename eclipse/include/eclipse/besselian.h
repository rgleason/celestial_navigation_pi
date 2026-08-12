#ifndef CELESTIAL_ECLIPSE_BESSELIAN_H
#define CELESTIAL_ECLIPSE_BESSELIAN_H

#include "eclipse/types.h"

namespace eclipse {

struct Polynomial3 {
  double c0;
  double c1;
  double c2;
  double c3;

  Polynomial3() : c0(0.0), c1(0.0), c2(0.0), c3(0.0) {}
  Polynomial3(double a, double b, double c, double d)
      : c0(a), c1(b), c2(c), c3(d) {}

  double Evaluate(double hours) const;
  double Derivative(double hours) const;
};

struct BesselianElements {
  double reference_tt_jd;
  double delta_t_seconds;
  Polynomial3 x;
  Polynomial3 y;
  Polynomial3 declination_deg;
  Polynomial3 penumbral_radius;
  Polynomial3 umbral_radius;
  Polynomial3 hour_angle_deg;
  double tan_f1;
  double tan_f2;

  BesselianElements();
};

struct EvaluatedElements {
  double x;
  double y;
  double declination_rad;
  double penumbral_radius;
  double umbral_radius;
  double hour_angle_rad;
  double tan_f1;
  double tan_f2;
};

// WGS 84, expressed in units of its equatorial radius.
struct ReferenceEllipsoid {
  double flattening;

  ReferenceEllipsoid() : flattening(1.0 / 298.257223563) {}
  explicit ReferenceEllipsoid(double value) : flattening(value) {}

  double polar_ratio() const { return 1.0 - flattening; }
};

EvaluatedElements Evaluate(const BesselianElements& elements,
                           double terrestrial_time_jd);

// Intersects the lunar shadow axis with the reference ellipsoid. The returned
// point is the geodetic latitude/longitude on the illuminated side of Earth.
// Returns false if the shadow axis misses Earth.
bool CentralLinePosition(const EvaluatedElements& elements,
                         const ReferenceEllipsoid& ellipsoid,
                         GeoPoint* position);

// Published smooth-limb reference for the total solar eclipse of 2027-08-02.
// Source: NASA/GSFC, Fred Espenak. This fixture is for independent validation;
// production eclipse discovery will derive elements from the installed DE440
// data pack.
BesselianElements Nasa2027Aug02Reference();

}  // namespace eclipse

#endif


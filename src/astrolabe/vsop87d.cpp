/* Copyright 2000, 2001 William McClain

    This file is part of Astrolabe.

    Astrolabe is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Astrolabe is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Astrolabe; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
    */

/* The VSOP87d planetary position model */

#include "astrolabe.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>

using std::cout;
using std::endl;
using std::getline;
using std::ifstream;
using std::list;
using std::map;
using std::string;
using std::vector;

using astrolabe::Coords;
using astrolabe::vPlanets;
using astrolabe::calendar::jd_to_jcent;
using astrolabe::constants::pi;
using astrolabe::constants::pi2;
using astrolabe::dicts::stringToCoord;
using astrolabe::dicts::stringToPlanet;
using astrolabe::util::d_to_r;
using astrolabe::util::diff_angle;
using astrolabe::util::dms_to_d;
using astrolabe::util::ecl_to_equ;
using astrolabe::util::modpi2;
using astrolabe::util::polynomial;
using astrolabe::util::split;
using astrolabe::util::string_to_double;
using astrolabe::util::string_to_int;

/*
# Local dictionary of planetary terms.
#
# The key is a tuple (planet_name, coordinate_name)
#
# The value of each entry is a list of lists.
*/

class Key {
public:
  Key(vPlanets planet, Coords dim) : planet(planet), dim(dim){};
  bool operator<(const Key& rhs) const {
    if (planet < rhs.planet) return true;
    if (planet > rhs.planet) return false;
    return (dim < rhs.dim);
  };

private:
  vPlanets planet;
  Coords dim;
};

class Terms {
public:
  Terms(double A, double B, double C) : A(A), B(B), C(C){};

  const double A, B, C;
};

class Series {
public:
  Series(const list<Terms>& terms) : terms(terms){};

  const list<Terms> terms;
};

namespace {
map<Key, list<Series> > _planets;
bool _first_time = true;
};  // namespace

astrolabe::vsop87d::VSOP87d::VSOP87d() {
  /* Load the database of planetary terms. This is actually done
  only once to save time and space.

  */
  if (!_first_time) return;
  //    cout << "loading text db..." << endl;
  load_vsop87d_text_db();
  _first_time = false;
}

double astrolabe::vsop87d::VSOP87d::dimension(double jd, vPlanets planet,
                                              Coords dim) const {
  /* Return one of heliocentric ecliptic longitude, latitude and radius.

  [Meeus-1998: pg 218]

  Parameters:
      jd : Julian Day in dynamical time
      planet : must be one of ("Mercury", "Venus", "Earth", "Mars",
          "Jupiter", "Saturn", "Uranus", "Neptune")
      dim : must be one of "L" (longitude) or "B" (latitude) or "R" (radius)

  Returns:
      longitude in radians, or
      latitude in radians, or
      radius in au

  */
  double X = 0.0;
  double tauN = 1.0;
  const double tau = jd_to_jcent(jd) / 10.0;
  const list<Series>& series = _planets[Key(planet, dim)];

  for (std::list<Series>::const_iterator p = series.begin(); p != series.end();
       ++p) {
    double seriesSum = 0.0;
    const list<Terms>& terms = p->terms;
    for (std::list<Terms>::const_iterator q = terms.begin(); q != terms.end();
         ++q)
      seriesSum += q->A * cos(q->B + q->C * tau);
    X += seriesSum * tauN;
    tauN *= tau;  // last one is wasted
  }

  if (dim == vL) X = modpi2(X);

  return X;
}

void astrolabe::vsop87d::VSOP87d::dimension3(double jd, vPlanets planet,
                                             double& longitude,
                                             double& latitude,
                                             double& radius) const {
  /* Return heliocentric ecliptic longitude, latitude and radius.

  Parameters:
      jd : Julian Day in dynamical time
      planet : must be one of ("Mercury", "Venus", "Earth", "Mars",
          "Jupiter", "Saturn", "Uranus", "Neptune")

  Returns:
      longitude in radians
      latitude in radians
      radius in au

  */
  longitude = dimension(jd, planet, vL);
  latitude = dimension(jd, planet, vB);
  radius = dimension(jd, planet, vR);
}

void astrolabe::vsop87d::vsop_to_fk5(double jd, double& L, double& B) {
  /* Convert VSOP to FK5 coordinates.

  This is required only when using the full precision of the
  VSOP model.

  [Meeus-1998: pg 219]

  Parameters:
      jd : Julian Day in dynamical time
      L : longitude in radians
      B : latitude in radians

  Returns:
      corrected longitude in radians
      corrected latitude in radians

  */
  //
  // Constant terms
  //
  static const double _k0 = d_to_r(-1.397);
  static const double _k1 = d_to_r(-0.00031);
  static const double _k2 = d_to_r(dms_to_d(0, 0, -0.09033));
  static const double _k3 = d_to_r(dms_to_d(0, 0, 0.03916));

  const double T = jd_to_jcent(jd);
  vector<double> poly;
  poly.push_back(L);
  poly.push_back(_k0);
  poly.push_back(_k1);
  const double L1 = polynomial(poly, T);
  const double cosL1 = cos(L1);
  const double sinL1 = sin(L1);
  const double deltaL = _k2 + _k3 * (cosL1 + sinL1) * tan(B);
  const double deltaB = _k3 * (cosL1 - sinL1);
  L = modpi2(L + deltaL);
  B += deltaB;
}

void astrolabe::vsop87d::apply_phase_correction(double& p_l, double& p_b,
                                                double p_r, double s_l,
                                                double s_b, double s_r) {
  /* Apply planet phase correction

  Results will be geocentric, equatorial coordinates of the planet
  corrected for phase.

  Thanks to Robert Bernecky:
  https://navlist.net/imgx/PHASE-CORRECTION-FOR-VENUS.pdf

  Parameters:
      p_l: planet Right Ascension in radians
      p_b: planet Dec in radians
      p_r: planet Radius in au
      s_l: sun Right Ascension in radians
      s_b: sun Dec in radians
      s_r: sun Radius in au

  Returns:
      p_l, p_b phase corrected
  */
  VSOP87d vsop;

  // Earth carthesian coordinates
  double e_xyz[3] = {0, 0, 0};

  // Sun carthesian coordinates
  double s_xyz[3];
  s_xyz[0] = s_r * cos(s_b) * cos(s_l);
  s_xyz[1] = s_r * cos(s_b) * sin(s_l);
  s_xyz[2] = s_r * sin(s_b);

  // Planet carthesian coordinates
  double p_xyz[3];
  p_xyz[0] = p_r * cos(p_b) * cos(p_l);
  p_xyz[1] = p_r * cos(p_b) * sin(p_l);
  p_xyz[2] = p_r * sin(p_b);

  // Sun->Planet vector
  double sp_xyz[3];
  sp_xyz[0] = p_xyz[0] - s_xyz[0];
  sp_xyz[1] = p_xyz[1] - s_xyz[1];
  sp_xyz[2] = p_xyz[2] - s_xyz[2];
  double sp_d = sqrt(sp_xyz[0] * sp_xyz[0] + sp_xyz[1] * sp_xyz[1] +
                     sp_xyz[2] * sp_xyz[2]);

  // Sun->Planet unit vector
  double usp_xyz[3];
  usp_xyz[0] = sp_xyz[0] / sp_d;
  usp_xyz[1] = sp_xyz[1] / sp_d;
  usp_xyz[2] = sp_xyz[2] / sp_d;

  // Earth->Planet unit vector
  double uep_xyz[3];
  uep_xyz[0] = p_xyz[0] / p_r;
  uep_xyz[1] = p_xyz[1] / p_r;
  uep_xyz[2] = p_xyz[2] / p_r;

  // compute uep . usp
  double uep_dot_usp;
  uep_dot_usp = usp_xyz[0] * uep_xyz[0] + usp_xyz[1] * uep_xyz[1] +
                usp_xyz[2] * uep_xyz[2];

  // compute (uep . usp) * uep
  double c[3];
  c[0] = uep_xyz[0] * uep_dot_usp;
  c[1] = uep_xyz[1] * uep_dot_usp;
  c[2] = uep_xyz[2] * uep_dot_usp;

  // compute (r . p)p - r
  c[0] -= usp_xyz[0];
  c[1] -= usp_xyz[1];
  c[2] -= usp_xyz[2];

  // normalize
  double csize = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
  c[0] /= csize;
  c[1] /= csize;
  c[2] /= csize;

  // compute K
  double k = (sp_d + p_r) * (sp_d + p_r) - s_r * s_r;
  k = k / (4 * sp_d * p_r);

  // compute s
  double s = (8.41 * pi) / (p_r * 180 * 3600);

  // compute coef
  double coef = (8 * s) * (1 - k) / (3 * pi);

  // final c compute
  c[0] *= coef;
  c[1] *= coef;
  c[2] *= coef;

  // apply correction
  p_xyz[0] = (uep_xyz[0] + c[0]) * p_r;
  p_xyz[1] = (uep_xyz[1] + c[1]) * p_r;
  p_xyz[2] = (uep_xyz[2] + c[2]) * p_r;

  p_l = atan2(p_xyz[1], p_xyz[0]);
  if (p_l < 0) p_l += 2 * pi;
  p_b = atan2(p_xyz[2], sqrt(p_xyz[0] * p_xyz[0] + p_xyz[1] * p_xyz[1]));
  p_r = sqrt(p_xyz[0] * p_xyz[0] + p_xyz[1] * p_xyz[1] + p_xyz[2] * p_xyz[2]);
}

void astrolabe::vsop87d::geocentric_planet(double jd, vPlanets planet,
                                           double deltaPsi, double epsilon,
                                           double delta, double& ra,
                                           double& dec, double& dist) {
  /* Calculate the equatorial coordinates of a planet

  The results will be geocentric, corrected for light-time and
  aberration.

  Parameters:
      jd : Julian Day in dynamical time
      planet : must be one of ("Mercury", "Venus", "Earth", "Mars",
          "Jupiter", "Saturn", "Uranus", "Neptune")
      deltaPsi : nutation in longitude, in radians
      epsilon : true obliquity (corrected for nutation), in radians
      delta : desired accuracy, in days

  Returns:
      right accension, in radians
      declination, in radians

  */
  VSOP87d vsop;
  double t = jd;
  double l0 = -100.0;  // impossible value

  // We need to iterate to correct for light-time and aberration.
  // At most three passes through the loop always nails it.
  // Note that we move both the Earth and the other planet during
  //    the iteration.
  double p_l, p_b, p_r;
  double s_l, s_b, s_r;
  bool ok = false;
  for (int bailout = 0; bailout < 20; bailout++) {
    // heliocentric geometric ecliptic coordinates of the Earth
    double L0, B0, R0;
    vsop.dimension3(t, vEarth, L0, B0, R0);

    // heliocentric geometric ecliptic coordinates of the planet
    double L, B, R;
    vsop.dimension3(t, planet, L, B, R);

    // rectangular offset
    const double cosB0 = cos(B0);
    const double cosB = cos(B);
    const double x = R * cosB * cos(L) - R0 * cosB0 * cos(L0);
    const double y = R * cosB * sin(L) - R0 * cosB0 * sin(L0);
    const double z = R * sin(B) - R0 * sin(B0);

    // geocentric geometric ecliptic coordinates of the planet
    const double x2 = x * x;
    const double y2 = y * y;
    p_l = atan2(y, x);
    p_b = atan2(z, sqrt(x2 + y2));
    p_r = sqrt(x2 + y2 + z * z);

    // geocentric geometric ecliptic coordinates of the sun
    s_l = L0 + pi;
    if (s_l > 2 * pi) s_l -= 2 * pi;
    s_b = -B0;
    s_r = R0;

    // light time in days
    const double tau = 0.0057755183 * p_r;

    if (fabs(diff_angle(p_l, l0)) < pi2 * delta) {
      ok = true;
      break;
    }

    // adjust for light travel time and try again
    l0 = p_l;
    t = jd - tau;
  }

  if (!ok) throw Error("astrolabe::vsop87d::geocentric_planet: bailout");

  // apply phase correction to Venus
  if (planet == vVenus) apply_phase_correction(p_l, p_b, p_r, s_l, s_b, s_r);

  // transform to FK5 ecliptic and equinox
  vsop_to_fk5(jd, p_l, p_b);

  // nutation in longitude
  p_l += deltaPsi;

  // equatorial coordinates
  ecl_to_equ(p_l, p_b, epsilon, ra, dec);

  // AU to km
  dist = p_r * 149597870.691;
}

void astrolabe::vsop87d::load_vsop87d_text_db() {
  /* Load the text version of the VSOP87d database into memory.

  IMPORTANT: normally you don't call this routine directly.
  That is done automatically by the __init__() method of the VSOP87d
  class.

  */

  //
  // Read the data file and load the dictionary.
  //
  ifstream infile(astrolabe::globals::vsop87d_text_path.c_str());
  if (!infile)
    throw Error(
        "astrolabe::vsop87d::load_vsop87d_text_db: unable to open VSOP87d text "
        "file");
  string line;
  getline(infile, line);
  while (infile) {
    const vector<string> fields = split(line);
    const string planet = fields[0];
    const string dim = fields[1];
    // field 2, term index, not used
    const int nt = string_to_int(fields[3]);
    list<Terms> t;
    for (int i = 0; i < nt; i++) {
      getline(infile, line);
      const vector<string> fields = split(line);
      const double A = string_to_double(fields[0]);
      const double B = string_to_double(fields[1]);
      const double C = string_to_double(fields[2]);
      t.push_back(Terms(A, B, C));
    }
    _planets[Key(stringToPlanet[planet], stringToCoord[dim])].push_back(t);
    getline(infile, line);
  }
  infile.close();
}

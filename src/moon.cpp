//
//  main.cpp
//  Moon SemiDiameter (arc min) and Moon Parallax (min)
//    for OpenCPN Celestial Plug-in
//  Created by Robert Bossert on 2/9/25.

#include <wx/wx.h>
#include <iostream>
#include <cmath>
#include <iomanip>
#include "ocpn_plugin.h"
#include "Sight.h"
#include "astrolabe/astrolabe.hpp"

using namespace astrolabe::util;

double modulo_360(double big_degrees) {
    //
    //   modulo_360
    //   Convert large Degree numbers (+ or - ) to 0 - 360 degrees.
    //   Calculate number of complete 360 degree rotations.
    //   Subtract or Add the rotations to BIG.  Result is the remaining degrees
    //   If remaining Degrees is between 0 and -360 rotations were = 0, add 360
    //   BIG shouldn't be bigger than 2.13 billion degrees.
    //
    int z = 0;
    double adj_degrees = big_degrees ;

    z = abs(trunc(big_degrees/360));

    if (big_degrees >= 360) {
        adj_degrees = big_degrees - z * 360;
    }

    if (big_degrees < 0) {
        adj_degrees = big_degrees + z * 360;

    }
    if (big_degrees < 0) {
        adj_degrees = adj_degrees + 360;

    }
    //std::cout << "input, rounds, adjdeg: "<<big_degrees<<", " <<z <<", "<< adj_degrees << std::endl;
    return adj_degrees ;
}

double moon_distance(double JD) {
    // JD Julian Date
    // JDE Julian Ephemerides Date.  jde = jd found to be good enough in testing.
    // TT Terrestial Julian Time in centuries starting from Jan 1 2000 Epoch
    //
    // For unit testing, used https://aa.usno.navy.mil/data/JulianDate.
    //      input UTC1 and the site returns Julian Date.
    //
    double JDE = JD ;
    const double J2000 = 2451545.0 ;       // Julian date 2451545.0 TT=2000 Jan 1, 12h TT
    const double DAYS_IN_CENTURY = 36525 ; // numnber of days in century.
    double TT = (JDE - J2000)/DAYS_IN_CENTURY ;   // Terestrial Time

    //
    // Calculates Geocentric Moon Distance from Earth in km at Julian Time TT.
    // TT should be carried to at least 9 significant digits.
    //
    double BASE_MOON_DISTANCE = 385000.56;       // Base Earth-Moon distance 385000.56 km
    double m_distance = BASE_MOON_DISTANCE ;     //  Avg Earth-Moon distance 384400 km
    double ml_prime, md, mm, mm_prime, mf, a1, a2, a3, e ;

    //variables and equations Defined in J. Meeus Celestial Equations chapter 45....
    // ml_prime Moon's Mean Longitude (degrees)
    // md Mean elongation of the moon (degrees)
    // mm Sun's mean anomaly (degrees)
    // mm_prime Moon's mean anomaly (degrees)
    // mf Moon's argument of latitude or
    //    Mean distance moon from it's ascending node (degrees)
    // a1 due to action of Venus (degrees)
    // a2 due to action of Jupiter (degrees)
    // e Eccentricity of earth's orbit around sun.  (ratio)
    //

    ml_prime = 218.3164591 + (481267.8813436*TT) - (.0013268*TT*TT)
      + (TT*TT*TT/538841) - (TT*TT*TT*TT/65194000) ;

    md = 297.8502042 + (445267.1115168*TT) - (.00163*TT*TT)
    + (TT*TT*TT/545868) - (TT*TT*TT*TT/113065000) ;

    mm = 357.5291092 + (35999.0502909*TT) - (.0001536*TT*TT)
    + (TT*TT*TT/24490000) ;

    mm_prime = 134.9634114 + (477198.8676313*TT) + (.008997*TT*TT)
    + (TT*TT*TT/69699) - (TT*TT*TT*TT/14712000) ;

    mf = 93.2720993 + (483202.0175273*TT) - (.0034029*TT*TT)
    + (TT*TT*TT/3526000) + (TT*TT*TT*TT/863310000) ;

    a1 = 119.75 + (131.849*TT) ;
    a2 = 53.09+(479264.29*TT) ;
    a3 = 313.45 + (481266.484*TT) ;
    e = (1 - (0.002516*TT) - (0.0000074*TT*TT) ) ;   // orbit ecentricity.

    ml_prime = modulo_360(ml_prime);
    md      = modulo_360(md);
    mm      = modulo_360(mm);
    mm_prime = modulo_360(mm_prime);
    mf      = modulo_360(mf);
    a1      = modulo_360(a1);
    a2      = modulo_360(a2);
    a3      = modulo_360(a3);

    //std::cout << "JD:       " << std::setprecision(15) << std::fixed << JD << std::endl;
    //std::cout << "TT:       " << std::setprecision(15) << std::fixed << TT << std::endl;
    //std::cout << "ml_prime: " << ml_prime << std::endl;
    //std::cout << "md:       " << md << std::endl;
    //std::cout << "mm:       " << mm << std::endl;
    //std::cout << "mm_prime: " << mm_prime << std::endl;
    //std::cout << "mf:       " << mf << std::endl;
    //std::cout << "a1:       " << a1 << std::endl;
    //std::cout << "a2:       " << a2 << std::endl;
    //std::cout << "a3:       " << a3 << std::endl;
    //std::cout << "e:        " << e << std::endl;
    //
    //    Source J. Meeus, Celestial Equations.  Table 45a  page 309 to 310
    //    constant arrays.  The numbers do not change.
    //
    int D[] = {0,2,2,0,0,0,2,2,2,2,0,1,0,2,0,0,4,0,4,2,2,1,1,2,2,4,2,0,2,2,1,2,0, 0,2,2,2,4,0,3,2,4,0,2,2,2,4,0,4,1,2,0,1,3,4,2,0,1,2,2};
    int M[] = {0,0,0,0,1,0,0,-1,0,-1,1,0,1,0,0,0,0,0,0,1,1,0,1,-1,0,0,0,1,0,-1,0,-2,1, 2,-2,0,0,-1,0,0,1,-1,2,2,1,-1,0,0,-1,0,1,0,1,0,0,-1,2,1,0,0};
    int M_PRIME[] = {1,-1,0,2,0,0,-2,-1,1,0,-1,0,1,0,1,1,-1,3,-2,-1,0,-1,0,1,2,0,-3,-2, -1,-2,1,0,2,0,-1,1,0,-1,2,-1,1,-2,-1,-1,-2,0,1,4,0,-2,0,2,1,-2,-3,2,1,-1,3,-1 };
    int F[]= {0,0,0,0,0,2,0,0,0,0,0,0,0,-2,2,-2,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0, -2,2,0,2,0,0,0,0,0,0,-2,0,0,0,0,-2,-2,0,0,0,0,0,0,0,-2};
    int R_AMP[]={-20905355, -3699111, -2955968, -569925, 48888, -3149, 246158, -152138, -170733, -204586, -129620, 108743, 104755, 10321, 0, 79661, -34782, -23210, -21636, 24208, 30824, -8379, -16675, -12831, -10445, -11650, 14403, -7003, 0, 10056, 6322, -9884, 5751, 0, -4950, 4130, 0, -3958, 0, 3258, 2616, -1897, -2117, 2354, 0, 0, -1423, -1117, -1571, -1739, 0, -4421,0,0,0,0,1165,0,0,8752};

    double sigma_r, e_mult, r;
    //
    // sigma_r  - Periodic terms of moon motion (longitude, latitude, distance)
    //          for a given TT.  Sigma_R is .001 Km to avoid calculation rounding.
    //          Table 45.A of Meeus Astronomical Algorithms (61 rows) contains the most
    //          important operiodic terms.
    //          then more adjustments to SigmaR for Flattening of Earth, motion of Venus.
    // e_mult    - Eccentricity multiplier.  Depends on M.  If M +/- 1, then multiply M
    //          by E.  If M is +/- 2, then Multiply M by E*E.  If M is 0, then Efactor is 1.
    // r         - Calculated for each period term's (calculation of rows in array).
    //          Temporary for testing and validation.

    sigma_r = 0;
    for (int i = 0; i < 60; i++) {
        e_mult = 1;
        if (M[i] == 1 || M[i] == -1) {
            e_mult=e;
        }
        if (M[i] == 2 || M[i] == -2) {
            e_mult = e*e;
        }
        r = R_AMP[i] * cos( d_to_r( (D[i] * md) + (e_mult * M[i] * mm) +
                     (M_PRIME[i] * mm_prime) + (F[i] * mf)) ) ;
        sigma_r = sigma_r + r;
        //std::cout << "Table 45.A  " <<i <<", " <<D[i] <<", " <<M[i] <<", " <<M_PRIME[i] <<", " <<F[i] <<", " <<e_mult <<", " <<R_AMP[i] <<", r:" <<r <<", Accum r: " <<sigma_r << std::endl;

        }

    //
    //  m_distsmvr - After sigma_r and sigma_r adjustments,
    //          convert from .001 km units to whole km units.
    //          Then add a constant (representing the base moon distance to earth).
    //          Note that in moon measured to be 384,400km (2/2025).
    //          I used constant supplied by Meeus since sigma_r assumed the base number.
    //
    //  m_distance is the Geocentric Distance of Moon from the Earth at time tt.
    //           Geocentric means, earth's center to moon's center.

    m_distance = BASE_MOON_DISTANCE + sigma_r / 1000 ;

    return m_distance;
}

#if 0
int main() {
    double JD;
    //
    // JD Julian Date
    // JDE Julian Ephemerides Date.  jde = jd found to be good enough in testing.
    // TT Terrestial Julian Time in centuries starting from Jan 1 2000 Epoch
    //
    // For unit testing, used https://aa.usno.navy.mil/data/JulianDate.
    //      input UTC1 and the site returns Julian Date.
    //
    std::cout << "Enter Julian Date: ";
    std::cin >> JD ;

    //
    //  Function moon-distance determines Geocentric Moon Distance to Earth.
    //  based on Time.
    //  Moon semi-diameter and parallax is based on Moon Distance in km.
    //  moon_semidiameter is a simplified equation.
    //  Alt_Moon_Semidiameter is a more precise equation.
    //  Meeus used K=.27248.  K =  Moon Mean radius/Earth Radius at Equator
    //
    double moon_dist, moon_semidiameter, moon_parallax, alt_moon_semidiameter;

    double EARTH_RADIUS     = 6378.14 ;    //Earth radius at equator in km.
    double MOON_MEAN_RADIUS = 1737.5;  // Moon mean radius in km
    double K                = MOON_MEAN_RADIUS / EARTH_RADIUS ;
    //
    // Function "moon_distance". Geocentric Moon to Earth distance in km at time TT.
    //
    moon_dist = moon_distance(JD) ;    // geocentric distance in km
    //
    // SemiDiameter and Parallax is based on Distance, Earth Radius, Moon Radius
    //
    moon_semidiameter = 5974556.667/moon_dist;
               // in arc-minutes (same units as Nautical Almanac.
    moon_parallax = 60 * r_to_d(asin(EARTH_RADIUS/moon_dist));
               // in arc-minutes (same units as Nautical Almanac.
    alt_moon_semidiameter=60 * r_to_d(asin(K*sin(d_to_r(moon_parallax/60))));    // in arc minutes
    std::cout << "Moon Distance (km):    " << moon_dist << std::endl;
    std::cout << "Moon Diameter (km):    " << EARTH_RADIUS << std::endl;
    std::cout << "MoonParallax (min)     " << moon_parallax << std::endl;
    std::cout << "MoonSemidiameter       " << moon_semidiameter << std::endl;
    std::cout << "Alt MoonSemidiameter   " << alt_moon_semidiameter << std::endl;

    return 0;
}
#endif

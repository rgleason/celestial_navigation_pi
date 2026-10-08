// Independent analytic checks for the isolated velocity experiment and the
// observer-aware ephemeris interface. No JPL angle fixtures enter these tests.
#include "LunarDistanceEngine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void near(double actual, double expected, double tolerance) {
  if (!std::isfinite(actual) || std::abs(actual-expected) > tolerance)
    throw std::runtime_error("Analytic component comparison failed");
}

int main() {
  try {
    constexpr double pi = 3.14159265358979323846;
    lunar_distance::Observation o;
    o.use_ellipsoid = true;
    o.artificial_horizon = true;
    o.pressure_hpa = 0;
    o.eye_height_m = 0;
    o.moon_altitude_limb = o.body_altitude_limb = lunar_distance::AltitudeLimb::Center;
    o.moon_contact = o.body_contact = lunar_distance::DistanceContact::Center;
    for (double height : {0.0,400.0}) {
      o.eye_height_m = height;
      for (double sign : {-1.0,1.0}) {
        auto ephemeris = [sign](double, lunar_distance::EphemerisSample* s, std::string*) {
          *s = {};
          s->body_geographic_longitude_deg = sign*45;
          s->moon_geographic_longitude_deg = 20;
          return true;
        };
        o.diurnal_aberration = false;
        auto plain = lunar_distance::PredictTimeTaggedObservation(o,ephemeris,0,{0,0});
        if (!plain.valid) throw std::runtime_error(plain.error);
        near(plain.body_altitude_deg/2,45,1e-11);
        o.diurnal_aberration = true;
        auto corrected = lunar_distance::PredictTimeTaggedObservation(o,ephemeris,0,{0,0});
        if (!corrected.valid) throw std::runtime_error(corrected.error);
        const double beta = 7.2921150e-5*(6378.137+height/1000)/299792.458;
        // Scalar Lorentz velocity-addition formula along the east axis,
        // independent of the implementation's three-vector transformation.
        const double q = std::sqrt(0.5);
        const double east = (sign*q+beta)/(1+beta*sign*q);
        const double up = q*std::sqrt(1-beta*beta)/(1+beta*sign*q);
        near(corrected.body_altitude_deg/2,std::atan2(up,std::abs(east))*180/pi,1e-10);
      }
    }
    // The callback must receive the actual trial station and the correct
    // separate observation epochs, not the input guess or distance epoch.
    o.diurnal_aberration = false;
    o.eye_height_m = 12;
    o.separate_times = true;
    o.moon_time_offset_seconds = -90;
    o.body_time_offset_seconds = 120;
    int calls = 0;
    auto ephemeris = [&calls](double epoch, lunar_distance::EphemerisSample* s, std::string*) {
      *s = {};
      s->observer_direction = [epoch,&calls](double lat,double lon,double height,bool moon,
                                            double* alt,double* az,double* sd) {
        ++calls;
        near(height,12,0);
        *alt = (moon ? 30 : 50) + epoch*0.01 + lat*0.02 + lon*0.03;
        *az = moon ? 40 : 130;
        *sd = 0;
      };
      return true;
    };
    for (double latitude : {-20.0,40.0}) {
      auto result = lunar_distance::PredictTimeTaggedObservation(o,ephemeris,23,{latitude,10});
      if (!result.valid) throw std::runtime_error(result.error);
      near(result.moon_altitude_deg/2,30-67*0.01+latitude*0.02+0.3,1e-10);
      near(result.body_altitude_deg/2,50+143*0.01+latitude*0.02+0.3,1e-10);
    }
    if (calls < 8) throw std::runtime_error("Observer provider was not used for all directions");
    std::cout << "Analytic aberration and observer-provider checks passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

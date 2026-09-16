// Calls the unmodified public forward model with artificial, exactly specified
// directions. This isolates atmospheric correction from ephemeris and parallax.
#include "LunarDistanceEngine.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    if (argc != 4) throw std::runtime_error("Usage: component-probe VACUUM_ALT P_HPA T_C");
    const double altitude = std::stod(argv[1]), pressure = std::stod(argv[2]), temperature = std::stod(argv[3]);
    if (!std::isfinite(altitude) || altitude <= 0 || altitude >= 90 ||
        !std::isfinite(pressure) || pressure < 0 || !std::isfinite(temperature) || temperature <= -273.15)
      throw std::runtime_error("Invalid atmospheric test input");
    lunar_distance::Observation o;
    o.use_ellipsoid = true;
    o.artificial_horizon = true; o.eye_height_m = 0;
    o.pressure_hpa = pressure; o.temperature_c = temperature;
    o.moon_altitude_limb = o.body_altitude_limb = lunar_distance::AltitudeLimb::Center;
    o.moon_contact = o.body_contact = lunar_distance::DistanceContact::Center;
    auto function = [altitude](double, lunar_distance::EphemerisSample* s, std::string*) {
      *s = lunar_distance::EphemerisSample();
      // At geodetic (0,0), a direction on the equator at longitude 90-h
      // has altitude h. Infinite range (HP=0) removes observer parallax.
      s->body_geographic_longitude_deg = 90 - altitude;
      s->moon_geographic_longitude_deg = 40;
      return true;
    };
    auto result = lunar_distance::PredictTimeTaggedObservation(o, function, 0, {0,0});
    if (!result.valid) throw std::runtime_error(result.error);
    std::cout << std::setprecision(17) << result.body_altitude_deg/2 << '\n';
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}

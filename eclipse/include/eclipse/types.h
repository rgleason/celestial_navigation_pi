#ifndef CELESTIAL_ECLIPSE_TYPES_H
#define CELESTIAL_ECLIPSE_TYPES_H

#include <cmath>

namespace eclipse {

struct GeoPoint {
  double latitude_deg;
  double longitude_deg;

  GeoPoint() : latitude_deg(0.0), longitude_deg(0.0) {}
  GeoPoint(double latitude, double longitude)
      : latitude_deg(latitude), longitude_deg(longitude) {}
};

struct JulianInstant {
  double terrestrial_time_jd;
  double delta_t_seconds;

  JulianInstant() : terrestrial_time_jd(0.0), delta_t_seconds(0.0) {}
  JulianInstant(double tt_jd, double delta_t)
      : terrestrial_time_jd(tt_jd), delta_t_seconds(delta_t) {}

  double universal_time_jd() const {
    return terrestrial_time_jd - delta_t_seconds / 86400.0;
  }
};

inline double DegreesToRadians(double degrees) {
  return degrees * 3.141592653589793238462643383279502884 / 180.0;
}

inline double RadiansToDegrees(double radians) {
  return radians * 180.0 / 3.141592653589793238462643383279502884;
}

inline double NormalizeLongitude(double longitude_deg) {
  double result = std::fmod(longitude_deg + 180.0, 360.0);
  if (result < 0.0) result += 360.0;
  return result - 180.0;
}

}  // namespace eclipse

#endif


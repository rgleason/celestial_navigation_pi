#ifndef CELESTIAL_ECLIPSE_VECTOR_H
#define CELESTIAL_ECLIPSE_VECTOR_H

#include <cmath>

namespace eclipse {

struct Vector3 {
  double x;
  double y;
  double z;

  Vector3() : x(0.0), y(0.0), z(0.0) {}
  Vector3(double x_value, double y_value, double z_value)
      : x(x_value), y(y_value), z(z_value) {}

  Vector3 operator+(const Vector3& other) const {
    return Vector3(x + other.x, y + other.y, z + other.z);
  }
  Vector3 operator-(const Vector3& other) const {
    return Vector3(x - other.x, y - other.y, z - other.z);
  }
  Vector3 operator*(double scale) const {
    return Vector3(x * scale, y * scale, z * scale);
  }
  Vector3 operator/(double scale) const {
    return Vector3(x / scale, y / scale, z / scale);
  }

  double Norm() const { return std::sqrt(x * x + y * y + z * z); }
};

inline double Dot(const Vector3& a, const Vector3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 Cross(const Vector3& a, const Vector3& b) {
  return Vector3(a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x);
}

inline Vector3 Normalize(const Vector3& value) {
  const double norm = value.Norm();
  return norm == 0.0 ? Vector3() : value / norm;
}

}  // namespace eclipse

#endif


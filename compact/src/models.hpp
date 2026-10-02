#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace celnav {
using Vec = std::array<double,3>;
struct PlanetTerm { unsigned planet, variable, power; double sine, cosine, phase, rate; };
class PlanetModel {
 public:
  explicit PlanetModel(const std::string& path);
  Vec Evaluate(int planet, double jd) const;
 private:
  std::array<std::vector<PlanetTerm>,7> terms_;
};
class MoonModel {
 public:
  MoonModel(const std::string& path, int fit);
  ~MoonModel();
  Vec Evaluate(double jd) const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
Vec LegacyHeliocentric(int planet, double jd, const std::string& path);
Vec LegacyMoonIcrf(double jd);
Vec VsopEclipticToIcrf(const Vec&);
Vec MoonEclipticToIcrf(const Vec&);
}

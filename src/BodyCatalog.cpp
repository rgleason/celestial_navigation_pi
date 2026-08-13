#include "BodyCatalog.h"

#include <algorithm>

const std::vector<CelestialBodyInfo>& BodyCatalog::All() {
  // Visual magnitudes are representative planning values.  The Sun, Moon and
  // planets vary; altitude, geometry and twilight carry more weight in the
  // best-sight score than these approximate brightness values.
  static const std::vector<CelestialBodyInfo> bodies = {
      {"Sun", CelestialBodyKind::Sun, -26.74},
      {"Moon", CelestialBodyKind::Moon, -12.7},
      {"Mercury", CelestialBodyKind::Planet, -0.5},
      {"Venus", CelestialBodyKind::Planet, -4.0},
      {"Mars", CelestialBodyKind::Planet, -1.0},
      {"Jupiter", CelestialBodyKind::Planet, -2.0},
      {"Saturn", CelestialBodyKind::Planet, 0.5},
      {"Acamar", CelestialBodyKind::Star, 2.88},
      {"Achernar", CelestialBodyKind::Star, 0.46},
      {"Acrux", CelestialBodyKind::Star, 0.77},
      {"Adhara", CelestialBodyKind::Star, 1.50},
      {"Aldebaran", CelestialBodyKind::Star, 0.86},
      {"Alioth", CelestialBodyKind::Star, 1.76},
      {"Alkaid", CelestialBodyKind::Star, 1.85},
      {"Al Na'ir", CelestialBodyKind::Star, 1.74},
      {"Alnilam", CelestialBodyKind::Star, 1.69},
      {"Alphard", CelestialBodyKind::Star, 1.98},
      {"Alphecca", CelestialBodyKind::Star, 2.23},
      {"Alpheratz", CelestialBodyKind::Star, 2.06},
      {"Altair", CelestialBodyKind::Star, 0.77},
      {"Ankaa", CelestialBodyKind::Star, 2.40},
      {"Antares", CelestialBodyKind::Star, 1.06},
      {"Arcturus", CelestialBodyKind::Star, -0.05},
      {"Atria", CelestialBodyKind::Star, 1.91},
      {"Avior", CelestialBodyKind::Star, 1.86},
      {"Bellatrix", CelestialBodyKind::Star, 1.64},
      {"Betelgeuse", CelestialBodyKind::Star, 0.50},
      {"Canopus", CelestialBodyKind::Star, -0.74},
      {"Capella", CelestialBodyKind::Star, 0.08},
      {"Deneb", CelestialBodyKind::Star, 1.25},
      {"Denebola", CelestialBodyKind::Star, 2.14},
      {"Diphda", CelestialBodyKind::Star, 2.04},
      {"Dubhe", CelestialBodyKind::Star, 1.79},
      {"Elnath", CelestialBodyKind::Star, 1.65},
      {"Eltanin", CelestialBodyKind::Star, 2.24},
      {"Enif", CelestialBodyKind::Star, 2.39},
      {"Fomalhaut", CelestialBodyKind::Star, 1.16},
      {"Gacrux", CelestialBodyKind::Star, 1.63},
      {"Gienah", CelestialBodyKind::Star, 2.58},
      {"Hadar", CelestialBodyKind::Star, 0.61},
      {"Hamal", CelestialBodyKind::Star, 2.00},
      {"Kaus Australis", CelestialBodyKind::Star, 1.79},
      {"Kochab", CelestialBodyKind::Star, 2.08},
      {"Markab", CelestialBodyKind::Star, 2.49},
      {"Menkar", CelestialBodyKind::Star, 2.54},
      {"Menkent", CelestialBodyKind::Star, 2.06},
      {"Miaplacidus", CelestialBodyKind::Star, 1.67},
      {"Mirfak", CelestialBodyKind::Star, 1.79},
      {"Nunki", CelestialBodyKind::Star, 2.05},
      {"Peacock", CelestialBodyKind::Star, 1.94},
      {"Polaris", CelestialBodyKind::Star, 1.98},
      {"Pollux", CelestialBodyKind::Star, 1.14},
      {"Procyon", CelestialBodyKind::Star, 0.34},
      {"Rasalhague", CelestialBodyKind::Star, 2.07},
      {"Regulus", CelestialBodyKind::Star, 1.35},
      {"Rigel", CelestialBodyKind::Star, 0.13},
      {"Rigil", CelestialBodyKind::Star, -0.27},
      {"Sabik", CelestialBodyKind::Star, 2.43},
      {"Schedar", CelestialBodyKind::Star, 2.24},
      {"Scheat", CelestialBodyKind::Star, 2.42},
      {"Shaula", CelestialBodyKind::Star, 1.62},
      {"Sirius", CelestialBodyKind::Star, -1.46},
      {"Spica", CelestialBodyKind::Star, 0.98},
      {"Suhail", CelestialBodyKind::Star, 2.21},
      {"Vega", CelestialBodyKind::Star, 0.03},
      {"Zubenelgenubi", CelestialBodyKind::Star, 2.75}};
  return bodies;
}

const CelestialBodyInfo* BodyCatalog::Find(const wxString& name) {
  const auto& all = All();
  const auto it = std::find_if(all.begin(), all.end(),
                               [&name](const CelestialBodyInfo& body) {
                                 return body.name.CmpNoCase(name) == 0;
                               });
  return it == all.end() ? nullptr : &*it;
}

std::vector<CelestialBodyInfo> BodyCatalog::Navigational(
    bool includeSun, bool includeMoon, bool includePlanets, bool includeStars) {
  std::vector<CelestialBodyInfo> result;
  for (const auto& body : All()) {
    if ((body.kind == CelestialBodyKind::Sun && includeSun) ||
        (body.kind == CelestialBodyKind::Moon && includeMoon) ||
        (body.kind == CelestialBodyKind::Planet && includePlanets) ||
        (body.kind == CelestialBodyKind::Star && includeStars))
      result.push_back(body);
  }
  return result;
}

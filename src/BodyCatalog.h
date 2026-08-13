/******************************************************************************
 * Shared catalogue of bodies supported by the celestial-navigation engine.
 *
 * Keep user-interface lists, planning, ranking and almanac export on the same
 * stable names: Sight::BodyLocation uses these names as its public body IDs.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_BODY_CATALOG_H
#define CELESTIAL_NAVIGATION_BODY_CATALOG_H

#include <wx/string.h>

#include <vector>

enum class CelestialBodyKind { Sun, Moon, Planet, Star };

struct CelestialBodyInfo {
  wxString name;
  CelestialBodyKind kind;
  double visualMagnitude;
};

class BodyCatalog {
public:
  static const std::vector<CelestialBodyInfo>& All();
  static const CelestialBodyInfo* Find(const wxString& name);
  static std::vector<CelestialBodyInfo> Navigational(bool includeSun,
                                                     bool includeMoon,
                                                     bool includePlanets,
                                                     bool includeStars);
};

#endif

#ifndef CELESTIAL_COMPACT_EPHEMERIS_PROVIDER_H
#define CELESTIAL_COMPACT_EPHEMERIS_PROVIDER_H

#include "NavigationEphemerisProvider.h"
#include "LunarDistanceEngine.h"
#include "celnav/engine.hpp"
#include "eclipse/dut1.h"
#include <memory>

namespace celestial_navigation {
bool CompactEphemerisEnabled();
void SetCompactEphemerisEnabled(bool enabled);
std::shared_ptr<const celnav::Engine> CompactEngine(
    std::string* reason = nullptr);
std::string CompactUtc(const wxDateTime& utc_fields,
                       bool time_is_instant = false);
// UTC fields and explicit Earth-rotation/leap inputs keep the plugin's dated
// bundled/imported IERS policy authoritative over the standalone defaults.
celnav::Request CompactRequest(
    const wxString& body, const wxDateTime& utc_fields,
    double dut1_override_seconds,
    const std::shared_ptr<const eclipse::Dut1Table>& update,
    bool time_is_instant = false);
bool TryCompactNavigationSample(
    const wxString& body, const wxDateTime& utc_fields,
    De440NavigationSample* sample, std::string* reason = nullptr,
    double dut1_override_seconds = std::numeric_limits<double>::quiet_NaN(),
    celnav::Result* result = nullptr, double latitude_deg = 0,
    double longitude_deg = 0, double height_m = 0,
    bool time_is_instant = false);
struct EnhancedLunarProvider {
  lunar_distance::EphemerisFunction ephemeris;
  bool used_de440 = false;
  bool used_compact = false;
  std::string fallback_reason;
};
// An empty callback requests classic for the whole workflow. Once chosen, an
// enhanced callback fails explicitly; it never switches providers mid-search.
EnhancedLunarProvider SelectEnhancedLunarProvider(
    const wxString& body, const wxDateTime& reference_utc,
    double first_offset_seconds, double last_offset_seconds, bool allow_de440,
    bool allow_compact, std::shared_ptr<const eclipse::Dut1Table> update);
}  // namespace celestial_navigation
#endif

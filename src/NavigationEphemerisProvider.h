#ifndef CELESTIAL_NAVIGATION_EPHEMERIS_PROVIDER_H
#define CELESTIAL_NAVIGATION_EPHEMERIS_PROVIDER_H

#include <wx/datetime.h>
#include <wx/string.h>

#include <string>
#include <limits>

namespace celestial_navigation {

// Only solar-system body centres actually present in the compact DE440s SPK
// can use this provider. A false return means the established analytical
// path remains in charge; it is never a partially populated result.
struct De440NavigationSample {
  double gha_deg = 0.0;
  double declination_deg = 0.0;
  double aries_gha_deg = 0.0;
  double range_km = 0.0;
  double sun_range_au = 0.0;
  double horizontal_parallax_deg = 0.0;
  double semidiameter_deg = 0.0;
  double dut1_seconds = 0.0;
  double tai_minus_utc_seconds = 0.0;
  bool dut1_available = false;
  bool venus_phase_corrected = false;
};

struct De440ObserverDirection {
  double airless_altitude_deg = 0.0;
  double azimuth_deg = 0.0;
  double range_km = 0.0;
  double semidiameter_deg = 0.0;
};

// utc_fields is the plugin's UTC *calendar fields*, not a wxDateTime local
// instant. Milliseconds are preserved. The data file is verified locally;
// neither this function nor its analytical fallback ever accesses the net.
bool TryDe440NavigationSample(const wxString& body,
                              const wxDateTime& utc_fields,
                              De440NavigationSample* sample,
                              std::string* reason = nullptr,
                              double dut1_override_seconds =
                                  std::numeric_limits<double>::quiet_NaN());

// WGS84 observer-specific light time and parallax, without atmospheric
// refraction. The observer height is above the reference ellipsoid in metres.
bool TryDe440ObserverDirection(const wxString& body,
                              const wxDateTime& utc_fields,
                              double latitude_deg, double longitude_deg,
                              double height_m,
                              De440ObserverDirection* direction,
                              std::string* reason = nullptr,
                              double dut1_override_seconds =
                                  std::numeric_limits<double>::quiet_NaN());

}  // namespace celestial_navigation

#endif

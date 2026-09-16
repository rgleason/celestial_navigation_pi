/***************************************************************************
 * Shared navigation-entry helpers for plugin dialogs.
 *
 * OpenCPN owns the user's preferred coordinate display format.  Keep dialog
 * values as decimal degrees internally, while accepting every angle syntax
 * supported by the core plugin API and redisplaying through that same API.
 ***************************************************************************/

#ifndef CELESTIAL_NAVIGATION_UI_UTILS_H
#define CELESTIAL_NAVIGATION_UI_UTILS_H

#include <algorithm>
#include <cmath>

#include <wx/fileconf.h>
#include <wx/textctrl.h>

#include "OcpnApiCompat.h"

enum class NavigationAngleKind {
  Generic = 0,
  Latitude = 1,
  Longitude = 2,
};

inline int NavigationAngleFlag(NavigationAngleKind kind) {
  return static_cast<int>(kind);
}

inline wxString FormatNavigationAngle(
    double degrees, NavigationAngleKind kind = NavigationAngleKind::Generic,
    bool highPrecision = true) {
  return toSDMM_PlugIn(NavigationAngleFlag(kind), degrees, highPrecision);
}

inline bool ParseNavigationAngle(const wxString& text, NavigationAngleKind kind,
                                 double minimum, double maximum,
                                 double* degrees) {
  if (!degrees) return false;
  bool hasDigit = false;
  for (const wxUniChar character : text) {
    if (character >= '0' && character <= '9') {
      hasDigit = true;
      break;
    }
  }
  if (!hasDigit) return false;
  const double value = fromDMM_Plugin(text);
  if (!std::isfinite(value) || value < minimum || value > maximum) return false;
  if (kind == NavigationAngleKind::Latitude && std::abs(value) > 90.0)
    return false;
  if (kind == NavigationAngleKind::Longitude && std::abs(value) > 180.0)
    return false;
  *degrees = value;
  return true;
}

class NavigationAngleCtrl : public wxTextCtrl {
public:
  NavigationAngleCtrl(wxWindow* parent, NavigationAngleKind kind, double value,
                      double minimum, double maximum,
                      const wxSize& size = wxDefaultSize,
                      long style = wxTE_PROCESS_ENTER)
      : wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, size,
                   style),
        m_kind(kind),
        m_minimum(minimum),
        m_maximum(maximum) {
    SetAngle(value);
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
      Normalize();
      event.Skip();
    });
    Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { Normalize(); });
  }

  using wxTextCtrl::SetValue;

  void SetAngle(double degrees) {
    ChangeValue(FormatNavigationAngle(degrees, m_kind));
  }

  bool GetAngle(double* degrees) const {
    return ParseNavigationAngle(wxTextCtrl::GetValue(), m_kind, m_minimum,
                                m_maximum, degrees);
  }

  double GetAngleOr(double fallback = 0.0) const {
    double value = fallback;
    return GetAngle(&value) ? value : fallback;
  }

  bool Normalize() {
    double value = 0.0;
    if (!GetAngle(&value)) return false;
    SetAngle(value);
    return true;
  }

private:
  NavigationAngleKind m_kind;
  double m_minimum;
  double m_maximum;
};

struct CelestialNavigationDefaults {
  double eyeHeight = 2.0;
  double temperature = 10.0;
  double pressure = 1013.0;
  double indexError = 0.0;
  bool dipShort = false;
  double dipShortDistance = 0.0;
  bool artificialHorizon = false;
};

inline CelestialNavigationDefaults LoadCelestialNavigationDefaults() {
  CelestialNavigationDefaults defaults;
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return defaults;
  config->SetPath(_T("/PlugIns/CelestialNavigation"));
  config->Read(_T("DefaultEyeHeight"), &defaults.eyeHeight, 2.0);
  config->Read(_T("DefaultTemperature"), &defaults.temperature, 10.0);
  config->Read(_T("DefaultPressure"), &defaults.pressure, 1013.0);
  config->Read(_T("DefaultIndexError"), &defaults.indexError, 0.0);
  config->Read(_T("DefaultDIPShort"), &defaults.dipShort, false);
  config->Read(_T("DefaultDIPShortDistance"), &defaults.dipShortDistance, 0.0);
  config->Read(_T("DefaultArtificialHorizon"), &defaults.artificialHorizon,
               false);
  return defaults;
}

#endif

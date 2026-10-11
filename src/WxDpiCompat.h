#pragma once

#include <wx/window.h>

namespace celestial_ui {

template <typename T>
T FromDIP(wxWindow* window, T size) {
#if wxCHECK_VERSION(3, 1, 0) && !defined(__OCPN__ANDROID__)
  return window->FromDIP(size);
#else
  // wx 3.0 and pinned wxQt lack these APIs. Keep their logical coordinates;
  // Android's decorated surface supplies its own touch scaling.
  (void)window;
  return size;
#endif
}

template <typename T>
T ToDIP(wxWindow* window, T size) {
#if wxCHECK_VERSION(3, 1, 0) && !defined(__OCPN__ANDROID__)
  return window->ToDIP(size);
#else
  (void)window;
  return size;
#endif
}

}  // namespace celestial_ui

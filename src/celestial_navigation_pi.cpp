#include "PlatformMessageBox.h"
/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#include "wx/wxprec.h"
#include "Dut1UpdatePanel.h"
#ifdef __OCPN__ANDROID__
#include "AndroidDocumentImport.h"
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QMenu>
#include <dlfcn.h>
#include <wx/weakref.h>
#include <vector>
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers

#include <wx/stdpaths.h>

#include "OcpnApiCompat.h"

#include "celestial_navigation_pi.h"
#include "CelestialNavigationDialog.h"
#include "Sight.h"
#include "icons.h"
#include <wx/jsonreader.h>
#include <wx/jsonwriter.h>
#include <wx/jsonval.h>

using namespace std;

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
#ifdef __OCPN__ANDROID__
  // The Android support archive supplies private static wx libraries. Their
  // stock lists are not initialized by the host's separate wxApp instance.
  // Colour pickers dereference this database even for an explicit RGB colour.
  if (!wxTheColourDatabase) wxInitializeStockLists();
#endif
  return (opencpn_plugin*)new celestial_navigation_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

//---------------------------------------------------------------------------------------------------------
//
//    Celestial_Navigation PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

celestial_navigation_pi::celestial_navigation_pi(void* ppimgr)
    : opencpn_plugin_118(ppimgr),
      m_route_almanac_menu_id(-1),
      m_hasPositionFix(false),
      m_hasCursorPosition(false),
      m_cursorLatitude(0.0),
      m_cursorLongitude(0.0) {
  // Create the PlugIn icons
  initialize_images();

  // Create the PlugIn icons  -from shipdriver
  // loads png file for the listing panel icon
  wxFileName fn;
  auto path = celestial_navigation_pi_DataDir();
  fn.SetPath(path);
  fn.AppendDir("data");
  fn.SetFullName("celestial_navigation_panel.png");

  path = fn.GetFullPath();

  wxInitAllImageHandlers();

  wxLogDebug(wxString("Using icon path: ") + path);
  if (!wxImage::CanRead(path)) {
    wxLogDebug("Initiating image handlers.");
    wxInitAllImageHandlers();
  }
  wxImage panelIcon(path);
  if (panelIcon.IsOk())
    m_panelBitmap = wxBitmap(panelIcon);
  else
    wxLogWarning("Celestial Navigation Panel icon has NOT been loaded");
  // End of from Shipdriver
}

celestial_navigation_pi::~celestial_navigation_pi(void) {}

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

int celestial_navigation_pi::Init(void) {
#ifdef __OCPN__ANDROID__
  celestial_android::CleanAbandonedImports();
#endif
  celestial_navigation::LoadInstalledDut1Update();
  AddLocaleCatalog(_T("opencpn-celestial_navigation_pi"));

  // Get a pointer to the opencpn display canvas, to use as a parent for windows
  // created
  m_parent_window = GetOCPNCanvasWindow();

  //    This PlugIn needs a toolbar icon, so request its insertion

#ifdef PLUGIN_USE_SVG
  m_leftclick_tool_id = InsertPlugInToolSVG(
      "Celestial Navigation", _svg_celestial_navigation,
      _svg_celestial_navigation_rollover, _svg_celestial_navigation_toggled,
      wxITEM_CHECK, _("Celestial Navigation"), "", NULL,
      CELESTIAL_NAVIGATION_TOOL_POSITION, 0, this);
#else
  m_leftclick_tool_id =
      InsertPlugInTool("", _img_celestial_navigation, _img_celestial_navigation,
                       wxITEM_NORMAL, _("Celestial Navigation"), "", NULL,
                       CELESTIAL_NAVIGATION_TOOL_POSITION, 0, this);
#endif

  m_pCelestialNavigationDialog = NULL;

#ifdef __OCPN__ANDROID__
  // AddCanvasMenuItem retains the wx item but does not own it. wxQt also
  // does not delete its QMenu in wxMenu's destructor. Keep both lifetimes
  // explicit so no QAction with a plugin vtable survives dlclose.
  m_androidRouteMenu = new wxMenu;
  m_androidRouteMenuItem = new wxMenuItem(m_androidRouteMenu, wxID_ANY,
                                         _("Generate fallback almanac..."));
  m_route_almanac_menu_id = AddCanvasMenuItem(m_androidRouteMenuItem, this, "Route");
#else
  wxMenu routeMenu;
  m_route_almanac_menu_id = AddCanvasMenuItem(
      new wxMenuItem(&routeMenu, wxID_ANY,
                     _("Generate fallback almanac...")),
      this, "Route");
#endif

#ifdef CELESTIAL_ECLIPSE_INTEGRATION_TEST
  wxTheApp->CallAfter([this]() {
    OnToolbarToolCallback(m_leftclick_tool_id);
    if (m_pCelestialNavigationDialog) {
      m_pCelestialNavigationDialog->RunEclipseIntegrationScenario();
      // Centre the Test-OpenCPN canvas on the 2027 path at a useful regional
      // scale.  JumpToPosition takes pixels/metre, not a chart denominator.
      JumpToPosition(25.5, 33.2, 5e-4);
      RequestRefresh(GetOCPNCanvasWindow());
    }
  });
#endif

#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
  wxTheApp->CallAfter([this]() {
    OnToolbarToolCallback(m_leftclick_tool_id);
    if (m_pCelestialNavigationDialog)
      m_pCelestialNavigationDialog->RunPlannerIntegrationScenario();
  });
#endif

  return (WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK |
          WANTS_NMEA_EVENTS | WANTS_NMEA_SENTENCES |
          WANTS_CURSOR_LATLON |
          WANTS_TOOLBAR_CALLBACK | WANTS_PLUGIN_MESSAGING |
          INSTALLS_TOOLBAR_TOOL);
}

bool celestial_navigation_pi::DeInit(void) {
#ifdef __OCPN__ANDROID__
  // wxWindow::Destroy delegates scheduling to the host wxApp, whose idle
  // queue may not run during Plugin Manager's modal import. Collect only
  // this private static wx library's windows, and remove these exact objects
  // from either queue before synchronously releasing them before dlclose.
  void* host = dlopen("libgorp.so", RTLD_NOW | RTLD_NOLOAD);
  auto* hostPending = host ? reinterpret_cast<wxList*>(dlsym(host, "wxPendingDelete")) : nullptr;
  if (host) dlclose(host);
  std::vector<wxWeakRef<wxWindow>> ownedWindows;
  std::vector<QPointer<QWidget>> ownedNativeWindows;
  for (auto node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext()) {
    ownedWindows.emplace_back(node->GetData());
    ownedNativeWindows.emplace_back(node->GetData()->GetHandle());
  }
  auto removePending = [hostPending](wxWindow* window) {
    wxPendingDelete.DeleteObject(window);
    if (hostPending && hostPending != &wxPendingDelete)
      hostPending->DeleteObject(window);
  };
  for (auto& window : ownedWindows) if (window) removePending(window.get());
#endif
  if (m_route_almanac_menu_id >= 0) {
    RemoveCanvasMenuItem(m_route_almanac_menu_id, "Route");
    m_route_almanac_menu_id = -1;
  }
#ifdef __OCPN__ANDROID__
  QPointer<QMenu> nativeRouteMenu = m_androidRouteMenu
      ? m_androidRouteMenu->GetHandle() : nullptr;
  if (m_androidRouteMenuItem && m_androidRouteMenuItem->GetHandle())
    m_androidRouteMenuItem->GetHandle()->blockSignals(true);
  delete m_androidRouteMenuItem;
  m_androidRouteMenuItem = nullptr;
  delete m_androidRouteMenu;
  m_androidRouteMenu = nullptr;
  delete nativeRouteMenu.data();
#endif
  RemovePlugInTool(m_leftclick_tool_id);

  if (m_pCelestialNavigationDialog) {
    // Do not route application shutdown through the normal window-close
    // handler.  OnDialogClose() uses wxWindow::Destroy(), which is deferred;
    // once OpenCPN's main event loop is stopping that deferred destruction can
    // leave this top-level dialog alive and keep the process running.  Clear
    // the plugin pointer first and destroy the owned dialog synchronously.
    CelestialNavigationDialog* dialog = m_pCelestialNavigationDialog;
    m_pCelestialNavigationDialog = NULL;
    dialog->Hide();
    delete dialog;
  }
#ifdef __OCPN__ANDROID__
  for (auto& window : ownedWindows) {
    if (!window) continue;
    removePending(window.get());
    delete window.get();
  }
  // This pinned wxQt defers native widget destruction even after the wx
  // wrapper is gone. A queued Qt event can then call a plugin vtable after
  // dlclose. Retain guarded handles before destroying the wrappers, clear
  // their wx handler properties, and release these exact owned native roots
  // while plugin code is still mapped. A parent may delete another root:
  // QPointer makes the later entry harmless in that case.
  for (auto& native : ownedNativeWindows) {
    if (!native) continue;
    const auto widgets = native->findChildren<QWidget*>();
    for (auto* widget : widgets) {
      wxWindow::QtStoreWindowPointer(widget, nullptr);
      widget->blockSignals(true);
    }
    wxWindow::QtStoreWindowPointer(native.data(), nullptr);
    native->blockSignals(true);
    delete native.data();
  }
  // wxWindow's Qt destructor also posts deleteLater for its parentless
  // wxQtShortcutHandler. Those objects cannot be found in a widget tree.
  // Complete only already-scheduled Qt deletions while our vtables remain
  // mapped; do not pump input, timers, paint or worker completion callbacks.
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  qInfo() << "Celestial Android owned sheets released before unload:" << ownedWindows.size();

#endif
  return true;
}

void celestial_navigation_pi::OnContextMenuItemCallback(int id) {
  if (id != m_route_almanac_menu_id) return;
  const wxString routeGuid = GetSelectedRouteGUID_Plugin();
  if (routeGuid.empty()) return;
  if (!m_pCelestialNavigationDialog)
    OnToolbarToolCallback(m_leftclick_tool_id);
  if (!m_pCelestialNavigationDialog) return;
  m_pCelestialNavigationDialog->Show();
  m_pCelestialNavigationDialog->Raise();
  m_pCelestialNavigationDialog->OpenAlmanacForRoute(routeGuid);
}

int celestial_navigation_pi::GetAPIVersionMajor() {
  return OCPN_API_VERSION_MAJOR;
}

int celestial_navigation_pi::GetAPIVersionMinor() {
  return OCPN_API_VERSION_MINOR;
}

int celestial_navigation_pi::GetPlugInVersionMajor() {
  return PLUGIN_VERSION_MAJOR;
}

int celestial_navigation_pi::GetPlugInVersionMinor() {
  return PLUGIN_VERSION_MINOR;
}

int celestial_navigation_pi::GetPlugInVersionPatch() {
  return PLUGIN_VERSION_PATCH;
}

int celestial_navigation_pi::GetPlugInVersionPost() {
  return PLUGIN_VERSION_TWEAK;
}

// wxBitmap *celestial_navigation_pi::GetPlugInBitmap()
//{
//     return new wxBitmap(_img_celestial_navigation->ConvertToImage().Copy());
// }

// Shipdriver uses the climatology_panel.png file to make the bitmap.
wxBitmap* celestial_navigation_pi::GetPlugInBitmap() { return &m_panelBitmap; }
// End of shipdriver process

wxString celestial_navigation_pi::GetCommonName() {
  //   return _("Celestial Navigation");
  return _T(PLUGIN_COMMON_NAME);
}

wxString celestial_navigation_pi::GetShortDescription() {
  return _(PLUGIN_SHORT_DESCRIPTION);
}

wxString celestial_navigation_pi::GetLongDescription() {
  return _(PLUGIN_LONG_DESCRIPTION);
}

void celestial_navigation_pi::OnToolbarToolCallback(int id) {
  int ret;
  if (!m_pCelestialNavigationDialog) {
    /* load the geographical magnetic table */
    wxString geomag_text_path = celestial_navigation_pi_DataDir();
    geomag_text_path.Append(_T("/data/IGRF11.COF"));

    wxLogMessage("Celestial: OnToolbarToolCallback enter");
    wxLogMessage("Celestial: geomag path = %s", geomag_text_path);

    if ((ret = geomag_load(geomag_text_path.mb_str())) < 0) {
      wxLogWarning("Celestial: geomag_load returned %d", ret);
      wxString message = _("Failed to load file: ") + geomag_text_path + "\n";
      switch (ret) {
        case -1:
          message += "(" + _("open error") + ")\n";
          break;
        case -5:
          message += "(" + _("corrupt record") + ")\n";
          break;
        case -6:
          message += "(" + _("too many models") + ")\n";
          break;
      }
      CelestialMessageDialog mdlg(m_parent_window,
                           message + _("Magnetic data will not be available "
                                       "for the celestial navigation plugin."),
                           wxString(_("OpenCPN Alert"), wxOK | wxICON_ERROR));
      mdlg.ShowModal();
    } else {
      wxLogMessage("Celestial: geomag_load succeeded (ret=%d)", ret);
    }

    // Defensive: ensure parent window valid
    if (!m_parent_window) {
      wxLogWarning("Celestial: m_parent_window is NULL; calling GetOCPNCanvasWindow()");
      m_parent_window = GetOCPNCanvasWindow();
      if (!m_parent_window) {
        wxLogError("Celestial: Cannot obtain parent window; aborting dialog creation");
        return;
      }
    }

    wxLogMessage("Celestial: Creating CelestialNavigationDialog");
    m_pCelestialNavigationDialog =
        new CelestialNavigationDialog(m_parent_window, this);
    wxLogMessage("Celestial: CelestialNavigationDialog constructed at %p", (void*)m_pCelestialNavigationDialog);
  }

  m_pCelestialNavigationDialog->Show();
  m_pCelestialNavigationDialog->Raise();
}

int celestial_navigation_pi::GetToolbarToolCount(void) { return 1; }

void celestial_navigation_pi::SetColorScheme(PI_ColorScheme cs) {
  if (NULL == m_pCelestialNavigationDialog) return;

  DimeWindow(m_pCelestialNavigationDialog);
}

bool celestial_navigation_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
#ifdef CELESTIAL_ECLIPSE_INTEGRATION_TEST
  static bool logged_cpu_overlay = false;
  if (!logged_cpu_overlay && m_pCelestialNavigationDialog &&
      m_pCelestialNavigationDialog->IsShown()) {
    wxLogMessage(
        "CELESTIAL_ECLIPSE_INTEGRATION_TEST: CPU overlay callback active");
    logged_cpu_overlay = true;
  }
#endif
  piDC* pidc = new piDC(dc);
  bool ret = RenderOverlayAll(pidc, vp);
  delete pidc;
  return ret;
}

bool celestial_navigation_pi::RenderGLOverlay(wxGLContext* pcontext,
                                              PlugIn_ViewPort* vp) {
#ifdef __OCPN__ANDROID__
  GLint previousProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
#endif
#ifdef CELESTIAL_ECLIPSE_INTEGRATION_TEST
  static bool logged_gl_overlay = false;
  if (!logged_gl_overlay && m_pCelestialNavigationDialog &&
      m_pCelestialNavigationDialog->IsShown()) {
    wxLogMessage(
        "CELESTIAL_ECLIPSE_INTEGRATION_TEST: OpenGL overlay callback active");
    logged_gl_overlay = true;
  }
#endif
  piDC* pidc = new piDC(pcontext);
  pidc->SetVP(vp);
  bool ret = RenderOverlayAll(pidc, vp);
  delete pidc;
#ifdef __OCPN__ANDROID__
  glUseProgram(previousProgram);
#endif
  return ret;
}

bool celestial_navigation_pi::RenderOverlayAll(piDC* dc, PlugIn_ViewPort* vp) {
  if (!m_pCelestialNavigationDialog) return false;
#ifndef __OCPN__ANDROID__
  if (!m_pCelestialNavigationDialog->IsShown()) return false;
#endif

  /* draw sights */
  for (Sight& s : m_pCelestialNavigationDialog->m_Sights) {
    s.Render(dc, *vp, m_pCelestialNavigationDialog->m_pix_per_mm);
  }

  m_pCelestialNavigationDialog->RenderEclipse(dc, vp);
  m_pCelestialNavigationDialog->RenderCoastal(dc, vp);

  if (!m_pCelestialNavigationDialog->m_FixDialog) return true;
#ifndef __OCPN__ANDROID__
  if (!m_pCelestialNavigationDialog->m_FixDialog->IsShown()) return true;
#endif

  /* now render fix */
  double lat = m_pCelestialNavigationDialog->m_FixDialog->m_fixlat;
  double lon = m_pCelestialNavigationDialog->m_FixDialog->m_fixlon;
  double err = m_pCelestialNavigationDialog->m_FixDialog->m_fixerror;

  if (!isnan(err)) {
    wxPoint r;
    GetCanvasPixLL(vp, &r, lat, lon);
    int crosslen = (int)(10.0 * m_pCelestialNavigationDialog->m_pix_per_mm);

    dc->SetPen(wxPen(wxColor(255, 0, 0),
                     (int)(0.5 * m_pCelestialNavigationDialog->m_pix_per_mm)));
    dc->SetBrush(*wxTRANSPARENT_BRUSH);
    dc->DrawLine(r.x - crosslen, r.y - crosslen, r.x + crosslen,
                 r.y + crosslen);
    dc->DrawLine(r.x - crosslen, r.y + crosslen, r.x + crosslen,
                 r.y - crosslen);
  }
  return true;
}

wxString celestial_navigation_pi::StandardPath() {
  wxString stdPath(*GetpPrivateApplicationDataLocation());
  stdPath = stdPath + wxFileName::GetPathSeparator() + "plugins" +
            wxFileName::GetPathSeparator() + "celestial_navigation" +
            wxFileName::GetPathSeparator();
  return stdPath;
}

static double s_boat_lat, s_boat_lon;
void celestial_navigation_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  s_boat_lat = pfix.Lat;
  s_boat_lon = pfix.Lon;
  m_hasPositionFix = std::isfinite(pfix.Lat) && std::isfinite(pfix.Lon) &&
                     pfix.Lat >= -90.0 && pfix.Lat <= 90.0 &&
                     pfix.Lon >= -180.0 && pfix.Lon <= 180.0;
  m_navigation.valid = m_hasPositionFix;
  m_navigation.latitude = pfix.Lat;
  m_navigation.longitude = pfix.Lon;
  m_navigation.cogTrue = std::isfinite(pfix.Cog) ? pfix.Cog : 0.0;
  m_navigation.sogKnots = std::isfinite(pfix.Sog) ? pfix.Sog : 0.0;
  m_navigation.variation = std::isfinite(pfix.Var) ? pfix.Var : 0.0;
  if (pfix.FixTime > 0)
    m_navigation.fixUtc = wxDateTime(static_cast<time_t>(pfix.FixTime));
}

void celestial_navigation_pi::SetNMEASentence(wxString& sentence) {
  m_gnssTime.Update(sentence);
}

GnssTimeSnapshot celestial_navigation_pi::GetGnssTimeSnapshot() const {
  return m_gnssTime.Snapshot();
}

bool celestial_navigation_pi::GetBoatPosition(double* latitude,
                                              double* longitude) const {
  if (!m_hasPositionFix || !latitude || !longitude) return false;
  *latitude = s_boat_lat;
  *longitude = s_boat_lon;
  return true;
}

BoatNavigationSnapshot celestial_navigation_pi::GetBoatNavigationSnapshot()
    const {
  return m_navigation;
}

bool celestial_navigation_pi::GetCursorPosition(double* latitude,
                                                double* longitude) const {
  if (!m_hasCursorPosition || !latitude || !longitude) return false;
  *latitude = m_cursorLatitude;
  *longitude = m_cursorLongitude;
  return true;
}

void celestial_navigation_pi::SetCursorLatLon(double lat, double lon) {
  m_cursorLatitude = lat;
  m_cursorLongitude = lon;
  m_hasCursorPosition = std::isfinite(lat) && std::isfinite(lon) &&
                        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 &&
                        lon <= 180.0;
}

void celestial_navigation_pi_BoatPos(double& lat, double& lon) {
  lat = s_boat_lat;
  lon = s_boat_lon;
}

double gQueryVar = 0;

void celestial_navigation_pi::SetPluginMessage(wxString& message_id,
                                               wxString& message_body) {
  if (message_id == _T("WMM_VARIATION")) {
    wxJSONValue root;
    wxJSONReader reader;
    if (reader.Parse(message_body, &root) > 0) return;

    wxString decl = root[_T("Decl")].AsString();
    double decl_val;
    decl.ToDouble(&decl_val);

    gQueryVar = decl_val;
  }
}

void celestial_navigation_pi::OnDialogClose() {
#ifdef __OCPN__ANDROID__
  if (m_pCelestialNavigationDialog) m_pCelestialNavigationDialog->Hide();
  RequestRefresh(m_parent_window);
  return;
#endif
  CelestialNavigationDialog* dialog = m_pCelestialNavigationDialog;
  m_pCelestialNavigationDialog = NULL;
  if (!dialog) return;
  dialog->Hide();
  dialog->Destroy();
}

double celestial_navigation_pi_GetWMM(double lat, double lon, double altitude,
                                      wxDateTime date) {
#ifdef __OCPN__ANDROID__
  const auto utc = UtcDateTime::Fields(date);
  const int year = utc.year, month = utc.mon, day = utc.mday;
#else
  const int year = date.GetYear(), month = date.GetMonth(), day = date.GetDay();
#endif
  wxJSONValue v;
  v[_T("Lat")] = lat;
  v[_T("Lon")] = lon;
  v[_T("Year")] = year;
  v[_T("Month")] = month;
  v[_T("Day")] = day;

  wxJSONWriter w;
  wxString out;
  w.Write(v, out);

  gQueryVar = 360;
  SendPluginMessage(wxString(_T("WMM_VARIATION_REQUEST")), out);
  if (gQueryVar == 360) {
    double results[14];
    geomag_calc(lat, lon, altitude / 1000, day, month,
                year, results);
    return results[0];
  }

  return gQueryVar;
}

wxString celestial_navigation_pi_DataDir() {
  static wxString dataDir;
  if (dataDir.Len() == 0) {
    dataDir = GetPluginDataDir("celestial_navigation_pi");
  }
  return dataDir;
}

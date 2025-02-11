/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN development team                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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
 **************************************************************************/

#include <wx/wx.h>
#include <wx/html/htmlwin.h>
#include <wx/html/winpars.h>

#include "ocpn_plugin.h"

// API 19 implementations

// API 18 implementations
opencpn_plugin_118::opencpn_plugin_118(void* ppimgr)
    : opencpn_plugin_117(ppimgr) {}
opencpn_plugin_18::~opencpn_plugin_18() {}
bool opencpn_plugin_18::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
  return false;
}
bool opencpn_plugin_18::RenderGLOverlay(wxGLContext* pcontext,
                                        PlugIn_ViewPort* vp) {
  return false;
}
void opencpn_plugin_18::SetPluginMessage(wxString& message_id,
                                         wxString& message_body) {}
void opencpn_plugin_18::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {}
bool opencpn_plugin_118::RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp,
                                                  int canvasIndex,
                                                  int defaultCanvasIndex) {
  return false;
}
bool opencpn_plugin_118::RenderGLOverlayMultiCanvas(wxGLContext* pcontext,
                                                    PlugIn_ViewPort* vp,
                                                    int canvasIndex,
                                                    int defaultCanvasIndex) {
  return false;
}

// API 17 implementations
opencpn_plugin_117::opencpn_plugin_117(void* ppimgr)
    : opencpn_plugin_116(ppimgr) {}
const char* opencpn_plugin_117::GetPlugInVersionPre() { return ""; }
int opencpn_plugin_117::GetPlugInVersionPost() { return 0; }
const char* opencpn_plugin_117::GetPlugInVersionBuild() { return ""; }
int opencpn_plugin_117::GetPlugInVersionPatch() { return 0; }
void opencpn_plugin_117::SetActiveLegInfo(Plugin_Active_Leg_Info& leg_info) {}

// API 16 implementations
opencpn_plugin_116::opencpn_plugin_116(void* ppimgr)
    : opencpn_plugin_115(ppimgr) {}
opencpn_plugin_116::~opencpn_plugin_116() {}
void opencpn_plugin_116::PrepareContextMenu(int canvasIndex) {}
bool opencpn_plugin_116::RenderGLOverlayMultiCanvas(wxGLContext* pcontext,
                                                    PlugIn_ViewPort* vp,
                                                    int canvasIndex) {
  return false;
}
bool opencpn_plugin_116::RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp,
                                                  int canvasIndex) {
  return false;
}

// API 15 implementations
opencpn_plugin_115::opencpn_plugin_115(void* ppimgr)
    : opencpn_plugin_114(ppimgr) {}
opencpn_plugin_115::~opencpn_plugin_115() {}

// API 14 implementations
opencpn_plugin_114::opencpn_plugin_114(void* ppimgr)
    : opencpn_plugin_113(ppimgr) {}
opencpn_plugin_114::~opencpn_plugin_114() {}

// API 13 implementations
opencpn_plugin_113::opencpn_plugin_113(void* ppimgr)
    : opencpn_plugin_112(ppimgr) {}
opencpn_plugin_113::~opencpn_plugin_113() {}
bool opencpn_plugin_113::KeyboardEventHook(wxKeyEvent& event) { return false; }
void opencpn_plugin_113::OnToolbarToolUpCallback(int id) {}
void opencpn_plugin_113::OnToolbarToolDownCallback(int id) {}

// API 12 implementations
opencpn_plugin_112::opencpn_plugin_112(void* ppimgr)
    : opencpn_plugin_111(ppimgr) {}
opencpn_plugin_112::~opencpn_plugin_112() {}
bool opencpn_plugin_112::MouseEventHook(wxMouseEvent& event) { return false; }
void opencpn_plugin_112::SendVectorChartObjectInfo(
    wxString& chart, wxString& feature, wxString& objname, double lat,
    double lon, double scale, int nativescale) {}

// API 11 implementations
opencpn_plugin_111::opencpn_plugin_111(void* ppimgr)
    : opencpn_plugin_110(ppimgr) {}
opencpn_plugin_111::~opencpn_plugin_111() {}

// API 10 implementations
opencpn_plugin_110::opencpn_plugin_110(void* ppimgr)
    : opencpn_plugin_19(ppimgr) {}
opencpn_plugin_110::~opencpn_plugin_110() {}
void opencpn_plugin_110::LateInit() {}

// API 9 implementations
opencpn_plugin_19::opencpn_plugin_19(void* ppimgr)
    : opencpn_plugin_18(ppimgr) {}
opencpn_plugin_19::~opencpn_plugin_19() {}
void opencpn_plugin_19::OnSetupOptions() {}

// API 8 implementations
opencpn_plugin_18::opencpn_plugin_18(void* ppimgr) : opencpn_plugin(ppimgr) {}

// Base plugin implementations
opencpn_plugin::~opencpn_plugin() {}
void opencpn_plugin::SetDefaults() {}
bool opencpn_plugin::RenderOverlay(wxMemoryDC* dc, PlugIn_ViewPort* vp) {
  return false;
}
void opencpn_plugin::SetPositionFix(PlugIn_Position_Fix& pfix) {}
void opencpn_plugin::SetCursorLatLon(double lat, double lon) {}
void opencpn_plugin::UpdateAuiStatus() {}
void opencpn_plugin::SetupToolboxPanel(int page, wxNotebook* notebook) {}
void opencpn_plugin::SetCurrentViewPort(PlugIn_ViewPort& vp) {}
void opencpn_plugin::OnCloseToolboxPanel(int page, int ok_apply_cancel) {}
void opencpn_plugin::ProcessParentResize(int x, int y) {}
int opencpn_plugin::GetToolboxPanelCount() { return 0; }
void opencpn_plugin::OnContextMenuItemCallback(int id) {}
wxArrayString opencpn_plugin::GetDynamicChartClassNameArray() {
  return wxArrayString();
}

wxString opencpn_plugin::GetCommonName() { return wxString("Mock Plugin"); }
void opencpn_plugin::SetAISSentence(wxString& sentence) { /* empty */ }
void opencpn_plugin::SetColorScheme(PI_ColorScheme cs) { /* empty */ }
wxBitmap* opencpn_plugin::GetPlugInBitmap() { return nullptr; }
void opencpn_plugin::SetNMEASentence(wxString& sentence) { /* empty */ }
int opencpn_plugin::GetAPIVersionMajor() { return 1; }
int opencpn_plugin::GetAPIVersionMinor() { return 18; }
wxString opencpn_plugin::GetLongDescription() { return wxString(); }
wxString opencpn_plugin::GetShortDescription() { return wxString(); }
int opencpn_plugin::GetToolbarToolCount() { return 0; }
int opencpn_plugin::GetPlugInVersionMajor() { return 1; }
int opencpn_plugin::GetPlugInVersionMinor() { return 0; }
void opencpn_plugin::OnToolbarToolCallback(int id) { /* empty */ }
void opencpn_plugin::ShowPreferencesDialog(wxWindow* parent) { /* empty */ }
int opencpn_plugin::Init() { return 0; }
bool opencpn_plugin::DeInit() { return true; }

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

#ifdef __WINDOWS__
#include <windows.h>
#endif

#include <wx/wx.h>
#include <wx/aui/framemanager.h>
#include <wx/bitmap.h>
#include <wx/fileconf.h>
#include <wx/font.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/event.h>

#include <vector>
#include <memory>
#include <string>

#include "ocpn_plugin.h"
#include "mock_plugin_api.h"

// Plugin API mock implementations
extern "C" {

DECL_EXP int GetChartbarHeight(void) { return 1; }
void SendPluginMessage(wxString message_id, wxString message_body) {}
bool AddLocaleCatalog(wxString catalog) { return true; }
bool GetGlobalColor(wxString colorName, wxColour* pcolour) { return true; }
static wxFileConfig* s_config = NULL;
wxFileConfig* GetOCPNConfigObject(void) {
  if (!s_config) {
    s_config = new wxFileConfig();
    // Add test defaults
    s_config->Write(_T("/PlugIns/CelestialNavigation/DefaultEyeHeight"), 2.0);
    s_config->Write(_T("/PlugIns/CelestialNavigation/DefaultTemperature"),
                    10.0);
    s_config->Write(_T("/PlugIns/CelestialNavigation/DefaultPressure"), 1010.0);
    s_config->Write(_T("/PlugIns/CelestialNavigation/DefaultIndexError"), 0.0);
  }
  return s_config;
}
wxAuiManager* GetFrameAuiManager(void) { return 0; }
wxWindow* GetOCPNCanvasWindow() { return 0; }

DECL_EXP void PushNMEABuffer(wxString str) {}

}  // extern "C"

void RemovePlugInTool(int tool_id) {}
DECL_EXP int InsertPlugInToolSVG(wxString label, wxString SVGfile,
                                 wxString SVGfileRollover,
                                 wxString SVGfileToggled, wxItemKind kind,
                                 wxString shortHelp, wxString longHelp,
                                 wxObject* clientData, int position,
                                 int tool_sel, opencpn_plugin* pplugin) {
  return 0;
}

DECL_EXP int InsertPlugInTool(wxString label, wxBitmap* bitmap,
                              wxBitmap* bmpRollover, wxItemKind kind,
                              wxString shortHelp, wxString longHelp,
                              wxObject* clientData, int position, int tool_sel,
                              opencpn_plugin* pplugin) {
  return 0;
}
void SetToolbarItemState(int item, bool toggle) {}

DECL_EXP int PlatformDirSelectorDialog(wxWindow* parent, wxString* file_spec,
                                       wxString Title, wxString initDir) {
  return 0;
}

DECL_EXP int PlatformFileSelectorDialog(wxWindow* parent, wxString* file_spec,
                                        wxString Title, wxString initDir,
                                        wxString suggestedName,
                                        wxString wildcard) {
  return 0;
}

DECL_EXP wxFont* GetOCPNScaledFont_PlugIn(wxString TextElement,
                                          int default_size) {
  return 0;
}

DECL_EXP wxFont* FindOrCreateFont_PlugIn(int point_size, wxFontFamily family,
                                         wxFontStyle style, wxFontWeight weight,
                                         bool ul, const wxString& face,
                                         wxFontEncoding enc) {
  return 0;
}

DECL_EXP void JumpToPosition(double lat, double lon, double scale) {}

// Plugin API mock implementations

wxString* GetpPrivateApplicationDataLocation(void) { return nullptr; }

wxString *GetpSharedDataLocation(void) { return nullptr; }

class ObservableListener {
public:
  ObservableListener(int /* unused */, wxEvtHandler* handler, wxEventType type)
      : m_handler(handler), m_type(type) {}
  wxEvtHandler* m_handler;
  wxEventType m_type;
};

// C++ functions with DECL_EXP go outside extern "C"
std::shared_ptr<ObservableListener> DECL_EXP GetListener(NMEA2000Id id,
                                                         wxEventType et,
                                                         wxEvtHandler* eh) {
  return std::make_shared<ObservableListener>(id.id, eh, et);
}

std::string DECL_EXP GetN2000Source(NMEA2000Id /* id */,
                                    ObservedEvt /* evt */) {
  return std::string("MockSource");
}

std::vector<uint8_t> DECL_EXP GetN2000Payload(NMEA2000Id /* id */,
                                              ObservedEvt /* evt */) {
  return std::vector<uint8_t>{0, 1, 2, 3};  // Mock data
}

wxString DECL_EXP GetPluginDataDir(const char* plugin_name) {
  const char* testdata = TESTDATA;
  return wxString(testdata);
}

// Define the wxAuiManager methods
bool wxAuiManager::DetachPane(wxWindow* window) {
  return true;  // Mock implementation always succeeds
}

bool wxAuiManager::AddPane(wxWindow* window, const wxAuiPaneInfo& pane_info) {
  return true;  // Mock implementation always succeeds
}

void wxAuiManager::Update() {
  // Mock implementation does nothing
}

wxAuiPaneInfo& wxAuiManager::GetPane(wxWindow* window) {
  static wxAuiPaneInfo info;
  return info;  // Return a static instance for mocking
}

bool wxAuiPaneInfo::IsValid() const { return true; }

void DimeWindow(wxWindow* win) {}
void GetCanvasPixLL(PlugIn_ViewPort* vp, wxPoint* pp, double lat, double lon) {}
void RequestRefresh(wxWindow* window) {}

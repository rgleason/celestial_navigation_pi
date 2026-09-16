#ifndef CELESTIAL_DUT1_UPDATE_PANEL_H
#define CELESTIAL_DUT1_UPDATE_PANEL_H
#include <wx/string.h>
class wxWindow;
namespace celestial_navigation {
wxWindow* CreateDut1UpdatePanel(wxWindow* parent);
void LoadInstalledDut1Update();  // Local file only; never connects at startup.
bool InstallDut1Update(const wxString& source, const wxString& destination,
                       wxString* message);
}
#endif

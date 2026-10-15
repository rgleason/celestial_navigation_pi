#ifndef CELESTIAL_DUT1_UPDATE_PANEL_H
#define CELESTIAL_DUT1_UPDATE_PANEL_H
#include <wx/string.h>
class wxWindow;
namespace celestial_navigation {
enum class Dut1PanelMode { AdvancedSettings, AccuracyAddons };
wxWindow* CreateDut1UpdatePanel(
    wxWindow* parent, Dut1PanelMode mode = Dut1PanelMode::AdvancedSettings);
void LoadInstalledDut1Update();  // Local file only; never connects at startup.
bool InstallDut1Update(const wxString& source, const wxString& destination,
                       wxString* message);
}
#endif

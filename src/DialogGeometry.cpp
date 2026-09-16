#include "DialogGeometry.h"

#include <algorithm>

#include <wx/display.h>
#include <wx/fileconf.h>
#include <wx/toplevel.h>

#include "OcpnApiCompat.h"

namespace dialog_geometry {
namespace {

wxRect WorkAreaFor(wxTopLevelWindow* dialog, const wxPoint* savedPosition) {
  int display = wxNOT_FOUND;
  if (savedPosition) display = wxDisplay::GetFromPoint(*savedPosition);
  if (display == wxNOT_FOUND && dialog->GetParent())
    display = wxDisplay::GetFromWindow(dialog->GetParent());
  if (display == wxNOT_FOUND && wxDisplay::GetCount() > 0) display = 0;
  if (display != wxNOT_FOUND)
    return wxDisplay(static_cast<unsigned int>(display)).GetClientArea();
  return wxRect(wxPoint(0, 0), wxGetDisplaySize());
}

wxString ConfigPath() {
  return _T("/PlugIns/CelestialNavigation/DialogGeometry");
}

}  // namespace

wxRect ClampToWorkArea(const wxRect& requested, const wxSize& minimum,
                       const wxRect& workArea) {
  if (workArea.GetWidth() <= 0 || workArea.GetHeight() <= 0) return requested;

  const int minWidth = std::max(
      1, std::min(minimum.GetWidth(), workArea.GetWidth()));
  const int minHeight = std::max(
      1, std::min(minimum.GetHeight(), workArea.GetHeight()));
  const int width = std::max(
      minWidth, std::min(requested.GetWidth(), workArea.GetWidth()));
  const int height = std::max(
      minHeight, std::min(requested.GetHeight(), workArea.GetHeight()));
  const int maxX = workArea.GetRight() - width + 1;
  const int maxY = workArea.GetBottom() - height + 1;
  const int x =
      std::max(workArea.GetLeft(), std::min(requested.GetX(), maxX));
  const int y =
      std::max(workArea.GetTop(), std::min(requested.GetY(), maxY));
  return wxRect(x, y, width, height);
}

void Restore(wxTopLevelWindow* dialog, const wxString& key,
             const wxSize& defaultSize) {
  if (!dialog) return;

  wxFileConfig* config = GetOCPNConfigObject();
  bool hasPosition = false;
  bool hasSize = false;
  long x = 0, y = 0, width = defaultSize.GetWidth();
  long height = defaultSize.GetHeight();
  if (config) {
    config->SetPath(ConfigPath());
    hasPosition = config->Read(key + _T("X"), &x) &&
                  config->Read(key + _T("Y"), &y);
    hasSize = config->Read(key + _T("Width"), &width) &&
              config->Read(key + _T("Height"), &height);
  }

  const wxSize requestedSize =
      hasSize ? wxSize(static_cast<int>(width), static_cast<int>(height))
              : defaultSize;
  const wxPoint requestedPosition =
      hasPosition ? wxPoint(static_cast<int>(x), static_cast<int>(y))
                  : dialog->GetPosition();
  const wxRect workArea =
      WorkAreaFor(dialog, hasPosition ? &requestedPosition : nullptr);
  const wxSize minimum = dialog->GetMinSize().IsFullySpecified()
                             ? dialog->GetMinSize()
                             : wxSize(1, 1);

  if (!hasPosition) {
    dialog->SetSize(ClampToWorkArea(
                        wxRect(workArea.GetPosition(), requestedSize), minimum,
                        workArea)
                        .GetSize());
    dialog->CentreOnParent();
    const wxRect centred = ClampToWorkArea(
        wxRect(dialog->GetPosition(), dialog->GetSize()), minimum, workArea);
    dialog->SetSize(centred);
    return;
  }

  dialog->SetSize(ClampToWorkArea(
      wxRect(requestedPosition, requestedSize), minimum, workArea));
}

void EnsureVisible(wxTopLevelWindow* dialog) {
  if (!dialog) return;
  const wxPoint position = dialog->GetPosition();
  const wxRect workArea = WorkAreaFor(dialog, &position);
  const wxSize minimum = dialog->GetMinSize().IsFullySpecified()
                             ? dialog->GetMinSize()
                             : wxSize(1, 1);
  dialog->SetSize(ClampToWorkArea(
      wxRect(position, dialog->GetSize()), minimum, workArea));
}

void Save(const wxTopLevelWindow* dialog, const wxString& key) {
  if (!dialog || dialog->IsIconized()) return;
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  config->SetPath(ConfigPath());
  const wxPoint position = dialog->GetPosition();
  const wxSize size = dialog->GetSize();
  config->Write(key + _T("X"), static_cast<long>(position.x));
  config->Write(key + _T("Y"), static_cast<long>(position.y));
  config->Write(key + _T("Width"), static_cast<long>(size.x));
  config->Write(key + _T("Height"), static_cast<long>(size.y));
}

}  // namespace dialog_geometry

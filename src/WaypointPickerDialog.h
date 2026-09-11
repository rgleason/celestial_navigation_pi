#ifndef CELESTIAL_WAYPOINT_PICKER_DIALOG_H
#define CELESTIAL_WAYPOINT_PICKER_DIALOG_H
#include "WaypointPositionSource.h"
#include "NavigationUIUtils.h"
#include "DialogGeometry.h"
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
std::vector<WaypointPosition> LoadOpenCpnWaypoints();
class WaypointPickerDialog : public wxDialog {
public:
  WaypointPickerDialog(wxWindow* parent,
                       const std::vector<WaypointPosition>& waypoints,
                       const wxString& selectedGuid)
      : wxDialog(parent, wxID_ANY, _("Select waypoint or place"),
                 wxDefaultPosition, wxSize(700, 500),
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        m_waypoints(waypoints),
        m_selectedGuid(selectedGuid),
        m_selectedIndex(-1) {
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(
                  this, wxID_ANY,
                  _("Choose an OpenCPN mark, waypoint or named route point.")),
              0, wxALL | wxEXPAND, 8);
    m_filter = new wxSearchCtrl(this, wxID_ANY);
    m_filter->SetDescriptiveText(_("Filter by name or coordinates"));
    root->Add(m_filter, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_list->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 300);
    m_list->InsertColumn(1, _("Latitude"), wxLIST_FORMAT_LEFT, 130);
    m_list->InsertColumn(2, _("Longitude"), wxLIST_FORMAT_LEFT, 130);
    root->Add(m_list, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

    wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
    m_ok = new wxButton(this, wxID_OK);
    m_ok->SetLabel(_("Use Waypoint"));
    m_ok->SetDefault();
    m_ok->Enable(false);
    buttons->AddButton(m_ok);
    buttons->AddButton(new wxButton(this, wxID_CANCEL));
    buttons->Realize();
    root->Add(buttons, 0, wxALL | wxEXPAND, 8);
    SetSizer(root);
    SetMinSize(wxSize(560, 400));
    dialog_geometry::Restore(this, _T("WaypointPicker"), wxSize(700, 500));

    m_filter->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RebuildList(); });
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
      m_selectedIndex = static_cast<long>(event.GetData());
      m_ok->Enable(m_selectedIndex >= 0);
    });
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
      m_selectedIndex = static_cast<long>(event.GetData());
      if (m_selectedIndex >= 0) EndModal(wxID_OK);
    });
    RebuildList();
    m_filter->SetFocus();
  }

  ~WaypointPickerDialog() override {
    dialog_geometry::Save(this, _T("WaypointPicker"));
  }

  const WaypointPosition* GetSelectedWaypoint() const {
    if (m_selectedIndex < 0 ||
        static_cast<size_t>(m_selectedIndex) >= m_waypoints.size())
      return NULL;
    return &m_waypoints[static_cast<size_t>(m_selectedIndex)];
  }

private:
  void RebuildList() {
    const wxString filter = m_filter->GetValue().Lower();
    m_list->DeleteAllItems();
    m_selectedIndex = -1;
    m_ok->Enable(false);
    long selectedRow = -1;
    for (size_t index = 0; index < m_waypoints.size(); ++index) {
      const WaypointPosition& waypoint = m_waypoints[index];
      const wxString name =
          waypoint.name.empty() ? _("(Unnamed waypoint)") : waypoint.name;
      const wxString latitude = FormatNavigationAngle(
          waypoint.latitude, NavigationAngleKind::Latitude, true);
      const wxString longitude = FormatNavigationAngle(
          waypoint.longitude, NavigationAngleKind::Longitude, true);
      const wxString searchable =
          (name + " " + latitude + " " + longitude).Lower();
      if (!filter.empty() && searchable.Find(filter) == wxNOT_FOUND) continue;
      const long row = m_list->InsertItem(m_list->GetItemCount(), name);
      m_list->SetItem(row, 1, latitude);
      m_list->SetItem(row, 2, longitude);
      m_list->SetItemData(row, static_cast<long>(index));
      if (waypoint.guid == m_selectedGuid) selectedRow = row;
    }
    if (selectedRow >= 0) {
      m_selectedIndex = static_cast<long>(m_list->GetItemData(selectedRow));
      m_ok->Enable(true);
      m_list->SetItemState(selectedRow,
                           wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                           wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
      m_list->EnsureVisible(selectedRow);
    }
  }

  std::vector<WaypointPosition> m_waypoints;
  wxString m_selectedGuid;
  wxSearchCtrl* m_filter;
  wxListCtrl* m_list;
  wxButton* m_ok;
  long m_selectedIndex;
};
#endif

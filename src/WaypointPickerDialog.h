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
#ifdef __OCPN__ANDROID__
#include "AndroidWaypointCards.h"
#include <QSignalBlocker>
#endif
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

#ifdef __OCPN__ANDROID__
    GetHandle()->setProperty("cnDocumentSurface", true);
    m_cards = CN_WaypointCards(this, root);
#else
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_list->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 300);
    m_list->InsertColumn(1, _("Latitude"), wxLIST_FORMAT_LEFT, 130);
    m_list->InsertColumn(2, _("Longitude"), wxLIST_FORMAT_LEFT, 130);
    root->Add(m_list, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
#endif

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
#ifdef __OCPN__ANDROID__
    QObject::connect(m_cards, &QListWidget::currentItemChanged, m_cards,
                     [this](QListWidgetItem* item, QListWidgetItem*) {
      m_selectedIndex = item && (item->flags() & Qt::ItemIsEnabled)
          ? item->data(Qt::UserRole).toInt() : -1;
      m_ok->Enable(m_selectedIndex >= 0);
      if (m_selectedIndex >= 0)
        m_selectedGuid = m_waypoints[m_selectedIndex].guid;
    });
#else
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
      m_selectedIndex = static_cast<long>(event.GetData());
      m_ok->Enable(m_selectedIndex >= 0);
    });
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
      m_selectedIndex = static_cast<long>(event.GetData());
      if (m_selectedIndex >= 0) EndModal(wxID_OK);
    });
#endif
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
#ifdef __OCPN__ANDROID__
    const QSignalBlocker blocked(m_cards);
    m_cards->clear();
#else
    m_list->DeleteAllItems();
#endif
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
#ifdef __OCPN__ANDROID__
      const wxString text = name + "\n" + _("Latitude") + ": " + latitude +
          "\n" + _("Longitude") + ": " + longitude;
      const long row = m_cards->count();
      auto* item = new QListWidgetItem(QString::fromUtf8(text.utf8_str()),
                                      m_cards);
      item->setData(Qt::UserRole, static_cast<int>(index));
#else
      const long row = m_list->InsertItem(m_list->GetItemCount(), name);
      m_list->SetItem(row, 1, latitude);
      m_list->SetItem(row, 2, longitude);
      m_list->SetItemData(row, static_cast<long>(index));
#endif
      if (waypoint.guid == m_selectedGuid) selectedRow = row;
    }
    if (selectedRow >= 0) {
#ifdef __OCPN__ANDROID__
      m_selectedIndex = m_cards->item(selectedRow)->data(Qt::UserRole).toInt();
      m_cards->setCurrentRow(selectedRow);
      m_cards->scrollToItem(m_cards->currentItem());
      m_ok->Enable(true);
#else
      m_selectedIndex = static_cast<long>(m_list->GetItemData(selectedRow));
      m_ok->Enable(true);
      m_list->SetItemState(selectedRow,
                           wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                           wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
      m_list->EnsureVisible(selectedRow);
#endif
    }
#ifdef __OCPN__ANDROID__
    if (!m_cards->count()) {
      auto* empty = new QListWidgetItem(QString::fromUtf8(
          _("No matching waypoints or places.").utf8_str()), m_cards);
      empty->setFlags(Qt::NoItemFlags);
    }
#endif
  }

  std::vector<WaypointPosition> m_waypoints;
  wxString m_selectedGuid;
  wxSearchCtrl* m_filter;
#ifdef __OCPN__ANDROID__
  QListWidget* m_cards;
#else
  wxListCtrl* m_list;
#endif
  wxButton* m_ok;
  long m_selectedIndex;
};
#endif

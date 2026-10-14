#include "PlannerBodySelectionDialog.h"
#include "BodyCatalog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <algorithm>

PlannerBodySelectionDialog::PlannerBodySelectionDialog(
    wxWindow* parent, const std::vector<wxString>& selected)
    : wxDialog(parent, wxID_ANY, _("Custom planner bodies"),
               wxDefaultPosition, wxSize(650, 650),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* note = new wxStaticText(this, wxID_ANY,
      _("Choose bodies for the table, both sky plots and recommendations. "
        "Custom choices override the plot magnitude limit. Below-horizon "
        "switches still apply; visibility and Hc affect recommendations only."));
  note->Wrap(600);
  root->Add(note, 0, wxEXPAND | wxALL, 10);
  m_search = new wxSearchCtrl(this, wxID_ANY);
  m_search->SetName("PlannerBodySearch");
  m_search->SetDescriptiveText(_("Search bodies"));
  m_search->ShowCancelButton(true);
  root->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  m_list = new wxScrolledWindow(this);
  m_list->SetName("PlannerBodyList");
  m_list->SetScrollRate(0, 10);
  auto* listSizer = new wxBoxSizer(wxVERTICAL);
  for (const auto& title : {_("Sun and Moon"), _("Planets"), _("Stars")}) {
    auto* group = new wxStaticBoxSizer(wxVERTICAL, m_list, title);
    group->Add(new wxFlexGridSizer(0, 3, 6, 12), 0, wxEXPAND | wxALL, 8);
    m_groups.push_back(group);
    listSizer->Add(group, 0, wxEXPAND | wxALL, 4);
  }
  for (const auto& body : BodyCatalog::All()) {
    const unsigned group = body.kind == CelestialBodyKind::Star ? 2
        : body.kind == CelestialBodyKind::Planet ? 1 : 0;
    auto* checkbox = new wxCheckBox(m_groups[group]->GetStaticBox(), wxID_ANY, body.name);
    checkbox->SetName("PlannerBodyCheck_" + body.name);
    checkbox->SetValue(std::any_of(selected.begin(), selected.end(),
        [&body](const wxString& name) { return name.CmpNoCase(body.name) == 0; }));
    checkbox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { UpdateCount(); });
    m_groups[group]->GetItem(size_t(0))->GetSizer()->Add(checkbox, 0, wxEXPAND);
    m_entries.push_back({body.name, checkbox, group});
  }
  m_list->SetSizer(listSizer);
  root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
  auto* selectionRow = new wxBoxSizer(wxHORIZONTAL);
  auto* all = new wxButton(this, wxID_ANY, _("Select all"));
  auto* none = new wxButton(this, wxID_ANY, _("Clear all"));
  all->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SelectAll(true); });
  none->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SelectAll(false); });
  all->SetToolTip(_("Select every catalogue body, including bodies hidden by search."));
  none->SetToolTip(_("Clear every checkbox, including bodies hidden by search."));
  selectionRow->Add(all, 0, wxRIGHT, 8);
  selectionRow->Add(none, 0, wxRIGHT, 12);
  m_count = new wxStaticText(this, wxID_ANY, wxEmptyString);
  selectionRow->Add(m_count, 1, wxALIGN_CENTER_VERTICAL);
  root->Add(selectionRow, 0, wxEXPAND | wxALL, 10);
  auto* lunarNote = new wxStaticText(this, wxID_ANY,
      _("Lunar distances always use the Moon as their reference. Your selected "
        "companions replace the traditional lunar list."));
  lunarNote->Wrap(600);
  root->Add(lunarNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
  SetSizer(root);
  SetMinSize(wxSize(560, 430));
  m_search->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { UpdateSearch(); });
  m_search->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, [this](wxCommandEvent&) { m_search->SetValue(""); });
  UpdateCount();
  Layout();
  m_list->FitInside();
  CentreOnParent();
}

std::vector<wxString> PlannerBodySelectionDialog::Selection() const {
  std::vector<wxString> result;
  for (const auto& entry : m_entries)
    if (entry.checkbox->GetValue()) result.push_back(entry.body);
  return result;
}

void PlannerBodySelectionDialog::UpdateCount() {
  m_count->SetLabel(wxString::Format(_("%u of %u bodies selected"),
      unsigned(Selection().size()), unsigned(m_entries.size())));
}

void PlannerBodySelectionDialog::SelectAll(bool checked) {
  for (const auto& entry : m_entries) entry.checkbox->SetValue(checked);
  UpdateCount();
}

void PlannerBodySelectionDialog::UpdateSearch() {
  const wxString query = m_search->GetValue().Lower().Trim().Trim(false);
  std::vector<bool> visible(m_groups.size(), false);
  for (const auto& entry : m_entries) {
    const bool show = entry.body.Lower().Contains(query);
    entry.checkbox->Show(show);
    visible[entry.group] = visible[entry.group] || show;
  }
  for (size_t i = 0; i < m_groups.size(); ++i) {
    m_groups[i]->GetStaticBox()->Show(visible[i]);
    m_groups[i]->Layout();
  }
  m_list->Layout();
  m_list->FitInside();
  m_list->Scroll(0, 0);
}

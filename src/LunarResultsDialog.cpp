#include "LunarResultsDialog.h"

#include "Sight.h"
#include "SightDialog.h"
#include "CelestialNavigationDialog.h"
#include "UtcDateTime.h"

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <cmath>

LunarResultsDialog::LunarResultsDialog(wxWindow* parent, Sight& sight)
    : wxDialog(parent, wxID_ANY, _("Lunar-distance UTC recovery"),
               wxDefaultPosition, wxSize(790, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight) {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticText* explanation = new wxStaticText(
      this, wxID_ANY,
      _("The observed Moon-to-body distance has been cleared of dip, "
        "refraction, semidiameter and parallax, then matched against the "
        "offline ephemeris."));
  explanation->Wrap(740);
  root->Add(explanation, 0, wxALL | wxEXPAND, 10);

  m_status = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_status->Wrap(740);
  root->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_candidates = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
  m_candidates->InsertColumn(0, _("UTC candidate"));
  m_candidates->InsertColumn(1, _("Clock correction"));
  m_candidates->InsertColumn(2, _("LD cleared"));
  m_candidates->InsertColumn(3, _("Rate (arcmin/h)"));
  m_candidates->InsertColumn(4, _("Estimated UTC uncertainty"));
  root->Add(m_candidates, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);

  root->Add(new wxStaticText(this, wxID_ANY,
                             _("Position from the two measured altitudes")),
            0, wxLEFT | wxRIGHT | wxTOP, 10);
  m_positions = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                               wxSize(-1, 105),
                               wxLC_REPORT | wxLC_SINGLE_SEL);
  m_positions->InsertColumn(0, _("Candidate"));
  m_positions->InsertColumn(1, _("Latitude"));
  m_positions->InsertColumn(2, _("Longitude"));
  m_positions->InsertColumn(3, _("Distance from DR/boat"));
  root->Add(m_positions, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);

  wxStaticText* warning = new wxStaticText(
      this, wxID_ANY,
      _("The lunar distance determines the constant watch offset. The two "
        "accompanying corrected altitudes can also intersect to give position "
        "and longitude; a rough DR/hemisphere chooses between the two "
        "mathematical intersections."));
  warning->Wrap(740);
  root->Add(warning, 0, wxALL | wxEXPAND, 10);

  root->Add(new wxStaticLine(this), 0, wxLEFT | wxRIGHT | wxEXPAND, 10);
  root->Add(new wxStaticText(this, wxID_ANY, _("Calculation details")), 0,
            wxLEFT | wxRIGHT | wxTOP, 10);
  m_details = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(-1, 190),
                             wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  root->Add(m_details, 0, wxALL | wxEXPAND, 10);

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  m_applyOffset = new wxButton(this, wxID_ANY,
                               _("Apply selected watch offset to all sights"));
  buttons->AddButton(m_applyOffset);
  buttons->AddButton(new wxButton(this, wxID_CLOSE, _("Close")));
  buttons->Realize();
  root->Add(buttons, 0, wxALL | wxALIGN_RIGHT, 10);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); },
       wxID_CLOSE);
  m_applyOffset->Bind(wxEVT_BUTTON,
                      &LunarResultsDialog::ApplySelectedWatchOffset, this);

  SetSizer(root);
  SetMinSize(wxSize(650, 460));
  CentreOnParent();
  UpdateResults();
}

void LunarResultsDialog::UpdateResults() {
  m_candidates->DeleteAllItems();
  m_positions->DeleteAllItems();
  m_details->SetValue(m_sight.m_CalcStr);
  if (!m_sight.m_LunarSolutionValid) {
    m_status->SetLabel(_("No UTC solution: ") +
                       m_sight.m_LunarSolutionError);
  } else {
    m_status->SetLabel(wxString::Format(
        m_sight.m_LunarCandidates.size() == 1
            ? _("One UTC solution was found in the selected search interval.")
            : _("%zu possible UTC solutions were found. Select using an "
                "approximate date/time or a second lunar distance."),
        m_sight.m_LunarCandidates.size()));
  }

  std::size_t selected = 0;
  for (std::size_t index = 1; index < m_sight.m_LunarCandidates.size(); ++index)
    if (std::fabs(m_sight.m_LunarCandidates[index].offset_seconds) <
        std::fabs(m_sight.m_LunarCandidates[selected].offset_seconds))
      selected = index;

  for (std::size_t index = 0; index < m_sight.m_LunarCandidates.size(); ++index) {
    const lunar_distance::TimeCandidate& candidate =
        m_sight.m_LunarCandidates[index];
    const wxDateTime utc = UtcDateTime::AddSeconds(
        m_sight.m_CorrectedDateTime, candidate.offset_seconds);
    const long row = m_candidates->InsertItem(
        static_cast<long>(index),
        UtcDateTime::FormatUtc(utc, "%Y-%m-%d %H:%M:%S.%l"));
    m_candidates->SetItem(
        row, 1,
        wxString::Format("%+.1f s", candidate.offset_seconds));
    m_candidates->SetItem(
        row, 2,
        wxString::Format("%.6f%c", candidate.cleared_distance_deg, 0x00B0));
    m_candidates->SetItem(
        row, 3, wxString::Format("%.3f", candidate.slope_arcmin_per_hour));
    m_candidates->SetItem(
        row, 4,
        std::isfinite(candidate.time_uncertainty_seconds)
            ? wxString::Format("%.1f s (1-sigma)",
                               candidate.time_uncertainty_seconds)
            : _("Indeterminate"));
    if (index == selected)
      m_candidates->SetItemState(row, wxLIST_STATE_SELECTED,
                                 wxLIST_STATE_SELECTED);
  }
  for (int column = 0; column < 5; ++column)
    m_candidates->SetColumnWidth(column, wxLIST_AUTOSIZE_USEHEADER);
  m_applyOffset->Enable(m_sight.m_LunarSolutionValid &&
                        !m_sight.m_LunarCandidates.empty());

  if (m_sight.m_LunarPositionResult.valid) {
    const lunar_distance::GeographicPoint approximate{m_sight.m_DRLat,
                                                       m_sight.m_DRLon};
    for (std::size_t index = 0;
         index < m_sight.m_LunarPositionResult.candidates.size(); ++index) {
      const auto& position = m_sight.m_LunarPositionResult.candidates[index];
      const long row = m_positions->InsertItem(
          static_cast<long>(index),
          static_cast<int>(index) == m_sight.m_LunarSelectedPosition
              ? wxString::Format(_("%zu (nearest DR)"), index + 1)
              : wxString::Format(_("%zu"), index + 1));
      m_positions->SetItem(row, 1,
                           wxString::Format("%.6f%c", position.latitude_deg,
                                            0x00B0));
      m_positions->SetItem(row, 2,
                           wxString::Format("%.6f%c", position.longitude_deg,
                                            0x00B0));
      m_positions->SetItem(
          row, 3,
          wxString::Format("%.1f NM",
                           lunar_distance::GreatCircleDistanceNm(
                               approximate, position)));
    }
  } else {
    m_positions->InsertItem(
        0, _("No intersection: ") +
               wxString::FromUTF8(m_sight.m_LunarPositionResult.error.c_str()));
  }
  for (int column = 0; column < 4; ++column)
    m_positions->SetColumnWidth(column, wxLIST_AUTOSIZE_USEHEADER);
}

void LunarResultsDialog::ApplySelectedWatchOffset(wxCommandEvent&) {
  long selected = m_candidates->GetNextItem(
      -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selected < 0 ||
      static_cast<std::size_t>(selected) >= m_sight.m_LunarCandidates.size()) {
    wxMessageBox(_("Select a UTC candidate first."), _("Lunar distance"),
                 wxOK | wxICON_INFORMATION, this);
    return;
  }
  SightDialog* sightDialog = dynamic_cast<SightDialog*>(GetParent());
  CelestialNavigationDialog* mainDialog =
      sightDialog
          ? dynamic_cast<CelestialNavigationDialog*>(sightDialog->GetParent())
          : nullptr;
  if (!sightDialog || !mainDialog) return;
  const int residual = static_cast<int>(std::lround(
      m_sight.m_LunarCandidates[static_cast<std::size_t>(selected)]
          .offset_seconds));
  const int corrected = mainDialog->GetClockCorrection() + residual;
  if (wxMessageBox(
          wxString::Format(
              _("Set the constant sight/watch correction to %+d seconds "
                "and recompute every saved sight? The recorded watch readings "
                "will not be changed."),
              corrected),
          _("Apply recovered watch offset"), wxYES_NO | wxICON_QUESTION,
          this) != wxYES)
    return;
  mainDialog->ApplyClockCorrection(corrected);
  sightDialog->SetClockOffset(corrected);
  UpdateResults();
}

#include "LunarResultsDialog.h"

#include "DialogGeometry.h"
#include "Sight.h"
#include "SightDialog.h"
#include "CelestialNavigationDialog.h"
#include "NavigationUIUtils.h"
#include "UtcDateTime.h"

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>

#include <cmath>

LunarResultsDialog::LunarResultsDialog(wxWindow* parent, Sight& sight)
    : wxDialog(parent, wxID_ANY, _("Lunar-distance UTC recovery"),
               wxDefaultPosition, wxSize(940, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight) {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxNotebook* pages = new wxNotebook(this, wxID_ANY);
  wxPanel* resultsPage = new wxPanel(pages);
  wxBoxSizer* results = new wxBoxSizer(wxVERTICAL);
  wxStaticText* explanation = new wxStaticText(
      resultsPage, wxID_ANY,
      m_sight.m_LunarSeparateTimes
          ? _("The lunar distance and the two altitudes are evaluated at "
              "their individual watch times. Their shared constant watch "
              "offset and the reference-epoch position are solved jointly "
              "against the offline ephemeris.")
          : _("The WGS84 model solves UTC and position from the lunar distance "
              "and two altitudes, including limb contact, refraction and "
              "parallax. "
              "Dip corrects horizon altitudes only. Recorded measurements are "
              "retained."));
  explanation->Wrap(740);
  results->Add(explanation, 0, wxALL | wxEXPAND, 10);
  if (m_sight.m_LunarDut1Fallback) {
    auto* warning = new wxStaticText(resultsPage, wxID_ANY,
        _("Earth-rotation data do not cover all evaluated dates. Calculations remain available offline using UT1=UTC; accuracy is reduced and formal uncertainty does not include this approximation."));
    warning->Wrap(740);
    results->Add(warning, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  }
  m_mode = new wxChoice(resultsPage, wxID_ANY);
  m_mode->Append(m_sight.m_LunarSeparateTimes
                     ? _("Recover UTC and position at individual reading times")
                     : _("Recover UTC from the lunar distance"));
  m_mode->Append(
      _("Check at entered UTC (including existing clock correction)"));
  m_mode->SetSelection(0);
  m_mode->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { UpdateResults(); });
  results->Add(m_mode, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_status = new wxStaticText(resultsPage, wxID_ANY, wxEmptyString);
  m_status->Wrap(740);
  results->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_candidates = new wxListCtrl(resultsPage, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
  m_candidates->InsertColumn(0, _("UTC candidate"));
  m_candidates->InsertColumn(1, _("Clock correction"));
  m_candidates->InsertColumn(2, _("Model centre distance"));
  m_candidates->InsertColumn(3, _("Rate (arcmin/h)"));
  m_candidates->InsertColumn(4, _("Estimated UTC uncertainty"));
  results->Add(m_candidates, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);

  results->Add(new wxStaticText(
                resultsPage, wxID_ANY,
                m_sight.m_LunarSeparateTimes
                    ? _("Position at the lunar-distance reading time")
                    : _("Position from the two measured altitudes")),
            0, wxLEFT | wxRIGHT | wxTOP, 10);
  m_positions = new wxListCtrl(resultsPage, wxID_ANY, wxDefaultPosition,
                               wxSize(-1, 105),
                               wxLC_REPORT | wxLC_SINGLE_SEL);
  m_positions->InsertColumn(0, _("Candidate"));
  m_positions->InsertColumn(1, _("Latitude"));
  m_positions->InsertColumn(2, _("Longitude"));
  m_positions->InsertColumn(3, _("Distance from saved sight DR"));
  m_positions->InsertColumn(4, _("Estimated position uncertainty"));
  results->Add(m_positions, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);
  m_geometry = new wxStaticText(resultsPage, wxID_ANY, wxEmptyString);
  results->Add(m_geometry, 0, wxALL | wxEXPAND, 10);

  wxStaticText* warning = new wxStaticText(
      resultsPage, wxID_ANY,
      m_sight.m_LunarSeparateTimes
          ? _("All three watch readings retain their measured intervals and "
              "receive the same recovered UTC correction. If motion is "
              "enabled, COG/SOG advances the observer between readings. A "
              "rough DR or hemisphere still helps select among mathematical "
              "solutions. Results use the WGS84 ellipsoid; formal uncertainty "
              "does not include systematic errors.")
          : _("The lunar distance determines the constant watch offset. The "
              "two accompanying corrected altitudes can also intersect to "
              "give position and longitude; a rough DR/hemisphere chooses "
              "between the two mathematical intersections. All results use a "
              "WGS84 ellipsoid. Position uncertainty is horizontal RMS, not a "
              "68% confidence-circle radius. Systematic errors are excluded."));
  warning->Wrap(740);
  results->Add(warning, 0, wxALL | wxEXPAND, 10);
  resultsPage->SetSizer(results);
  pages->AddPage(resultsPage, _("Results"), true);

  wxPanel* calculationsPage = new wxPanel(pages);
  wxBoxSizer* calculations = new wxBoxSizer(wxVERTICAL);
  wxStaticText* calculationNote = new wxStaticText(
      calculationsPage, wxID_ANY,
      m_sight.m_LunarSeparateTimes
          ? _("Auditable time-tagged UTC/position working. This page shows "
              "the observations, forward-model evaluations, root refinement, "
              "position solutions and warnings used to produce Results.")
          : _("Auditable WGS84 solution with a Direct Triangle comparison. "
              "This page shows the "
              "observation reductions, formula substitutions, ephemeris "
              "scan, root refinement, position intersections and warnings "
              "used to produce Results."));
  calculationNote->Wrap(740);
  calculations->Add(calculationNote, 0, wxALL | wxEXPAND, 10);
  m_details = new wxTextCtrl(
      calculationsPage, wxID_ANY, wxEmptyString, wxDefaultPosition,
      wxDefaultSize,
      wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
  m_details->SetFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
  calculations->Add(m_details, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  calculationsPage->SetSizer(calculations);
  pages->AddPage(calculationsPage, _("Calculations"), false);
  root->Add(pages, 1, wxEXPAND | wxALL, 6);

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  m_applyOffset = new wxButton(this, wxID_ANY, _("Save lunar solution"));
  buttons->AddButton(m_applyOffset);
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  buttons->AddButton(close);
  buttons->Realize();
  root->Add(buttons, 0, wxALL | wxALIGN_RIGHT, 10);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); },
       wxID_CLOSE);
  Bind(wxEVT_CLOSE_WINDOW,
       [this](wxCloseEvent&) { EndModal(wxID_CLOSE); });
  SetEscapeId(wxID_CLOSE);
  m_applyOffset->Bind(wxEVT_BUTTON,
                      &LunarResultsDialog::ApplySelectedWatchOffset, this);
  m_candidates->Bind(wxEVT_LIST_ITEM_SELECTED,
                     [this](wxListEvent& event) {
                       UpdatePositions(event.GetIndex());
                     });

  SetSizer(root);
  SetMinSize(wxSize(650, 460));
  dialog_geometry::Restore(this, _T("LunarResults"), wxSize(940, 700));
  UpdateResults();
}

LunarResultsDialog::~LunarResultsDialog() {
  dialog_geometry::Save(this, _T("LunarResults"));
}

void LunarResultsDialog::UpdateResults() {
  m_candidates->DeleteAllItems();
  m_positions->DeleteAllItems();
  const bool checking = m_mode->GetSelection() == 1;
  wxListItem column;
  column.SetText(checking ? _("Model - observed (arcmin)")
                          : _("Additional clock correction"));
  m_candidates->SetColumn(1, column);
  if (checking) {
    lunar_distance::EphemerisSample sample;
    std::string error;
    m_applyOffset->Enable(false);
    if (!m_sight.LunarEphemeris() ||
        !m_sight.LunarEphemeris()(0, &sample, &error)) {
      m_status->SetLabel(_("Cannot evaluate entered UTC: ") +
                         wxString::FromUTF8(error.c_str()));
      m_details->Clear();
      return;
    }
    const auto observation = m_sight.LunarObservation();
    const auto position = lunar_distance::PositionAtTime(
        observation, m_sight.LunarEphemeris(), 0);
    double residual = NAN;
    double distance = sample.predicted_distance_deg;
    if (!observation.separate_times && !observation.use_ellipsoid) {
      const auto cleared = lunar_distance::ClearDistance(observation, sample);
      if (cleared.valid) {
        distance = cleared.cleared_distance_deg;
        residual = 60 * (sample.predicted_distance_deg - distance);
      }
    } else if (position.valid) {
      auto nearest = position.candidates.front();
      const lunar_distance::GeographicPoint dr(m_sight.m_DRLat,
                                               m_sight.m_DRLon);
      for (const auto& p : position.candidates)
        if (lunar_distance::GreatCircleDistanceNm(dr, p) <
            lunar_distance::GreatCircleDistanceNm(dr, nearest))
          nearest = p;
      const auto predicted = lunar_distance::PredictTimeTaggedObservation(
          observation, m_sight.LunarEphemeris(), 0, nearest);
      if (predicted.valid)
        residual =
            60 * (predicted.raw_distance_deg - observation.raw_distance_deg);
    }
    m_status->SetLabel(
        _("Checking the entered UTC and existing correction. No lunar-derived "
          "time shift is used. "
          "The distance residual uses the nearest saved-DR position branch; "
          "branch residuals are shown below."));
    m_status->Wrap(740);
    m_candidates->InsertItem(0,
                             UtcDateTime::FormatUtc(m_sight.m_CorrectedDateTime,
                                                    "%Y-%m-%d %H:%M:%S.%l"));
    m_candidates->SetItem(0, 1,
                          std::isfinite(residual)
                              ? wxString::Format("%+.6f", residual)
                              : _("Indeterminate"));
    m_candidates->SetItem(0, 2, FormatNavigationAngle(distance));
    m_candidates->SetItem(0, 3, _("Not solving UTC"));
    m_candidates->SetItem(0, 4, _("Not estimated"));
    for (int c = 0; c < 5; ++c)
      m_candidates->SetColumnWidth(c, wxLIST_AUTOSIZE_USEHEADER);
    m_details->SetValue(
        _("CHECK AT ENTERED UTC\n") + LunarInputSnapshot(m_sight) +
        wxString::Format("\nUTC: %s\nEphemeris centre distance: %.9f "
                         "deg\nDistance residual: %+.6f arcmin\n"
                         "WGS84 ellipsoid. No additional lunar clock "
                         "correction has been solved or applied.\n",
                         UtcDateTime::FormatUtc(m_sight.m_CorrectedDateTime,
                                                "%Y-%m-%d %H:%M:%S"),
                         sample.predicted_distance_deg, residual));
    UpdatePositions(-1);
    return;
  }
  m_details->SetValue(m_sight.m_CalcStr);
  if (!m_sight.m_LunarSolutionValid) {
    m_status->SetLabel(_("No UTC solution: ") +
                       m_sight.m_LunarSolutionError);
  } else {
    m_status->SetLabel(wxString::Format(
        m_sight.m_LunarCandidates.size() == 1
            ? _("One UTC solution was found in the selected search interval.")
            : _("%zu possible UTC solutions were found. The default uses "
                "proximity to DR when available, otherwise proximity to entered "
                "UTC. Check the DR and all candidates; use another observation "
                "if the choice remains ambiguous."),
        m_sight.m_LunarCandidates.size()));
  }

  const int selected = m_sight.SelectLunarCandidate(
      m_sight.m_LunarSelectedCandidate);

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
        row, 2, FormatNavigationAngle(candidate.cleared_distance_deg));
    m_candidates->SetItem(
        row, 3, wxString::Format("%.3f", candidate.slope_arcmin_per_hour));
    m_candidates->SetItem(
        row, 4,
        std::isfinite(candidate.time_uncertainty_seconds)
            ? wxString::Format("%.1f s (1-sigma)",
                               candidate.time_uncertainty_seconds)
            : _("Indeterminate"));
    if (static_cast<int>(index) == selected)
      m_candidates->SetItemState(row, wxLIST_STATE_SELECTED,
                                 wxLIST_STATE_SELECTED);
  }
  for (int column = 0; column < 5; ++column)
    m_candidates->SetColumnWidth(column, wxLIST_AUTOSIZE_USEHEADER);
  m_applyOffset->Enable(m_sight.m_LunarSolutionValid &&
                        !m_sight.m_LunarCandidates.empty());

  UpdatePositions(static_cast<long>(selected));
}

void LunarResultsDialog::UpdatePositions(long candidate_index) {
  m_positions->DeleteAllItems();
  const bool checking = m_mode->GetSelection() == 1;
  wxListItem uncertainty_column;
  uncertainty_column.SetText(checking
                                 ? _("Model - observed LD (arcmin)")
                                 : _("Position uncertainty (horizontal RMS)"));
  m_positions->SetColumn(4, uncertainty_column);
  lunar_distance::PositionResult check_position;
  if (checking)
    check_position = lunar_distance::PositionAtTime(
        m_sight.LunarObservation(), m_sight.LunarEphemeris(), 0);
  const std::vector<lunar_distance::GeographicPoint>* positions = nullptr;
  const lunar_distance::TimeCandidate* time_candidate = nullptr;
  if (!checking && candidate_index >= 0 &&
      static_cast<std::size_t>(candidate_index) <
          m_sight.m_LunarCandidates.size()) {
    m_sight.RecomputeLunar(static_cast<int>(candidate_index));
    m_details->SetValue(m_sight.m_CalcStr);
    time_candidate =
        &m_sight.m_LunarCandidates[static_cast<std::size_t>(candidate_index)];
    m_applyOffset->Enable(true);
    if (!time_candidate->positions.empty()) positions = &time_candidate->positions;
  }
  if (checking && check_position.valid) positions = &check_position.candidates;
  if (!checking && !positions && m_sight.m_LunarPositionResult.valid) {
    positions = &m_sight.m_LunarPositionResult.candidates;
  }
  const double crossing = checking ? check_position.circle_crossing_angle_deg
                          : time_candidate
                              ? time_candidate->circle_crossing_angle_deg
                              : 0;
  m_geometry->SetLabel(
      m_sight.LunarObservation().use_ellipsoid || m_sight.m_LunarSeparateTimes
          ? _("Position belongs to this UTC candidate at the distance-reading "
              "epoch. Review alternate branches and residuals.")
          : wxString::Format(
                _("Altitude-circle crossing: %.2f degrees. %s"), crossing,
                crossing < 15 ? _("Weak position geometry: additional "
                                  "independent sights are needed.")
                              : _("This describes position geometry, not lunar "
                                  "time accuracy.")));
  m_geometry->Wrap(740);
  if (positions) {
    const lunar_distance::GeographicPoint approximate{m_sight.m_DRLat,
                                                       m_sight.m_DRLon};
    std::size_t nearest = 0;
    double nearest_distance = INFINITY;
    for (std::size_t index = 0; index < positions->size(); ++index) {
      const double distance = lunar_distance::GreatCircleDistanceNm(
          approximate, (*positions)[index]);
      if (distance < nearest_distance) {
        nearest_distance = distance;
        nearest = index;
      }
    }
    for (std::size_t index = 0; index < positions->size(); ++index) {
      const auto& position = (*positions)[index];
      const long row = m_positions->InsertItem(
          static_cast<long>(index),
          index == nearest
              ? wxString::Format(_("%zu (nearest DR)"), index + 1)
              : wxString::Format(_("%zu"), index + 1));
      m_positions->SetItem(
          row, 1,
          FormatNavigationAngle(position.latitude_deg,
                                NavigationAngleKind::Latitude, true));
      m_positions->SetItem(
          row, 2,
          FormatNavigationAngle(position.longitude_deg,
                                NavigationAngleKind::Longitude, true));
      m_positions->SetItem(
          row, 3,
          wxString::Format("%.1f NM",
                           lunar_distance::GreatCircleDistanceNm(
                               approximate, position)));
      m_positions->SetItem(
          row, 4,
          time_candidate &&
                  std::isfinite(time_candidate->position_uncertainty_nm) &&
                  time_candidate->position_uncertainty_nm > 0.0
              ? wxString::Format("%.2f NM (1-sigma)",
                                 time_candidate->position_uncertainty_nm)
              : _("Not estimated"));
      if (checking) {
        const auto predicted = lunar_distance::PredictTimeTaggedObservation(
            m_sight.LunarObservation(), m_sight.LunarEphemeris(), 0, position);
        const double residual =
            predicted.valid
                ? 60.0 * (predicted.raw_distance_deg - m_sight.m_Measurement)
                : NAN;
        m_positions->SetItem(row, 4,
                             std::isfinite(residual)
                                 ? wxString::Format("%+.6f", residual)
                                 : _("Indeterminate"));
        m_details->AppendText(wxString::Format(
            "Branch %zu: %.8f, %.8f; model-observed LD %+.6f arcmin\n",
            index + 1, position.latitude_deg, position.longitude_deg,
            residual));
      }
    }
  } else {
    m_positions->InsertItem(
        0,
        _("No intersection: ") +
            wxString::FromUTF8((checking ? check_position.error
                                         : m_sight.m_LunarPositionResult.error)
                                   .c_str()));
  }
  for (int column = 0; column < 5; ++column)
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
  const auto& candidate = m_sight.m_LunarCandidates[selected];
  LunarSolutionRecord record;
  record.reference_time =
      UtcDateTime::FormatUtc(m_sight.m_DateTime, "%Y-%m-%d %H:%M:%S");
  record.method = m_sight.m_LunarSeparateTimes
                      ? "WGS84 time-tagged UTC/position"
                      : "WGS84 simultaneous UTC/position";
  record.base_correction_seconds = mainDialog->GetClockCorrection();
  record.additional_correction_seconds = candidate.offset_seconds;
  record.time_sigma_seconds = candidate.time_uncertainty_seconds;
  record.inputs.push_back(LunarInputSnapshot(m_sight));
  record.report =
      wxString::Format("Selected UTC candidate: %ld\n", selected + 1) +
      m_details->GetValue();
  if (mainDialog->SaveLunarSolution(record)) m_applyOffset->Enable(false);
}

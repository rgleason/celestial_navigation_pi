#include "PlatformMessageBox.h"
#ifdef __OCPN__ANDROID__
#include "AndroidJob.h"
#include <QAbstractItemView>
#endif
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
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>

#include <cmath>

namespace {
#ifdef __OCPN__ANDROID__
wxString AndroidResultCell(wxListCtrl* list, long row, int column) {
  auto* view = qobject_cast<QAbstractItemView*>(list->GetHandle());
  if (!view) view = list->GetHandle()->findChild<QAbstractItemView*>();
  if (!view || !view->model()) return wxEmptyString;
  return wxString::FromUTF8(view->model()->index(row, column)
      .data(Qt::DisplayRole).toString().toUtf8().constData());
}
#endif
bool HasSavedDr(const Sight& sight) {
  return std::isfinite(sight.m_DRLat) && std::isfinite(sight.m_DRLon) &&
         std::fabs(sight.m_DRLat) <= 90.0 &&
         std::fabs(sight.m_DRLon) <= 180.0 &&
         (sight.m_DRLat != 0.0 || sight.m_DRLon != 0.0);
}

wxString GeometryAssessment(double effective) {
  if (effective < 15.0)
    return _("Very weak geometry. Latitude and longitude are retained, but "
             "their errors are strongly correlated; use an independent "
             "latitude or another well-separated sight.");
  if (effective < 30.0)
    return _("Weak geometry. The retained position has an elongated, "
             "correlated uncertainty; another well-separated sight is "
             "recommended.");
  if (effective < 60.0)
    return _("Moderate geometry. The retained position is usable with "
             "greater uncertainty along one direction.");
  return _("Good geometry. This describes the position solution, not lunar "
           "time accuracy.");
}
}  // namespace

LunarResultsDialog::LunarResultsDialog(wxWindow* parent, Sight& sight)
    : wxDialog(parent, wxID_ANY, _("Lunar-distance UTC recovery"),
               wxDefaultPosition, wxSize(1040, 720),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight) {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxNotebook* pages = new wxNotebook(this, wxID_ANY);
  auto* resultsPage = new wxScrolledWindow(pages);
  resultsPage->SetScrollRate(10, 10);
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

  m_accuracyPanel = new wxPanel(resultsPage);
  auto* accuracy = new wxBoxSizer(wxVERTICAL);
  auto* explanationCheck = new wxStaticText(
      m_accuracyPanel, wxID_ANY,
      _("Lunar sight accuracy: enter an independently known position at the "
        "distance-reading time and use a reliable UTC. Saved DR fills these "
        "fields initially; confirm it before judging the sight. Position and "
        "UTC are not fitted from this lunar."));
  explanationCheck->Wrap(740);
  accuracy->Add(explanationCheck, 0, wxALL | wxEXPAND, 5);
  auto* positionRow = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  m_knownLatitude = new NavigationAngleCtrl(
      m_accuracyPanel, NavigationAngleKind::Latitude, m_sight.m_DRLat, -90, 90);
  m_knownLongitude =
      new NavigationAngleCtrl(m_accuracyPanel, NavigationAngleKind::Longitude,
                              m_sight.m_DRLon, -180, 180);
  m_knownLatitude->SetName("LunarKnownLatitude");
  m_knownLongitude->SetName("LunarKnownLongitude");
  if (!HasSavedDr(m_sight)) {
    m_knownLatitude->ChangeValue("");
    m_knownLongitude->ChangeValue("");
  }
  for (const auto& field :
       {std::make_pair(_("Known latitude"), m_knownLatitude),
        std::make_pair(_("Known longitude"), m_knownLongitude)}) {
    positionRow->Add(new wxStaticText(m_accuracyPanel, wxID_ANY, field.first),
                     0, wxALL |
#ifdef __OCPN__ANDROID__
                            wxEXPAND,
#else
                            wxALIGN_CENTER_VERTICAL,
#endif
                     5);
    positionRow->Add(field.second,
#ifdef __OCPN__ANDROID__
                     0,
#else
                     1,
#endif
                     wxALL | wxEXPAND, 5);
    field.second->Bind(wxEVT_TEXT,
                       [this](wxCommandEvent&) { UpdateResults(); });
  }
  accuracy->Add(positionRow, 0, wxEXPAND);
  m_distanceCheck = new wxStaticText(m_accuracyPanel, wxID_ANY, wxEmptyString);
  m_distanceCheck->SetName("LunarDistanceCheck");
  accuracy->Add(m_distanceCheck, 0, wxALL | wxEXPAND, 5);
  m_accuracyPanel->SetSizer(accuracy);
  results->Add(m_accuracyPanel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_status = new wxStaticText(resultsPage, wxID_ANY, wxEmptyString);
  m_status->Wrap(880);
  results->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_dr = new wxStaticText(resultsPage, wxID_ANY, wxEmptyString);
  if (HasSavedDr(m_sight)) {
    m_dr->SetLabel(wxString::Format(
        _("Saved DR used to rank solution branches: %s, %s"),
        FormatNavigationAngle(m_sight.m_DRLat,
                              NavigationAngleKind::Latitude, true),
        FormatNavigationAngle(m_sight.m_DRLon,
                              NavigationAngleKind::Longitude, true)));
  } else {
    m_dr->SetLabel(_("Saved DR used to rank solution branches: not available; "
                     "review all mathematical branches."));
  }
  m_dr->Wrap(880);
  results->Add(m_dr, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

  m_candidates = new wxListCtrl(resultsPage, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL);
  m_candidates->InsertColumn(0, _("UTC candidate"));
  m_candidates->InsertColumn(1, _("Correction"));
  m_candidates->InsertColumn(
      2, m_sight.m_LunarSeparateTimes ? _("Predicted geocentric distance")
                                      : _("LD cleared"));
  m_candidates->InsertColumn(3, _("Local rate"));
  m_candidates->InsertColumn(4, _("UTC uncertainty"));
#ifdef __OCPN__ANDROID__
  m_candidates->Hide();
  m_androidCandidates = new wxPanel(resultsPage);
  m_androidCandidates->SetSizer(new wxBoxSizer(wxVERTICAL));
  results->Add(m_androidCandidates, 0, wxALL | wxEXPAND, 8);
#else
  m_candidates->SetMinSize(wxSize(-1, std::max(130, 6 * GetCharHeight())));
  results->Add(m_candidates, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);
#endif

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
  m_positions->InsertColumn(3, _("From saved DR"));
  m_positions->InsertColumn(4, _("Position uncertainty"));
#ifdef __OCPN__ANDROID__
  m_positions->Hide();
  m_androidPositions = new wxPanel(resultsPage);
  m_androidPositions->SetSizer(new wxBoxSizer(wxVERTICAL));
  results->Add(m_androidPositions, 0, wxALL | wxEXPAND, 8);
#else
  results->Add(m_positions, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);
#endif
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

  wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
  m_applyOffset = new wxButton(this, wxID_ANY, _("Save lunar solution"));
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  buttons->AddStretchSpacer();
  buttons->Add(m_applyOffset, 0, wxRIGHT, 8);
  buttons->Add(close, 0);
  root->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
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
  SetMinSize(wxSize(760, 500));
  dialog_geometry::Restore(this, _T("LunarResults"), wxSize(1040, 720));
  UpdateResults();
#ifdef __OCPN__ANDROID__
  close->Hide();
  celestial_android::Decorate(this, GetTitle(), [this]() { EndModal(wxID_CLOSE); });
#endif
}

LunarResultsDialog::~LunarResultsDialog() {
  dialog_geometry::Save(this, _T("LunarResults"));
}

void LunarResultsDialog::UpdateResults() {
#ifdef __OCPN__ANDROID__
  RefreshAndroidCards();
#endif
  m_candidates->DeleteAllItems();
  m_positions->DeleteAllItems();
  const bool checking = m_mode->GetSelection() == 1;
  m_accuracyPanel->Show(checking);
  Layout();
  if (auto* scrolled = dynamic_cast<wxScrolledWindow*>(m_accuracyPanel->GetParent()))
    scrolled->FitInside();
  wxListItem column;
  column.SetText(checking ? _("Lunar error (arcmin)")
                          : _("Additional clock correction"));
  m_candidates->SetColumn(1, column);
  column.SetText(checking ? _("True LD")
      : m_sight.m_LunarSeparateTimes ? _("Predicted geocentric distance") : _("LD cleared"));
  m_candidates->SetColumn(2, column);
  if (checking) {
    lunar_distance::EphemerisSample sample;
    std::string error;
    m_applyOffset->Enable(false);
    if (!m_sight.LunarEphemeris() ||
        !m_sight.LunarEphemeris()(0, &sample, &error)) {
      m_status->SetLabel(_("Cannot evaluate entered UTC: ") +
                         wxString::FromUTF8(error.c_str()));
      m_distanceCheck->SetLabel(
          _("True LD: unavailable\nCleared LD: unavailable\nLunar error: "
            "unavailable"));
      m_details->Clear();
      return;
    }
    const auto observation = m_sight.LunarObservation();
    double residual = NAN;
    double latitude, longitude;
    if (m_knownLatitude->GetAngle(&latitude) &&
        m_knownLongitude->GetAngle(&longitude)) {
      const auto check = lunar_distance::CheckDistanceAtPosition(
          observation, m_sight.LunarEphemeris(), {latitude, longitude});
      if (check.valid) {
        residual = check.lunar_error_arcmin;
        m_distanceCheck->SetLabel(wxString::Format(
            _("True LD: %s\nCleared LD (model corrected): %s\n"
              "Lunar error (cleared - true): %+.4f arcmin\n"
              "Positive error means the observed distance is too large. "
              "Clearing subtracts the forward model's index, limb, refraction "
              "and parallax correction at the known position. "
              "Measured altitudes are retained for the position solution; "
              "they are not used to fit this accuracy check."),
            FormatNavigationAngle(check.true_distance_deg),
            FormatNavigationAngle(check.cleared_distance_deg), residual));
      } else {
        m_distanceCheck->SetLabel(_("Lunar accuracy check unavailable: ") +
                                  wxString::FromUTF8(check.error.c_str()));
      }
    } else {
      m_distanceCheck->SetLabel(
          _("True LD: ") +
          FormatNavigationAngle(sample.predicted_distance_deg) +
          _("\nCleared LD and lunar error: enter a valid known latitude and "
            "longitude."));
    }
    m_distanceCheck->Wrap(740);
    m_status->SetLabel(
        _("Checking the entered UTC and existing correction. No lunar-derived "
          "time shift is used. "
          "The accuracy check uses only the entered known position. "
          "Position branches from the measured altitudes are shown separately "
          "below."));
    m_status->Wrap(740);
    m_candidates->InsertItem(0,
                             UtcDateTime::FormatUtc(m_sight.m_CorrectedDateTime,
                                                    "%Y-%m-%d %H:%M:%S.%l"));
    m_candidates->SetItem(0, 1,
                          std::isfinite(residual)
                              ? wxString::Format("%+.6f", residual)
                              : _("Indeterminate"));
    m_candidates->SetItem(0, 2,
                          FormatNavigationAngle(sample.predicted_distance_deg));
    m_candidates->SetItem(0, 3, _("Not solving UTC"));
    m_candidates->SetItem(0, 4, _("Not estimated"));
    for (int c = 0; c < 5; ++c)
      m_candidates->SetColumnWidth(c, wxLIST_AUTOSIZE_USEHEADER);
    m_details->SetValue(
        _("CHECK AT ENTERED UTC\n") + LunarInputSnapshot(m_sight) +
        wxString::Format("\nUTC: %s\nEphemeris centre distance: %.9f "
                         "deg\nLunar error (observed - model): %+.6f arcmin\n"
                         "WGS84 ellipsoid. No additional lunar clock "
                         "correction has been solved or applied.\n",
                         UtcDateTime::FormatUtc(m_sight.m_CorrectedDateTime,
                                                "%Y-%m-%d %H:%M:%S"),
                         sample.predicted_distance_deg, residual));
    m_details->AppendText("\n" + m_distanceCheck->GetLabel());
    UpdatePositions(-1);
    return;
  }
  m_details->SetValue(m_sight.m_CalcStr);
  if (!m_sight.m_LunarSolutionValid) {
    m_status->SetLabel(_("No UTC solution: ") + m_sight.m_LunarSolutionError);
  } else {
    m_status->SetLabel(wxString::Format(
        m_sight.m_LunarCandidates.size() == 1
            ? _("One UTC solution was found in the selected search interval.")
            : _("%zu possible UTC solutions were found. The default uses "
                "proximity to DR when available, otherwise proximity to "
                "entered "
                "UTC. Even closely spaced solutions are unresolved "
                "alternatives, "
                "not measurements to average. Check all candidates; use "
                "another "
                "observation or independent position/time evidence to "
                "distinguish them."),
        m_sight.m_LunarCandidates.size()));
  }
  m_status->Wrap(880);

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
        row, 3,
        candidate.local_slope_available && std::isfinite(candidate.slope_arcmin_per_hour)
            ? wxString::Format("%.2f%s/h (%.3f%s/min)%s",
                         candidate.slope_arcmin_per_hour,
                         wxString::FromUTF8("\xE2\x80\xB2"),
                         candidate.slope_arcmin_per_hour / 60.0,
                         wxString::FromUTF8("\xE2\x80\xB2"),
                         candidate.used_one_sided_slope ? _(" (one-sided)") : wxString())
            : _("Unavailable"));
    m_candidates->SetItem(
        row, 4,
        candidate.uncertainty_available && std::isfinite(candidate.time_uncertainty_seconds)
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
#ifdef __OCPN__ANDROID__
  RefreshAndroidCards();
#endif
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
#ifdef __OCPN__ANDROID__
    Sight candidate = m_sight;
    wxString error;
    const bool completed = celestial_android::RunJob(this, _("Inspect lunar solution"),
        [&](celestial_android::JobState& state) {
          candidate.m_androidCheckpoint = [&state]() { state.Checkpoint(); };
          candidate.RecomputeLunar(static_cast<int>(candidate_index));
        }, &error);
    candidate.m_androidCheckpoint = {};
    if (!completed) {
      if (!error.empty()) CelestialMessageBox(error, _("Lunar result"), wxOK | wxICON_ERROR, this);
      return;
    }
    m_sight = candidate;
    m_androidSelectedCandidate = candidate_index;
#else
    m_sight.RecomputeLunar(static_cast<int>(candidate_index));
#endif
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
  const std::vector<lunar_distance::PositionGeometry>* geometries = nullptr;
  if (checking && !check_position.geometry.empty())
    geometries = &check_position.geometry;
  else if (time_candidate && !time_candidate->position_geometry.empty())
    geometries = &time_candidate->position_geometry;
  else if (!checking && !m_sight.m_LunarPositionResult.geometry.empty())
    geometries = &m_sight.m_LunarPositionResult.geometry;
  if (positions) {
    const lunar_distance::GeographicPoint approximate{m_sight.m_DRLat,
                                                       m_sight.m_DRLon};
    std::size_t nearest = 0;
    double nearest_distance = INFINITY;
    const bool hasDr = HasSavedDr(m_sight);
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
          hasDr && index == nearest
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
      m_positions->SetItem(row, 3,
                           hasDr ? wxString::Format(
                                       "%.1f NM",
                                       lunar_distance::GreatCircleDistanceNm(
                                           approximate, position))
                                 : _("Not available"));
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
    const std::size_t described = hasDr ? nearest : 0;
    if (geometries && described < geometries->size()) {
      const auto& geometry = (*geometries)[described];
      const wxString deltaZn = wxString::FromUTF8("\xCE\x94Zn");
      m_geometry->SetLabel(wxString::Format(
          _("Branch %zu geometry: Moon Zn %.1f%c; %s Zn %.1f%c; "
            "azimuth separation (%s) %.1f%c; effective crossing %.1f%c. "
            "%s"),
          described + 1, geometry.moon_azimuth_deg, 0x00B0, m_sight.m_Body,
          geometry.body_azimuth_deg, 0x00B0, deltaZn,
          geometry.azimuth_separation_deg, 0x00B0,
          geometry.effective_crossing_angle_deg, 0x00B0,
          GeometryAssessment(geometry.effective_crossing_angle_deg)));
    } else {
      m_geometry->SetLabel(
          _("Position geometry could not be evaluated for this branch."));
    }
  } else {
    m_positions->InsertItem(
        0,
        _("No intersection: ") +
            wxString::FromUTF8((checking ? check_position.error
                                         : m_sight.m_LunarPositionResult.error)
                                   .c_str()));
  }
#ifdef __OCPN__ANDROID__
  // A worker modal loop may deliver the first queued refresh while this
  // list is still empty. Refresh again after all branches are committed.
  RefreshAndroidCards();
#endif
  m_geometry->Wrap(880);
  for (int column = 0; column < 5; ++column)
    m_positions->SetColumnWidth(column, wxLIST_AUTOSIZE_USEHEADER);
  Layout();
  if (auto* page = dynamic_cast<wxScrolledWindow*>(m_positions->GetParent())) {
    page->Layout();
    page->FitInside();
  }
}

void LunarResultsDialog::ApplySelectedWatchOffset(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  const long selected = m_androidSelectedCandidate;
#else
  long selected = m_candidates->GetNextItem(
      -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
#endif
  if (selected < 0 ||
      static_cast<std::size_t>(selected) >= m_sight.m_LunarCandidates.size()) {
    CelestialMessageBox(_("Select a UTC candidate first."), _("Lunar distance"),
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

#ifdef __OCPN__ANDROID__
void LunarResultsDialog::RefreshAndroidCards() {
  if (!m_androidCandidates || m_androidRefreshPending) return;
  m_androidRefreshPending = true;
  wxWeakRef<LunarResultsDialog> weak(this);
  // Rebuild only after a selection callback (and any owned worker) returns.
  QTimer::singleShot(0, GetHandle(), [weak]() {
    if (!weak) return;
    auto* self = weak.get();
    self->m_androidRefreshPending = false;
    auto* candidates = self->m_androidCandidates->GetSizer();
    auto* positions = self->m_androidPositions->GetSizer();
    candidates->Clear(true);
    positions->Clear(true);
    const bool checking = self->m_mode->GetSelection() == 1;
    const long selected = self->m_androidSelectedCandidate;
    for (long row = 0; row < self->m_candidates->GetItemCount(); ++row) {
      wxString caption = checking ? _("Entered UTC check")
          : wxString::Format(row == selected ? _("Selected UTC candidate %ld")
                                             : _("UTC candidate %ld"), row + 1);
      caption += "\nUTC: " + AndroidResultCell(self->m_candidates, row, 0);
      caption += checking ? "\nLunar error (arcmin): " : "\nAdditional correction: ";
      if (!checking && static_cast<size_t>(row) < self->m_sight.m_LunarCandidates.size())
        caption += wxString::Format("%+.6f s", self->m_sight.m_LunarCandidates[row].offset_seconds);
      else caption += AndroidResultCell(self->m_candidates, row, 1);
      caption += (checking ? "\nTrue LD: " : "\nLD cleared: ") + AndroidResultCell(self->m_candidates, row, 2);
      caption += "\nLocal rate: " + AndroidResultCell(self->m_candidates, row, 3);
      caption += "\nUTC uncertainty: " + AndroidResultCell(self->m_candidates, row, 4);
      auto* card = new wxButton(self->m_androidCandidates, wxID_ANY, caption);
      card->GetHandle()->setProperty("cnSelectedCandidate", row == selected && !checking);
      if (!checking) card->Bind(wxEVT_BUTTON, [weak, row](wxCommandEvent&) {
        if (!weak) return;
        // The native clicked callback must return before selection can enter
        // a modal worker loop and rebuild/destroy its originating card.
        QTimer::singleShot(0, weak->GetHandle(), [weak, row]() {
          if (!weak || row >= weak->m_candidates->GetItemCount()) return;
          if (weak->m_androidSelectedCandidate == row) return;
          // wxQt programmatic selection does not reliably emit the wx list
          // notification. Invoke the existing branch controller explicitly.
          weak->UpdatePositions(row);
        });
      });
      candidates->Add(card, 0, wxEXPAND | wxALL, 6);
    }
    for (long row = 0; row < self->m_positions->GetItemCount(); ++row) {
      const wxString latitude = AndroidResultCell(self->m_positions, row, 1);
      wxString caption = latitude.empty() ? AndroidResultCell(self->m_positions, row, 0)
          : _("Position branch ") + AndroidResultCell(self->m_positions, row, 0);
      if (!latitude.empty()) {
        caption += "\nLatitude: " + AndroidResultCell(self->m_positions, row, 1);
        caption += "\nLongitude: " + AndroidResultCell(self->m_positions, row, 2);
        caption += "\nFrom saved DR: " + AndroidResultCell(self->m_positions, row, 3);
        caption += checking ? "\nModel - observed (arcmin): " : "\nHorizontal RMS uncertainty: ";
        caption += AndroidResultCell(self->m_positions, row, 4);
      }
      auto* card = new wxStaticText(self->m_androidPositions, wxID_ANY, caption);
      positions->Add(card, 0, wxEXPAND | wxALL, 12);
    }
    CN_StyleAndroidControls(self->m_androidCandidates);
    CN_StyleAndroidControls(self->m_androidPositions);
    // Styling must precede height and selection colour, since it styles
    // descendants rather than the parent passed to it.
    for (auto* child : self->m_androidCandidates->GetChildren()) {
      auto* card = wxDynamicCast(child, wxButton);
      if (!card) continue;
      const int height = QFontMetrics(card->GetHandle()->font()).lineSpacing() * 6 + 32;
      card->SetMinSize(wxSize(0, qMax(CN_TouchHeight(), height)));
      if (card->GetHandle()->property("cnSelectedCandidate").toBool())
        card->GetHandle()->setStyleSheet(card->GetHandle()->styleSheet() +
            "QPushButton { background: #d1e8f1; color: #102e3b; }");
    }
    self->m_androidCandidates->Layout();
    self->m_androidPositions->Layout();
    celestial_android::LayoutScrolls(self);
  });
}
#endif

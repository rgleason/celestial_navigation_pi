#include "LunarToolsDialog.h"

#include "BodyCatalog.h"
#include "CelestialNavigationDialog.h"
#include "NavigationAlgorithms.h"
#include "NavigationUIUtils.h"
#include "OcpnApiCompat.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "astrolabe/astrolabe.hpp"
#include "moon.h"

#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/progdlg.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/timectrl.h>
#include <wx/wx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <sstream>

namespace {

wxSpinCtrlDouble* Spin(wxWindow* parent, double minimum, double maximum,
                       double value, double increment, int digits = 2) {
  auto* control = new wxSpinCtrlDouble(parent, wxID_ANY);
  control->SetRange(minimum, maximum);
  control->SetValue(value);
  control->SetIncrement(increment);
  control->SetDigits(digits);
  return control;
}

wxBoxSizer* LabelControl(wxWindow* parent, const wxString& label,
                         wxWindow* control) {
  auto* sizer = new wxBoxSizer(wxHORIZONTAL);
  sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  sizer->Add(control, 1, wxEXPAND);
  return sizer;
}

wxDateTime PickerUtc(wxDatePickerCtrl* date, wxTimePickerCtrl* time) {
  const wxDateTime d = date->GetValue();
  const wxDateTime t = time->GetValue();
  return wxDateTime(d.GetDay(), d.GetMonth(), d.GetYear(), t.GetHour(),
                    t.GetMinute(), t.GetSecond());
}

double AngularDistance(double lat1, double lon1, double lat2, double lon2) {
  constexpr double to_rad = 3.14159265358979323846 / 180.0;
  const double cosine = std::sin(lat1 * to_rad) * std::sin(lat2 * to_rad) +
                        std::cos(lat1 * to_rad) * std::cos(lat2 * to_rad) *
                            std::cos((lon1 - lon2) * to_rad);
  return std::acos(std::max(-1.0, std::min(1.0, cosine))) / to_rad;
}

}  // namespace

LunarToolsDialog::LunarToolsDialog(CelestialNavigationDialog* parent)
    : wxDialog(parent, wxID_ANY, _("Lunar Distance and Sextant Tools"),
               wxDefaultPosition, wxSize(1120, 780),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parentDialog(parent),
      m_entryFormat(nullptr),
      m_activeEntryFormat(0),
      m_defaultLatitude(0.0),
      m_defaultLongitude(0.0),
      m_sequencePositionAutomatic(true),
      m_lastPredictionDeg(NAN) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  m_parentDialog->GetPlugin()->GetBoatPosition(&m_defaultLatitude,
                                               &m_defaultLongitude);

  wxFileConfig* config = GetOCPNConfigObject();
  long entryFormat = 0;
  if (config) {
    config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
    config->Read(_T("EntryFormat"), &entryFormat, 0L);
    config->SetPath(_T("/PlugIns/CelestialNavigation/LunarTools"));
    config->Read(_T("EntryFormat"), &entryFormat, entryFormat);
  }
  m_activeEntryFormat = std::max(0L, std::min(entryFormat, 1L));
  auto* formatRow = new wxBoxSizer(wxHORIZONTAL);
  formatRow->Add(new wxStaticText(this, wxID_ANY, _("UTC date/time entry")), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  m_entryFormat = new wxChoice(this, wxID_ANY);
  m_entryFormat->Append(_("Nautical: YYYY-MM-DD, 24-hour"));
  m_entryFormat->Append(_("OpenCPN / platform format"));
  m_entryFormat->SetSelection(m_activeEntryFormat);
  m_entryFormat->SetToolTip(
      _("Nautical format avoids ambiguous dates and AM/PM."));
  formatRow->Add(m_entryFormat, 0, wxEXPAND);
  top->Add(formatRow, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  m_notebook = new wxNotebook(this, wxID_ANY);
  auto* sequence = new wxPanel(m_notebook);
  auto* planner = new wxPanel(m_notebook);
  auto* calibration = new wxPanel(m_notebook);
  BuildSequencePage(sequence);
  BuildPlannerPage(planner);
  BuildCalibrationPage(calibration);
  m_notebook->AddPage(sequence, _("Lunar sequence"));
  m_notebook->AddPage(planner, _("Lunar planner"));
  m_notebook->AddPage(calibration, _("Sextant check"));
  UpdateUtcEntryVisibility();
  m_entryFormat->Bind(wxEVT_CHOICE, &LunarToolsDialog::ChangeUtcEntryFormat,
                      this);
  top->Add(m_notebook, 1, wxEXPAND | wxALL, 8);
  auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); });
  auto* bottom = new wxBoxSizer(wxHORIZONTAL);
  bottom->AddStretchSpacer();
  bottom->Add(close, 0, wxALL, 8);
  top->Add(bottom, 0, wxEXPAND);
  SetSizer(top);
  SetMinSize(wxSize(880, 650));
  CentreOnParent();
  LoadProfiles();
}

LunarToolsDialog::~LunarToolsDialog() {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config || !m_entryFormat) return;
  config->SetPath(_T("/PlugIns/CelestialNavigation/LunarTools"));
  config->Write(_T("EntryFormat"),
                static_cast<long>(m_entryFormat->GetSelection()));
}

void LunarToolsDialog::CreateUtcEntry(wxWindow* parent,
                                      UtcEntryControls* controls,
                                      const wxDateTime& utc) {
  controls->dateContainer = new wxPanel(parent);
  auto* dateSizer = new wxBoxSizer(wxVERTICAL);
  controls->nativeDate =
      new wxDatePickerCtrl(controls->dateContainer, wxID_ANY);
  controls->nauticalDate =
      new wxTextCtrl(controls->dateContainer, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxSize(145, -1), wxTE_PROCESS_ENTER);
  controls->nauticalDate->SetHint(_("YYYY-MM-DD"));
  dateSizer->Add(controls->nativeDate, 0, wxEXPAND);
  dateSizer->Add(controls->nauticalDate, 0, wxEXPAND);
  controls->dateContainer->SetSizer(dateSizer);

  controls->timeContainer = new wxPanel(parent);
  auto* timeSizer = new wxBoxSizer(wxVERTICAL);
  controls->nativeTime =
      new wxTimePickerCtrl(controls->timeContainer, wxID_ANY);
  controls->nauticalTime =
      new wxTextCtrl(controls->timeContainer, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxSize(145, -1), wxTE_PROCESS_ENTER);
  controls->nauticalTime->SetHint(_("HH:MM:SS"));
  timeSizer->Add(controls->nativeTime, 0, wxEXPAND);
  timeSizer->Add(controls->nauticalTime, 0, wxEXPAND);
  controls->timeContainer->SetSizer(timeSizer);
  SetUtcEntry(controls, utc);
}

void LunarToolsDialog::SetUtcEntry(UtcEntryControls* controls,
                                   const wxDateTime& utc) {
  if (!controls || !utc.IsValid()) return;
  controls->nativeDate->SetValue(utc);
  controls->nativeTime->SetValue(utc);
  controls->nauticalDate->ChangeValue(FormatNauticalPlannerDate(utc));
  controls->nauticalTime->ChangeValue(FormatNauticalPlannerTime(utc));
}

wxDateTime LunarToolsDialog::ReadUtcEntry(const UtcEntryControls& controls,
                                          int format, bool showErrors,
                                          const wxString& title) const {
  if (format == 0) {
    wxDateTime utc;
    if (ParseNauticalPlannerDateTime(controls.nauticalDate->GetValue(),
                                     controls.nauticalTime->GetValue(), &utc))
      return utc;
    if (showErrors)
      wxMessageBox(_("Enter UTC date as YYYY-MM-DD and 24-hour time as "
                     "HH:MM:SS."),
                   title, wxOK | wxICON_ERROR,
                   const_cast<LunarToolsDialog*>(this));
    return wxDateTime();
  }
  return PickerUtc(controls.nativeDate, controls.nativeTime);
}

void LunarToolsDialog::ChangeUtcEntryFormat(wxCommandEvent&) {
  const wxDateTime planner =
      ReadUtcEntry(m_plannerUtc, m_activeEntryFormat, false, wxEmptyString);
  const wxDateTime calibration =
      ReadUtcEntry(m_calUtc, m_activeEntryFormat, false, wxEmptyString);
  if (planner.IsValid()) SetUtcEntry(&m_plannerUtc, planner);
  if (calibration.IsValid()) SetUtcEntry(&m_calUtc, calibration);
  m_activeEntryFormat = m_entryFormat->GetSelection();
  UpdateUtcEntryVisibility();
}

void LunarToolsDialog::UpdateUtcEntryVisibility() {
  const bool nautical = m_entryFormat && m_entryFormat->GetSelection() == 0;
  for (UtcEntryControls* controls : {&m_plannerUtc, &m_calUtc}) {
    if (!controls->dateContainer) continue;
    controls->nauticalDate->Show(nautical);
    controls->nauticalTime->Show(nautical);
    controls->nativeDate->Show(!nautical);
    controls->nativeTime->Show(!nautical);
    controls->dateContainer->Layout();
    controls->timeContainer->Layout();
  }
  Layout();
}

void LunarToolsDialog::SelectPageForIntegration(unsigned page) {
  if (page >= m_notebook->GetPageCount()) return;
  m_notebook->SetSelection(page);
  wxCommandEvent event;
  if (page == 0) SolveSequence(event);
  if (page == 1) CalculatePlanner(event);
  if (page == 2) {
    sextant_calibration::Environment environment;
    environment.observer = {m_calLatitude->GetAngleOr(0.0),
                            m_calLongitude->GetAngleOr(0.0)};
    const wxDateTime utc = CalibrationUtc();
    bool found = false;
    for (unsigned first = 0; first < m_calFirstBody->GetCount() && !found;
         ++first) {
      for (unsigned second = 0; second < m_calSecondBody->GetCount();
           ++second) {
        if (m_calFirstBody->GetString(first) ==
            m_calSecondBody->GetString(second))
          continue;
        const auto prediction =
            sextant_calibration::PredictApparentCenterDistance(
                SampleBody(m_calFirstBody->GetString(first), utc),
                SampleBody(m_calSecondBody->GetString(second), utc),
                environment);
        if (prediction.valid && prediction.altitude_difference_deg < 15.0 &&
            prediction.apparent_center_distance_deg > 8.0 &&
            prediction.apparent_center_distance_deg < 120.0) {
          m_calFirstBody->SetSelection(first);
          m_calSecondBody->SetSelection(second);
          found = true;
          break;
        }
      }
    }
    PredictCalibrationPair(event);
  }
}

void LunarToolsDialog::BuildSequencePage(wxWindow* page) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* explanation = new wxStaticText(
      page, wxID_ANY,
      _("Jointly fit a constant watch correction and position from several "
        "saved lunar triples. Each triple may retain its own separately timed "
        "Moon altitude, body altitude and lunar distance."));
  explanation->Wrap(1000);
  top->Add(explanation, 0, wxEXPAND | wxALL, 8);
  auto* upper = new wxBoxSizer(wxHORIZONTAL);
  m_sequenceSights = new wxCheckListBox(page, wxID_ANY);
  const Sight* highlighted = m_parentDialog->GetSelectedSight();
  wxDateTime highlightedUtc;
  if (highlighted && highlighted->m_Type == Sight::LUNAR)
    highlightedUtc = highlighted->m_DateTime;
  for (std::size_t index = 0; index < m_parentDialog->m_Sights.size();
       ++index) {
    const Sight& sight = m_parentDialog->m_Sights[index];
    if (sight.m_Type != Sight::LUNAR) continue;
    m_lunarIndices.push_back(index);
    m_sequenceSights->Append(wxString::Format(
        _("%s  Moon–%s  %s"),
        UtcDateTime::FormatUtc(sight.m_DateTime, "%Y-%m-%d %H:%M:%S"),
        sight.m_Body, FormatNavigationAngle(sight.m_Measurement).c_str()));
    if (highlightedUtc.IsValid() &&
        std::fabs(UtcDateTime::SecondsBetween(sight.m_DateTime,
                                              highlightedUtc)) <= 6.0 * 3600.0)
      m_sequenceSights->Check(m_sequenceSights->GetCount() - 1, true);
  }
  auto* sightColumn = new wxBoxSizer(wxVERTICAL);
  sightColumn->Add(m_sequenceSights, 1, wxEXPAND | wxALL, 5);
  auto* selectionButtons = new wxBoxSizer(wxHORIZONTAL);
  auto* selectVisible =
      new wxButton(page, wxID_ANY, _("Select visible sights"));
  auto* clearSelection = new wxButton(page, wxID_ANY, _("Clear selection"));
  selectionButtons->Add(selectVisible, 0, wxRIGHT, 6);
  selectionButtons->Add(clearSelection, 0);
  sightColumn->Add(selectionButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  m_sequenceReference = new wxStaticText(page, wxID_ANY, wxEmptyString);
  sightColumn->Add(m_sequenceReference, 0,
                   wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);
  upper->Add(sightColumn, 1, wxEXPAND);
  auto* settings = new wxStaticBoxSizer(wxVERTICAL, page, _("Solution"));
  m_sequenceMode = new wxChoice(page, wxID_ANY);
  m_sequenceMode->Append(_("Recover time and position"));
  m_sequenceMode->Append(_("Recover time at known position"));
  m_sequenceMode->SetSelection(0);
  settings->Add(LabelControl(page, _("Mode"), m_sequenceMode), 0,
                wxEXPAND | wxALL, 3);
  const double latitude = m_defaultLatitude;
  const double longitude = m_defaultLongitude;
  m_sequenceLatitude =
      new NavigationAngleCtrl(page, NavigationAngleKind::Latitude, latitude,
                              -90.0, 90.0, wxSize(155, -1));
  m_sequenceLongitude =
      new NavigationAngleCtrl(page, NavigationAngleKind::Longitude, longitude,
                              -180.0, 180.0, wxSize(165, -1));
  settings->Add(
      LabelControl(page, _("Initial / known latitude"), m_sequenceLatitude), 0,
      wxEXPAND | wxALL, 3);
  settings->Add(
      LabelControl(page, _("Initial / known longitude"), m_sequenceLongitude),
      0, wxEXPAND | wxALL, 3);
  auto* positionRow = new wxBoxSizer(wxHORIZONTAL);
  m_sequencePositionSource = new wxStaticText(page, wxID_ANY, wxEmptyString);
  auto* useEarliest =
      new wxButton(page, wxID_ANY, _("Use earliest selected DR"));
  positionRow->Add(m_sequencePositionSource, 1,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  positionRow->Add(useEarliest, 0);
  settings->Add(positionRow, 0, wxEXPAND | wxALL, 3);
  m_sequenceSearchHours = Spin(page, 0.25, 24.0, 12.0, 0.5, 1);
  settings->Add(LabelControl(page, _("Additional watch correction search ±"),
                             m_sequenceSearchHours),
                0, wxEXPAND | wxALL, 3);
  m_sequenceSearchHours->SetToolTip(
      _("Search this many hours either side of the correction already applied "
        "to each sight. This is not an observation start time."));
  m_sequenceRobust =
      new wxCheckBox(page, wxID_ANY, _("Robust fit; retain and flag outliers"));
  m_sequenceRobust->SetValue(true);
  m_sequenceBias = new wxCheckBox(
      page, wxID_ANY, _("Estimate common residual index bias (advanced)"));
  m_sequenceMotion = new wxCheckBox(
      page, wxID_ANY, _("Advance session position with COG / SOG"));
  settings->Add(m_sequenceRobust, 0, wxALL, 3);
  settings->Add(m_sequenceBias, 0, wxALL, 3);
  settings->Add(m_sequenceMotion, 0, wxALL, 3);
  auto* motion = new wxBoxSizer(wxHORIZONTAL);
  m_sequenceCog = Spin(page, 0.0, 359.9, 0.0, 1.0, 1);
  m_sequenceSog = Spin(page, 0.0, 80.0, 0.0, 0.1, 1);
  motion->Add(LabelControl(page, _("COG true"), m_sequenceCog), 1,
              wxEXPAND | wxRIGHT, 5);
  motion->Add(LabelControl(page, _("SOG kn"), m_sequenceSog), 1, wxEXPAND);
  settings->Add(motion, 0, wxEXPAND | wxALL, 3);
  auto* solve = new wxButton(page, wxID_ANY, _("Solve selected sequence"));
  solve->Bind(wxEVT_BUTTON, &LunarToolsDialog::SolveSequence, this);
  settings->Add(solve, 0, wxEXPAND | wxALL, 5);
  upper->Add(settings, 0, wxEXPAND | wxALL, 5);
  top->Add(upper, 1, wxEXPAND);

  auto* resultHeader = new wxBoxSizer(wxHORIZONTAL);
  m_sequenceCandidate = new wxChoice(page, wxID_ANY);
  m_sequenceCandidate->Bind(wxEVT_CHOICE, &LunarToolsDialog::SelectCandidate,
                            this);
  resultHeader->Add(new wxStaticText(page, wxID_ANY, _("Solution candidate")),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  resultHeader->Add(m_sequenceCandidate, 0, wxRIGHT, 10);
  resultHeader->AddStretchSpacer();
  m_applySequence =
      new wxButton(page, wxID_ANY, _("Apply watch correction to all sights"));
  m_applySequence->Enable(false);
  m_applySequence->Bind(wxEVT_BUTTON,
                        &LunarToolsDialog::ApplySequenceCorrection, this);
  resultHeader->Add(m_applySequence, 0, wxLEFT, 5);
  top->Add(resultHeader, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  m_sequenceSummary = new wxStaticText(
      page, wxID_ANY, _("Select at least two saved lunar observations."));
  top->Add(m_sequenceSummary, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  m_sequenceResiduals =
      new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
  const wxString columns[] = {_("Observation"), _("Distance residual"),
                              _("Moon-alt residual"), _("Body-alt residual"),
                              _("Assessment")};
  const int widths[] = {260, 140, 140, 140, 220};
  for (int index = 0; index < 5; ++index) {
    m_sequenceResiduals->InsertColumn(index, columns[index]);
    m_sequenceResiduals->SetColumnWidth(index, widths[index]);
  }
  top->Add(m_sequenceResiduals, 1, wxEXPAND | wxALL, 6);
  page->SetSizer(top);

  m_sequenceSights->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent&) {
    UpdateSequenceSelection();
  });
  selectVisible->Bind(wxEVT_BUTTON, &LunarToolsDialog::SelectVisibleSequence,
                      this);
  clearSelection->Bind(wxEVT_BUTTON, &LunarToolsDialog::ClearSequenceSelection,
                       this);
  useEarliest->Bind(wxEVT_BUTTON,
                    &LunarToolsDialog::UseEarliestSequencePosition, this);
  m_sequenceLatitude->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
    m_sequencePositionAutomatic = false;
    m_sequencePositionSource->SetLabel(_("Manual position"));
  });
  m_sequenceLongitude->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
    m_sequencePositionAutomatic = false;
    m_sequencePositionSource->SetLabel(_("Manual position"));
  });
  UpdateSequenceSelection();
}

void LunarToolsDialog::BuildPlannerPage(wxWindow* page) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* note = new wxStaticText(
      page, wxID_ANY,
      _("Rank fully offline Moon–body pairs. The time sensitivity is the "
        "approximate UTC change corresponding to 0.1′ of distance; visibility "
        "and a comfortable sextant angle still require the navigator's "
        "judgement."));
  note->Wrap(1000);
  top->Add(note, 0, wxEXPAND | wxALL, 8);
  auto* controls = new wxBoxSizer(wxVERTICAL);
  auto* positionRow = new wxBoxSizer(wxHORIZONTAL);
  m_plannerLatitude =
      new NavigationAngleCtrl(page, NavigationAngleKind::Latitude,
                              m_defaultLatitude, -90.0, 90.0, wxSize(155, -1));
  m_plannerLongitude = new NavigationAngleCtrl(
      page, NavigationAngleKind::Longitude, m_defaultLongitude, -180.0, 180.0,
      wxSize(165, -1));
  // The controls are deliberately UTC-entry fields.  Supplying a real
  // instant here would make wxWidgets display local clock fields and repeat
  // the exact local/UTC ambiguity the planner is intended to avoid.
  const wxDateTime utcNow = UtcDateTime::Now();
  CreateUtcEntry(page, &m_plannerUtc, utcNow);
  positionRow->Add(LabelControl(page, _("Latitude"), m_plannerLatitude), 1,
                   wxRIGHT, 6);
  positionRow->Add(LabelControl(page, _("Longitude"), m_plannerLongitude), 1,
                   wxRIGHT, 6);
  auto* calculate = new wxButton(page, wxID_ANY, _("Rank pairs"));
  calculate->Bind(wxEVT_BUTTON, &LunarToolsDialog::CalculatePlanner, this);
  positionRow->Add(calculate, 0);
  controls->Add(positionRow, 0, wxEXPAND | wxBOTTOM, 5);
  auto* timeRow = new wxBoxSizer(wxHORIZONTAL);
  timeRow->Add(LabelControl(page, _("UTC date"), m_plannerUtc.dateContainer), 1,
               wxRIGHT, 6);
  timeRow->Add(LabelControl(page, _("UTC time"), m_plannerUtc.timeContainer), 1,
               wxRIGHT, 6);
  controls->Add(timeRow, 0, wxEXPAND);
  top->Add(controls, 0, wxEXPAND | wxALL, 6);
  m_plannerList = new wxListCtrl(page, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxLC_REPORT | wxBORDER_SUNKEN);
  const wxString columns[] = {
      _("Body"),          _("Moon altitude"), _("Moon Zn true"),
      _("Body altitude"), _("Body Zn true"),  _("Distance"),
      _("Rate"),          _("0.1′ time"),     _("Moon illum."),
      _("Magnitude"),     _("Quality")};
  const int widths[] = {150, 145, 105, 145, 105, 125, 125, 115, 105, 95, 230};
  for (int index = 0; index < 11; ++index) {
    m_plannerList->InsertColumn(index, columns[index]);
    m_plannerList->SetColumnWidth(index, widths[index]);
  }
  top->Add(m_plannerList, 1, wxEXPAND | wxALL, 6);
  page->SetSizer(top);
}

void LunarToolsDialog::BuildCalibrationPage(wxWindow* page) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* note = new wxStaticText(
      page, wxID_ANY,
      _("This is an observational check, not a substitute for mechanical "
        "adjustment. First remove perpendicularity, side, collimation and "
        "index "
        "errors in the instrument's specified order. Star–star pairs at "
        "similar "
        "comfortable altitudes are best for scale/centering checks; Moon pairs "
        "are end-to-end validation and depend strongly on UTC and position."));
  note->Wrap(1000);
  top->Add(note, 0, wxEXPAND | wxALL, 8);
  auto* prediction =
      new wxStaticBoxSizer(wxVERTICAL, page, _("Offline pair prediction"));
  auto* row1 = new wxBoxSizer(wxHORIZONTAL);
  m_calLatitude =
      new NavigationAngleCtrl(page, NavigationAngleKind::Latitude,
                              m_defaultLatitude, -90.0, 90.0, wxSize(155, -1));
  m_calLongitude = new NavigationAngleCtrl(page, NavigationAngleKind::Longitude,
                                           m_defaultLongitude, -180.0, 180.0,
                                           wxSize(165, -1));
  const wxDateTime utcNow = UtcDateTime::Now();
  CreateUtcEntry(page, &m_calUtc, utcNow);
  row1->Add(LabelControl(page, _("Latitude"), m_calLatitude), 1, wxRIGHT, 5);
  row1->Add(LabelControl(page, _("Longitude"), m_calLongitude), 1, wxRIGHT, 5);
  prediction->Add(row1, 0, wxEXPAND | wxALL, 3);
  auto* utcRow = new wxBoxSizer(wxHORIZONTAL);
  utcRow->Add(LabelControl(page, _("UTC date"), m_calUtc.dateContainer), 1,
              wxRIGHT, 5);
  utcRow->Add(LabelControl(page, _("UTC time"), m_calUtc.timeContainer), 1);
  prediction->Add(utcRow, 0, wxEXPAND | wxALL, 3);
  auto* row2 = new wxBoxSizer(wxHORIZONTAL);
  m_calFirstBody = new wxChoice(page, wxID_ANY);
  m_calSecondBody = new wxChoice(page, wxID_ANY);
  m_calContact = new wxChoice(page, wxID_ANY);
  m_calContact->Append(_("Centre to centre"));
  m_calContact->Append(_("Near limbs / contact"));
  m_calContact->Append(_("Far limbs / contact"));
  m_calContact->SetSelection(0);
  PopulateBodies(m_calFirstBody, true);
  PopulateBodies(m_calSecondBody, false);
  m_calFirstBody->SetStringSelection(_("Sirius"));
  m_calSecondBody->SetStringSelection(_("Vega"));
  row2->Add(LabelControl(page, _("First body"), m_calFirstBody), 1, wxRIGHT, 5);
  row2->Add(LabelControl(page, _("Second body"), m_calSecondBody), 1, wxRIGHT,
            5);
  row2->Add(LabelControl(page, _("Contact"), m_calContact), 1, wxRIGHT, 5);
  auto* predict = new wxButton(page, wxID_ANY, _("Predict distance"));
  predict->Bind(wxEVT_BUTTON, &LunarToolsDialog::PredictCalibrationPair, this);
  row2->Add(predict, 0);
  prediction->Add(row2, 0, wxEXPAND | wxALL, 3);
  auto* row3 = new wxBoxSizer(wxHORIZONTAL);
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  m_calPressure = Spin(page, 0.0, 1100.0, defaults.pressure, 1.0, 1);
  m_calTemperature = Spin(page, -60.0, 60.0, defaults.temperature, 1.0, 1);
  row3->Add(LabelControl(page, _("Pressure hPa"), m_calPressure), 0, wxRIGHT,
            8);
  row3->Add(LabelControl(page, _("Temperature °C"), m_calTemperature), 0,
            wxRIGHT, 12);
  prediction->Add(row3, 0, wxEXPAND | wxALL, 3);
  auto* configuredIndex = new wxStaticText(
      page, wxID_ANY,
      wxString::Format(
          _("Configured sight index error: %+.2f′ (reference only; not applied "
            "to this sextant calibration check)."),
          defaults.indexError));
  prediction->Add(configuredIndex, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                  6);
  m_calPrediction = new wxStaticText(page, wxID_ANY, _("Not calculated"));
  prediction->Add(m_calPrediction, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                  6);
  top->Add(prediction, 0, wxEXPAND | wxALL, 5);

  auto* entry = new wxBoxSizer(wxHORIZONTAL);
  m_calObservedAngle = new NavigationAngleCtrl(
      page, NavigationAngleKind::Generic, 0.0, 0.0, 180.0, wxSize(165, -1));
  m_calUncertainty = Spin(page, 0.05, 10.0, 0.2, 0.05, 2);
  m_calNote = new wxTextCtrl(page, wxID_ANY);
  entry->Add(LabelControl(page, _("Observed angle"), m_calObservedAngle), 2,
             wxRIGHT, 5);
  entry->Add(LabelControl(page, _("uncertainty ±′"), m_calUncertainty), 1,
             wxRIGHT, 5);
  entry->Add(LabelControl(page, _("note / shade"), m_calNote), 2, wxRIGHT, 5);
  auto* add = new wxButton(page, wxID_ANY, _("Add repeat"));
  add->Bind(wxEVT_BUTTON, &LunarToolsDialog::AddCalibrationReading, this);
  entry->Add(add, 0);
  top->Add(entry, 0, wxEXPAND | wxALL, 6);
  m_calReadings =
      new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, 170),
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
  const wxString columns[] = {_("Predicted"), _("Observed"),
                              _("Correction to add"), _("Uncertainty"),
                              _("Note")};
  const int widths[] = {150, 150, 160, 130, 330};
  for (int index = 0; index < 5; ++index) {
    m_calReadings->InsertColumn(index, columns[index]);
    m_calReadings->SetColumnWidth(index, widths[index]);
  }
  top->Add(m_calReadings, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
  auto* profile = new wxStaticBoxSizer(wxHORIZONTAL, page,
                                       _("Persistent correction profile"));
  m_profileChoice = new wxChoice(page, wxID_ANY);
  m_profileChoice->Bind(wxEVT_CHOICE,
                        &LunarToolsDialog::SelectCalibrationProfile, this);
  m_profileName = new wxTextCtrl(page, wxID_ANY, _("Primary sextant"));
  m_profileSerial = new wxTextCtrl(page, wxID_ANY);
  profile->Add(LabelControl(page, _("Saved"), m_profileChoice), 1, wxALL, 3);
  profile->Add(LabelControl(page, _("Name"), m_profileName), 1, wxALL, 3);
  profile->Add(LabelControl(page, _("Serial"), m_profileSerial), 1, wxALL, 3);
  auto* remove = new wxButton(page, wxID_ANY, _("Remove selected reading"));
  remove->Bind(wxEVT_BUTTON, &LunarToolsDialog::RemoveCalibrationReading, this);
  profile->Add(remove, 0, wxALL, 3);
  auto* save = new wxButton(page, wxID_ANY, _("Build / save profile"));
  save->Bind(wxEVT_BUTTON, &LunarToolsDialog::SaveCalibrationProfile, this);
  profile->Add(save, 0, wxALL, 3);
  top->Add(profile, 0, wxEXPAND | wxALL, 5);
  m_profileCorrection = new wxStaticText(
      page, wxID_ANY,
      _("Active-profile correction at the entered observed angle: —"));
  top->Add(m_profileCorrection, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  m_profileSummary =
      new wxStaticText(page, wxID_ANY,
                       _("No profile built. Corrections are added to raw "
                         "readings; the plugin never rewrites observations."));
  m_profileSummary->Wrap(1000);
  top->Add(m_profileSummary, 0, wxEXPAND | wxALL, 6);
  m_calObservedAngle->Bind(
      wxEVT_TEXT, [this](wxCommandEvent&) { UpdateProfileCorrection(); });
  page->SetSizer(top);
}

void LunarToolsDialog::PopulateBodies(wxChoice* choice, bool includeMoon) {
  for (const auto& body : BodyCatalog::All()) {
    if (!includeMoon && body.kind == CelestialBodyKind::Moon) continue;
    choice->Append(body.name);
  }
  if (choice->GetCount()) choice->SetSelection(0);
}

void LunarToolsDialog::UpdateSequenceSelection(bool refreshAutomaticPosition) {
  const Sight* earliest = nullptr;
  const Sight* latest = nullptr;
  unsigned selected = 0;
  for (unsigned list = 0; list < m_sequenceSights->GetCount(); ++list) {
    if (!m_sequenceSights->IsChecked(list)) continue;
    const Sight& sight = m_parentDialog->m_Sights[m_lunarIndices[list]];
    ++selected;
    if (!earliest ||
        UtcDateTime::IsEarlier(sight.m_DateTime, earliest->m_DateTime))
      earliest = &sight;
    if (!latest || UtcDateTime::IsLater(sight.m_DateTime, latest->m_DateTime))
      latest = &sight;
  }
  if (!earliest) {
    m_sequenceReference->SetLabel(
        _("No observations selected. Select one watch/session."));
    m_sequencePositionSource->SetLabel(m_sequencePositionAutomatic
                                           ? _("Boat position (no selected DR)")
                                           : _("Manual position"));
    return;
  }
  m_sequenceReference->SetLabel(wxString::Format(
      _("%u selected; recorded UTC %s to %s"), selected,
      UtcDateTime::FormatUtc(earliest->m_DateTime, "%Y-%m-%d %H:%M:%S"),
      UtcDateTime::FormatUtc(latest->m_DateTime, "%Y-%m-%d %H:%M:%S")));
  if (refreshAutomaticPosition && m_sequencePositionAutomatic) {
    m_sequenceLatitude->SetAngle(earliest->m_DRLat);
    m_sequenceLongitude->SetAngle(earliest->m_DRLon);
  }
  m_sequencePositionSource->SetLabel(m_sequencePositionAutomatic
                                         ? _("Earliest selected sight DR")
                                         : _("Manual position"));
}

void LunarToolsDialog::SelectVisibleSequence(wxCommandEvent&) {
  for (unsigned list = 0; list < m_sequenceSights->GetCount(); ++list) {
    const Sight& sight = m_parentDialog->m_Sights[m_lunarIndices[list]];
    m_sequenceSights->Check(list, sight.IsVisible());
  }
  UpdateSequenceSelection();
}

void LunarToolsDialog::ClearSequenceSelection(wxCommandEvent&) {
  for (unsigned list = 0; list < m_sequenceSights->GetCount(); ++list)
    m_sequenceSights->Check(list, false);
  UpdateSequenceSelection();
}

void LunarToolsDialog::UseEarliestSequencePosition(wxCommandEvent&) {
  m_sequencePositionAutomatic = true;
  UpdateSequenceSelection();
}

void LunarToolsDialog::SolveSequence(wxCommandEvent&) {
  double sequenceLatitude = 0.0;
  double sequenceLongitude = 0.0;
  if (!m_sequenceLatitude->GetAngle(&sequenceLatitude) ||
      !m_sequenceLongitude->GetAngle(&sequenceLongitude)) {
    wxMessageBox(_("Enter a valid initial or known position."),
                 _("Lunar sequence"), wxOK | wxICON_ERROR, this);
    return;
  }
  m_sequenceLatitude->Normalize();
  m_sequenceLongitude->Normalize();
  std::vector<unsigned> selectedLists;
  wxDateTime reference;
  wxDateTime latest;
  for (unsigned list = 0; list < m_sequenceSights->GetCount(); ++list) {
    if (!m_sequenceSights->IsChecked(list)) continue;
    selectedLists.push_back(list);
    const Sight& sight = m_parentDialog->m_Sights[m_lunarIndices[list]];
    if (!reference.IsValid() ||
        UtcDateTime::IsEarlier(sight.m_DateTime, reference))
      reference = sight.m_DateTime;
    if (!latest.IsValid() || UtcDateTime::IsLater(sight.m_DateTime, latest))
      latest = sight.m_DateTime;
  }
  if (selectedLists.size() < 2) {
    wxMessageBox(_("Select at least two lunar observations."),
                 _("Lunar sequence"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  if (selectedLists.size() > 12) {
    wxMessageBox(
        _("A lunar sequence is one watch/session. Select no more than 12 "
          "observations; use Clear selection or Select visible sights to "
          "choose a coherent group."),
        _("Lunar sequence"), wxOK | wxICON_WARNING, this);
    return;
  }
  const double sessionSpan =
      std::fabs(UtcDateTime::SecondsBetween(latest, reference));
  if (sessionSpan > 24.0 * 3600.0) {
    wxMessageBox(
        _("The selected observations span more than 24 hours. They do not "
          "represent one watch/session; select a coherent group before "
          "solving."),
        _("Lunar sequence"), wxOK | wxICON_WARNING, this);
    return;
  }

  std::vector<lunar_session::SessionObservation> entries;
  std::vector<std::shared_ptr<Sight> > snapshots;
  reference = wxDateTime();
  for (unsigned list : selectedLists) {
    std::shared_ptr<Sight> snapshot(
        new Sight(m_parentDialog->m_Sights[m_lunarIndices[list]]));
    snapshot->Recompute(m_parentDialog->GetClockCorrection());
    if (!reference.IsValid() ||
        UtcDateTime::IsEarlier(snapshot->m_CorrectedDateTime, reference))
      reference = snapshot->m_CorrectedDateTime;
    snapshots.push_back(snapshot);
  }
  lunar_session::Options options;
  options.solve_position = m_sequenceMode->GetSelection() == 0;
  options.known_or_initial_position = {sequenceLatitude, sequenceLongitude};
  options.position_seeds.push_back({sequenceLatitude, sequenceLongitude});
  options.robust_fit = m_sequenceRobust->GetValue();
  options.estimate_common_index_bias = m_sequenceBias->GetValue();
  const double span = m_sequenceSearchHours->GetValue() * 3600.0;
  options.start_correction_seconds = -span;
  options.end_correction_seconds = span;
  options.correction_seed_step_seconds = std::max(1800.0, span / 4.0);
  options.maximum_iterations = 50;
  options.moving_observer = m_sequenceMotion->GetValue();
  options.course_true_deg = m_sequenceCog->GetValue();
  options.speed_knots = m_sequenceSog->GetValue();
  for (const std::shared_ptr<Sight>& snapshot : snapshots) {
    Sight& sight = *snapshot;
    lunar_session::SessionObservation entry;
    entry.label = wxString::Format(_("%s Moon–%s"),
                                   UtcDateTime::FormatUtc(
                                       sight.m_CorrectedDateTime, "%H:%M:%S"),
                                   sight.m_Body)
                      .ToStdString();
    entry.settings = sight.LunarObservation();
    entry.epoch_offset_seconds =
        UtcDateTime::SecondsBetween(sight.m_CorrectedDateTime, reference);
    const auto sight_ephemeris = sight.LunarEphemeris();
    const double epoch = entry.epoch_offset_seconds;
    entry.ephemeris = [snapshot, sight_ephemeris, epoch](
                          double seconds,
                          lunar_distance::EphemerisSample* sample,
                          std::string* error) {
      (void)snapshot;
      return sight_ephemeris(seconds - epoch, sample, error);
    };
    entries.push_back(entry);
    for (const auto& individual : sight.m_LunarCandidates) {
      options.correction_seeds.push_back(individual.offset_seconds);
      for (const auto& position : individual.positions) {
        bool duplicate = false;
        for (const auto& seed : options.position_seeds) {
          if (lunar_distance::GreatCircleDistanceNm(
                  {seed.latitude_deg, seed.longitude_deg}, position) < 30.0) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate && options.position_seeds.size() < 8)
          options.position_seeds.push_back(
              {position.latitude_deg, position.longitude_deg});
      }
    }
  }
  std::atomic<bool> cancelRequested(false);
  std::atomic<std::size_t> completedStarts(0);
  std::atomic<std::size_t> totalStarts(0);
  options.cancel_requested = [&cancelRequested]() {
    return cancelRequested.load();
  };
  options.progress = [&completedStarts, &totalStarts](std::size_t completed,
                                                      std::size_t total) {
    completedStarts.store(completed);
    totalStarts.store(total);
  };
  std::future<lunar_session::Result> future = std::async(
      std::launch::async,
      [entries, options]() { return lunar_session::Solve(entries, options); });
  wxProgressDialog progress(
      _("Lunar sequence"), _("Preparing bounded multi-start solution…"), 100,
      this,
      wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME |
          wxPD_REMAINING_TIME | wxPD_SMOOTH | wxPD_AUTO_HIDE);
  bool userCancelled = false;
  while (future.wait_for(std::chrono::milliseconds(75)) !=
         std::future_status::ready) {
    const std::size_t total = totalStarts.load();
    const std::size_t completed = completedStarts.load();
    const int percent =
        total > 0 ? std::min(99, static_cast<int>(completed * 100 / total)) : 0;
    const std::size_t displayed =
        total > 0 ? std::min(total, completed + 1) : 0;
    if (!userCancelled &&
        !progress.Update(
            percent,
            total > 0
                ? wxString::Format(_("Testing bounded solution %zu of %zu"),
                                   displayed, total)
                : _("Preparing bounded multi-start solution…"))) {
      userCancelled = true;
      cancelRequested.store(true);
    }
  }
  m_sequenceResult = future.get();
  if (!userCancelled) progress.Update(100, _("Lunar sequence complete."));
  m_sequenceCandidate->Clear();
  m_sequenceResiduals->DeleteAllItems();
  m_applySequence->Enable(false);
  if (!m_sequenceResult.valid) {
    m_sequenceSummary->SetLabel(
        userCancelled
            ? _("Calculation cancelled; saved sights were not changed.")
            : _("No solution: ") +
                  wxString::FromUTF8(m_sequenceResult.error.c_str()));
    return;
  }
  for (std::size_t index = 0; index < m_sequenceResult.candidates.size();
       ++index)
    m_sequenceCandidate->Append(
        wxString::Format(_("Candidate %zu"), index + 1));
  m_sequenceCandidate->SetSelection(0);
  ShowCandidate(0);
  m_applySequence->Enable(true);
}

void LunarToolsDialog::SelectCandidate(wxCommandEvent&) {
  const int selection = m_sequenceCandidate->GetSelection();
  if (selection != wxNOT_FOUND)
    ShowCandidate(static_cast<std::size_t>(selection));
}

void LunarToolsDialog::ShowCandidate(std::size_t index) {
  if (index >= m_sequenceResult.candidates.size()) return;
  const auto& candidate = m_sequenceResult.candidates[index];
  wxString summary = wxString::Format(
      _("Additional watch correction %+0.1f s; reference position %.5f, %.5f; "
        "RMS %.2f′ (weighted %.2f); σtime %.1f s; σposition %.1f NM"),
      candidate.clock_correction_seconds,
      candidate.reference_position.latitude_deg,
      candidate.reference_position.longitude_deg, candidate.angular_rms_arcmin,
      candidate.weighted_rms, candidate.time_uncertainty_seconds,
      candidate.position_uncertainty_nm);
  if (m_sequenceBias->GetValue())
    summary += wxString::Format(_("; common index bias %+0.2f′"),
                                candidate.common_index_bias_arcmin);
  for (const auto& warning : m_sequenceResult.warnings)
    summary += _(" — ") + wxString::FromUTF8(warning.c_str());
  m_sequenceSummary->SetLabel(summary);
  m_sequenceSummary->Wrap(1000);
  m_sequenceResiduals->DeleteAllItems();
  for (std::size_t row = 0; row < candidate.residuals.size(); ++row) {
    const auto& residual = candidate.residuals[row];
    const long item = m_sequenceResiduals->InsertItem(
        static_cast<long>(row), wxString::FromUTF8(residual.label.c_str()));
    m_sequenceResiduals->SetItem(
        item, 1, wxString::Format("%+0.2f'", residual.distance_arcmin));
    m_sequenceResiduals->SetItem(
        item, 2, wxString::Format("%+0.2f'", residual.moon_altitude_arcmin));
    m_sequenceResiduals->SetItem(
        item, 3, wxString::Format("%+0.2f'", residual.body_altitude_arcmin));
    m_sequenceResiduals->SetItem(
        item, 4,
        residual.possible_outlier
            ? wxString::Format(_("Inspect: %.1fσ"), residual.standardized_max)
            : _("Consistent"));
  }
}

void LunarToolsDialog::ApplySequenceCorrection(wxCommandEvent&) {
  const int selection = m_sequenceCandidate->GetSelection();
  if (selection == wxNOT_FOUND) return;
  const int additional = static_cast<int>(std::lround(
      m_sequenceResult.candidates[selection].clock_correction_seconds));
  const int total = m_parentDialog->GetClockCorrection() + additional;
  if (wxMessageBox(
          wxString::Format(
              _("Apply total sight correction %+d seconds (current %+d, "
                "sequence adds %+d) to every saved sight? Raw watch times are "
                "retained."),
              total, m_parentDialog->GetClockCorrection(), additional),
          _("Apply recovered watch correction"), wxYES_NO | wxICON_QUESTION,
          this) == wxYES) {
    m_parentDialog->ApplyClockCorrection(total);
    m_applySequence->Enable(false);
  }
}

void LunarToolsDialog::CalculatePlanner(wxCommandEvent&) {
  m_plannerList->DeleteAllItems();
  double observerLatitude = 0.0;
  double observerLongitude = 0.0;
  if (!m_plannerLatitude->GetAngle(&observerLatitude) ||
      !m_plannerLongitude->GetAngle(&observerLongitude)) {
    wxMessageBox(_("Enter a valid planner latitude and longitude."),
                 _("Lunar planner"), wxOK | wxICON_ERROR, this);
    return;
  }
  m_plannerLatitude->Normalize();
  m_plannerLongitude->Normalize();
  const wxDateTime utc = ReadUtcEntry(
      m_plannerUtc, m_entryFormat->GetSelection(), true, _("Lunar planner"));
  if (!utc.IsValid()) return;
  Sight sky;
  sky.m_CorrectedDateTime = utc;
  double moon_lat = 0.0, moon_lon = 0.0;
  sky.m_Body = _("Moon");
  sky.BodyLocation(utc, &moon_lat, &moon_lon, nullptr, nullptr, nullptr);
  const wxDateTime later = UtcDateTime::AddSeconds(utc, 300.0);
  double moon_lat_later = 0.0, moon_lon_later = 0.0;
  sky.BodyLocation(later, &moon_lat_later, &moon_lon_later, nullptr, nullptr,
                   nullptr);
  struct Row {
    wxString body;
    double moon_alt;
    double moon_az;
    double body_alt;
    double body_az;
    double distance;
    double rate;
    double sensitivity;
    double illumination;
    double magnitude;
    double score;
    wxString quality;
  };
  const MoonInformation moonInformation =
      CalculateMoonInformation(utc, observerLatitude, observerLongitude);
  std::vector<Row> rows;
  for (const auto& info : BodyCatalog::All()) {
    if (info.kind == CelestialBodyKind::Moon) continue;
    double body_lat = 0.0, body_lon = 0.0;
    sky.m_Body = info.name;
    sky.BodyLocation(utc, &body_lat, &body_lon, nullptr, nullptr, nullptr);
    double moon_alt = 0.0, moon_az = 0.0, body_alt = 0.0, body_az = 0.0;
    sky.AltitudeAzimuth(observerLatitude, observerLongitude, moon_lat, moon_lon,
                        &moon_alt, &moon_az);
    sky.AltitudeAzimuth(observerLatitude, observerLongitude, body_lat, body_lon,
                        &body_alt, &body_az);
    const double distance =
        AngularDistance(moon_lat, moon_lon, body_lat, body_lon);
    double body_lat_later = 0.0, body_lon_later = 0.0;
    sky.BodyLocation(later, &body_lat_later, &body_lon_later, nullptr, nullptr,
                     nullptr);
    const double distance_later = AngularDistance(
        moon_lat_later, moon_lon_later, body_lat_later, body_lon_later);
    const double rate = (distance_later - distance) * 60.0 * 12.0;
    const double sensitivity =
        std::fabs(rate) > 0.01 ? 360.0 / std::fabs(rate) : INFINITY;
    double score = 100.0;
    wxString quality = _("Good geometry");
    if (moon_alt < 10.0 || body_alt < 10.0) {
      score -= 80.0;
      quality = _("Too low / hidden");
    }
    if (moon_alt > 75.0 || body_alt > 75.0) {
      score -= 15.0;
      quality = _("Awkward altitude");
    }
    if (distance < 5.0 || distance > 120.0) {
      score -= 60.0;
      quality = _("Awkward sextant angle");
    }
    if (std::fabs(rate) < 10.0) {
      score -= 30.0;
      quality = _("Weak time sensitivity");
    }
    score -= std::min(25.0, std::fabs(moon_alt - body_alt) * 0.4);
    score -= std::max(0.0, info.visualMagnitude - 1.5) * 4.0;
    rows.push_back({info.name, moon_alt, moon_az, body_alt, body_az, distance,
                    rate, sensitivity,
                    moonInformation.illuminatedFraction * 100.0,
                    info.visualMagnitude, score, quality});
  }
  std::sort(rows.begin(), rows.end(),
            [](const Row& a, const Row& b) { return a.score > b.score; });
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    long item = m_plannerList->InsertItem(index, row.body);
    m_plannerList->SetItem(item, 1, FormatNavigationAngle(row.moon_alt));
    m_plannerList->SetItem(item, 2, wxString::Format("%.1f°", row.moon_az));
    m_plannerList->SetItem(item, 3, FormatNavigationAngle(row.body_alt));
    m_plannerList->SetItem(item, 4, wxString::Format("%.1f°", row.body_az));
    m_plannerList->SetItem(item, 5, FormatNavigationAngle(row.distance));
    m_plannerList->SetItem(item, 6, wxString::Format("%+.1f′/h", row.rate));
    m_plannerList->SetItem(item, 7,
                           std::isfinite(row.sensitivity)
                               ? wxString::Format("%.1f s", row.sensitivity)
                               : _("—"));
    m_plannerList->SetItem(item, 8,
                           wxString::Format("%.1f%%", row.illumination));
    m_plannerList->SetItem(item, 9, wxString::Format("%.1f", row.magnitude));
    m_plannerList->SetItem(item, 10, row.quality);
  }
}

wxDateTime LunarToolsDialog::CalibrationUtc() const {
  return ReadUtcEntry(m_calUtc, m_entryFormat->GetSelection(), true,
                      _("Sextant check"));
}

sextant_calibration::BodySample LunarToolsDialog::SampleBody(
    const wxString& body, const wxDateTime& utc) {
  Sight sky;
  sky.m_Body = body;
  double latitude = 0.0, longitude = 0.0, radius_au = 0.0, distance_km = 0.0;
  sky.BodyLocation(utc, &latitude, &longitude, nullptr, &radius_au,
                   &distance_km);
  sextant_calibration::BodySample sample;
  sample.name = body.ToStdString();
  sample.geographic_latitude_deg = latitude;
  sample.geographic_longitude_deg = longitude;
  constexpr double to_deg = 180.0 / 3.14159265358979323846;
  if (body.CmpNoCase(_("Moon")) == 0) {
    const double moon_km = moon_distance(astrolabe::dynamical::ut_to_dt(
        UtcDateTime::ToInstant(utc).GetJulianDayNumber()));
    sample.horizontal_parallax_deg = std::asin(EARTH_RADIUS / moon_km) * to_deg;
    sample.semidiameter_deg =
        std::asin(K_MOON * std::sin(sample.horizontal_parallax_deg / to_deg)) *
        to_deg;
  } else if (body.CmpNoCase(_("Sun")) == 0 && radius_au > 0.0) {
    sample.horizontal_parallax_deg = 0.002442 / radius_au;
    sample.semidiameter_deg = 0.266564 / radius_au;
  } else if (sky.m_IsPlanet && distance_km > EARTH_RADIUS) {
    sample.horizontal_parallax_deg =
        std::asin(EARTH_RADIUS / distance_km) * to_deg;
    double radius_km = 0.0;
    if (body.CmpNoCase(_("Mercury")) == 0) radius_km = 2439.7;
    if (body.CmpNoCase(_("Venus")) == 0) radius_km = 6051.8;
    if (body.CmpNoCase(_("Mars")) == 0) radius_km = 3389.5;
    if (body.CmpNoCase(_("Jupiter")) == 0) radius_km = 69911.0;
    if (body.CmpNoCase(_("Saturn")) == 0) radius_km = 58232.0;
    if (radius_km > 0.0 && radius_km < distance_km)
      sample.semidiameter_deg = std::asin(radius_km / distance_km) * to_deg;
  }
  return sample;
}

void LunarToolsDialog::PredictCalibrationPair(wxCommandEvent&) {
  if (m_calFirstBody->GetSelection() == wxNOT_FOUND ||
      m_calSecondBody->GetSelection() == wxNOT_FOUND ||
      m_calFirstBody->GetStringSelection() ==
          m_calSecondBody->GetStringSelection()) {
    m_calPrediction->SetLabel(_("Choose two different bodies."));
    m_lastPredictionDeg = NAN;
    return;
  }
  double latitude = 0.0;
  double longitude = 0.0;
  if (!m_calLatitude->GetAngle(&latitude) ||
      !m_calLongitude->GetAngle(&longitude)) {
    m_calPrediction->SetLabel(_("Enter a valid latitude and longitude."));
    m_lastPredictionDeg = NAN;
    return;
  }
  m_calLatitude->Normalize();
  m_calLongitude->Normalize();
  sextant_calibration::Environment environment;
  environment.observer = {latitude, longitude};
  environment.pressure_hpa = m_calPressure->GetValue();
  environment.temperature_c = m_calTemperature->GetValue();
  const wxDateTime utc = CalibrationUtc();
  if (!utc.IsValid()) {
    m_calPrediction->SetLabel(_("Enter a valid UTC date and time."));
    m_lastPredictionDeg = NAN;
    return;
  }
  const auto result = sextant_calibration::PredictApparentCenterDistance(
      SampleBody(m_calFirstBody->GetStringSelection(), utc),
      SampleBody(m_calSecondBody->GetStringSelection(), utc), environment);
  if (!result.valid) {
    m_calPrediction->SetLabel(_("Unavailable: ") +
                              wxString::FromUTF8(result.error.c_str()));
    m_lastPredictionDeg = NAN;
    return;
  }
  const int contact = m_calContact->GetSelection();
  m_lastPredictionDeg =
      contact == 1 ? result.apparent_near_contact_distance_deg
                   : (contact == 2 ? result.apparent_far_contact_distance_deg
                                   : result.apparent_center_distance_deg);
  m_calPrediction->SetLabel(wxString::Format(
      _("%s %s; centre %s; altitudes %s / %s (Δ %s)%s"),
      m_calContact->GetStringSelection(),
      FormatNavigationAngle(m_lastPredictionDeg).c_str(),
      FormatNavigationAngle(result.apparent_center_distance_deg).c_str(),
      FormatNavigationAngle(result.first_altitude_deg).c_str(),
      FormatNavigationAngle(result.second_altitude_deg).c_str(),
      FormatNavigationAngle(result.altitude_difference_deg).c_str(),
      result.altitude_difference_deg > 15.0
          ? _(" — prefer a more equal-altitude star pair")
          : wxString()));
  m_calObservedAngle->SetAngle(m_lastPredictionDeg);
}

void LunarToolsDialog::AddCalibrationReading(wxCommandEvent&) {
  if (!std::isfinite(m_lastPredictionDeg)) {
    wxMessageBox(_("Calculate a valid pair prediction first."),
                 _("Sextant check"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  sextant_calibration::CheckReading reading;
  reading.predicted_deg = m_lastPredictionDeg;
  if (!m_calObservedAngle->GetAngle(&reading.observed_deg)) {
    wxMessageBox(_("Enter a valid observed angle."), _("Sextant check"),
                 wxOK | wxICON_ERROR, this);
    return;
  }
  m_calObservedAngle->Normalize();
  reading.uncertainty_arcmin = m_calUncertainty->GetValue();
  reading.note = m_calNote->GetValue().ToStdString();
  m_calibrationReadings.push_back(reading);
  const long row =
      m_calReadings->InsertItem(m_calReadings->GetItemCount(),
                                FormatNavigationAngle(reading.predicted_deg));
  m_calReadings->SetItem(row, 1, FormatNavigationAngle(reading.observed_deg));
  m_calReadings->SetItem(
      row, 2,
      wxString::Format("%+0.2f'",
                       (reading.predicted_deg - reading.observed_deg) * 60.0));
  m_calReadings->SetItem(
      row, 3, wxString::Format("±%.2f'", reading.uncertainty_arcmin));
  m_calReadings->SetItem(row, 4, wxString::FromUTF8(reading.note.c_str()));
}

void LunarToolsDialog::RemoveCalibrationReading(wxCommandEvent&) {
  const long selected =
      m_calReadings->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selected < 0) return;
  m_calibrationReadings.erase(m_calibrationReadings.begin() + selected);
  m_calReadings->DeleteItem(selected);
}

void LunarToolsDialog::SaveCalibrationProfile(wxCommandEvent&) {
  if (m_calibrationReadings.size() < 2) {
    wxMessageBox(_("Add at least two repeated or multi-angle checks."),
                 _("Sextant profile"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  const std::string name = m_profileName->GetValue().ToStdString();
  auto profile = sextant_calibration::BuildProfile(
      name, m_profileSerial->GetValue().ToStdString(),
      UtcDateTime::FormatIsoUtc(UtcDateTime::Now()).ToStdString(),
      m_calibrationReadings);
  auto existing =
      std::find_if(m_profiles.begin(), m_profiles.end(),
                   [&name](const sextant_calibration::Profile& value) {
                     return value.name == name;
                   });
  if (existing == m_profiles.end())
    m_profiles.push_back(profile);
  else
    *existing = profile;
  m_profileChoice->Clear();
  for (const auto& saved : m_profiles)
    m_profileChoice->Append(wxString::FromUTF8(saved.name.c_str()));
  const auto selected =
      std::find_if(m_profiles.begin(), m_profiles.end(),
                   [&name](const sextant_calibration::Profile& value) {
                     return value.name == name;
                   });
  m_profileChoice->SetSelection(
      static_cast<int>(std::distance(m_profiles.begin(), selected)));
  PersistProfiles();
  wxString points;
  for (const auto& point : profile.points) {
    if (!points.empty()) points += _("; ");
    points += wxString::Format(_("%s: %+0.2f′ ±%.2f′ (%d)"),
                               FormatNavigationAngle(point.angle_deg).c_str(),
                               point.correction_arcmin,
                               point.uncertainty_arcmin, point.reading_count);
  }
  m_profileSummary->SetLabel(wxString::Format(
      _("Saved profile “%s” (%s). Add the interpolated correction to a raw "
        "sextant reading. Repeatability %.2f′. Points: %s. Never extrapolate "
        "this table as evidence that mechanical adjustment is unnecessary."),
      wxString::FromUTF8(profile.name.c_str()),
      wxString::FromUTF8(profile.serial_number.c_str()),
      profile.repeatability_arcmin, points));
  m_profileSummary->Wrap(1000);
  UpdateProfileCorrection();
}

void LunarToolsDialog::SelectCalibrationProfile(wxCommandEvent&) {
  const int selected = m_profileChoice->GetSelection();
  if (selected < 0 || static_cast<std::size_t>(selected) >= m_profiles.size())
    return;
  const auto& profile = m_profiles[static_cast<std::size_t>(selected)];
  m_profileName->SetValue(wxString::FromUTF8(profile.name.c_str()));
  m_profileSerial->SetValue(wxString::FromUTF8(profile.serial_number.c_str()));
  m_profileSummary->SetLabel(wxString::Format(
      _("Profile “%s”: %zu correction points; repeatability %.2f′; created %s. "
        "Corrections are advisory and never rewrite observations."),
      wxString::FromUTF8(profile.name.c_str()), profile.points.size(),
      profile.repeatability_arcmin,
      wxString::FromUTF8(profile.created_utc.c_str())));
  m_profileSummary->Wrap(1000);
  UpdateProfileCorrection();
}

void LunarToolsDialog::UpdateProfileCorrection() {
  const int selected = m_profileChoice->GetSelection();
  if (selected < 0 || static_cast<std::size_t>(selected) >= m_profiles.size()) {
    m_profileCorrection->SetLabel(
        _("Active-profile correction at the entered observed angle: —"));
    return;
  }
  const auto& profile = m_profiles[static_cast<std::size_t>(selected)];
  double angle = 0.0;
  if (!m_calObservedAngle->GetAngle(&angle)) {
    m_profileCorrection->SetLabel(
        _("Active-profile correction: enter a valid observed angle."));
    return;
  }
  double uncertainty = 0.0;
  const double correction =
      sextant_calibration::CorrectionAt(profile, angle, &uncertainty);
  const bool outside = angle < profile.points.front().angle_deg ||
                       angle > profile.points.back().angle_deg;
  m_profileCorrection->SetLabel(wxString::Format(
      _("Active-profile correction at %s: %+0.2f′ ±%.2f′%s"),
      FormatNavigationAngle(angle).c_str(), correction, uncertainty,
      outside ? _(" — outside tested range; nearest endpoint only")
              : wxString()));
}

void LunarToolsDialog::LoadProfiles() {
  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_("/PlugIns/CelestialNavigation/SextantProfiles"));
  long count = 0;
  config->Read(_("Count"), &count, 0L);
  for (long index = 0; index < count; ++index) {
    sextant_calibration::Profile profile;
    const wxString prefix = wxString::Format("P%ld_", index);
    wxString value;
    config->Read(prefix + _("Name"), &value);
    profile.name = value.ToStdString();
    config->Read(prefix + _("Serial"), &value);
    profile.serial_number = value.ToStdString();
    config->Read(prefix + _("Created"), &value);
    profile.created_utc = value.ToStdString();
    config->Read(prefix + _("Repeatability"), &profile.repeatability_arcmin,
                 0.0);
    config->Read(prefix + _("Points"), &value);
    std::stringstream stream(value.ToStdString());
    std::string point;
    while (std::getline(stream, point, ';')) {
      std::replace(point.begin(), point.end(), ',', ' ');
      std::stringstream fields(point);
      sextant_calibration::CorrectionPoint item;
      if (fields >> item.angle_deg >> item.correction_arcmin >>
          item.uncertainty_arcmin >> item.reading_count)
        profile.points.push_back(item);
    }
    if (!profile.name.empty() && !profile.points.empty())
      m_profiles.push_back(profile);
  }
  config->SetPath(_("/PlugIns/CelestialNavigation"));
  if (!m_profiles.empty()) {
    for (const auto& profile : m_profiles)
      m_profileChoice->Append(wxString::FromUTF8(profile.name.c_str()));
    m_profileChoice->SetSelection(static_cast<int>(m_profiles.size() - 1));
    const auto& profile = m_profiles.back();
    m_profileName->SetValue(wxString::FromUTF8(profile.name.c_str()));
    m_profileSerial->SetValue(
        wxString::FromUTF8(profile.serial_number.c_str()));
    m_profileSummary->SetLabel(wxString::Format(
        _("Loaded %zu saved profile(s); active “%s”, %zu correction points, "
          "repeatability %.2f′."),
        m_profiles.size(), wxString::FromUTF8(profile.name.c_str()),
        profile.points.size(), profile.repeatability_arcmin));
    UpdateProfileCorrection();
  }
}

void LunarToolsDialog::PersistProfiles() {
  wxFileConfig* config = GetOCPNConfigObject();
  // Delete only our profile subgroup.  wxConfig::DeleteAll would erase the
  // entire OpenCPN configuration, not merely the current path.
  config->SetPath(_("/PlugIns/CelestialNavigation"));
  config->DeleteGroup(_("SextantProfiles"));
  config->SetPath(_("SextantProfiles"));
  config->Write(_("Count"), static_cast<long>(m_profiles.size()));
  for (std::size_t index = 0; index < m_profiles.size(); ++index) {
    const auto& profile = m_profiles[index];
    const wxString prefix = wxString::Format("P%zu_", index);
    config->Write(prefix + _("Name"), wxString::FromUTF8(profile.name.c_str()));
    config->Write(prefix + _("Serial"),
                  wxString::FromUTF8(profile.serial_number.c_str()));
    config->Write(prefix + _("Created"),
                  wxString::FromUTF8(profile.created_utc.c_str()));
    config->Write(prefix + _("Repeatability"), profile.repeatability_arcmin);
    wxString points;
    for (const auto& point : profile.points) {
      if (!points.empty()) points += ";";
      points += wxString::Format("%.10g,%.10g,%.10g,%d", point.angle_deg,
                                 point.correction_arcmin,
                                 point.uncertainty_arcmin, point.reading_count);
    }
    config->Write(prefix + _("Points"), points);
  }
  config->Flush();
  config->SetPath(_("/PlugIns/CelestialNavigation"));
}

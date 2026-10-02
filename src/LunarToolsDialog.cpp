#include "PlatformMessageBox.h"
#ifdef __OCPN__ANDROID__
#include "AndroidJob.h"
#include "AndroidSurface.h"
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTimer>
#include <wx/weakref.h>
#endif
#include "LunarToolsDialog.h"
#include "Dut1UpdatePanel.h"

#include "BodyCatalog.h"
#include "CelestialNavigationDialog.h"
#include "DialogGeometry.h"
#include "NavigationAlgorithms.h"
#include "NavigationUIUtils.h"
#include "OcpnApiCompat.h"
#include "Sight.h"
#include "LunarSessionWorker.h"
#include "UtcDateTime.h"
#include "Utf8Translation.h"
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
#include <cmath>
#include <memory>
#include <sstream>

namespace {

// POBsoft (1985-2026): touch forms have one full-width field per row.
#ifdef __OCPN__ANDROID__
constexpr int kToolFormOrientation = wxVERTICAL;
#else
constexpr int kToolFormOrientation = wxHORIZONTAL;
#endif

std::string ProfileText(const wxString& value) {
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): preserve user text as UTF8 across the pinned wxQt
  // locale conversion and std::string profile model; display/storage use UTF8.
  const auto utf8 = value.ToUTF8();
  return utf8.data() ? std::string(utf8.data(), utf8.length()) : std::string();
#else
  return value.ToStdString();
#endif
}

wxSpinCtrlDouble* Spin(wxWindow* parent, double minimum, double maximum,
                       double value, double increment, int digits = 2) {
  auto* control = new wxSpinCtrlDouble(parent, wxID_ANY);
  control->SetRange(minimum, maximum);
  control->SetValue(value);
  control->SetIncrement(increment);
  control->SetDigits(digits);
#ifdef __OCPN__ANDROID__
  control->SetDigits(15); control->SetValue(value);
#endif
  return control;
}

wxBoxSizer* LabelControl(wxWindow* parent, const wxString& label,
                         wxWindow* control) {
  auto* sizer = new wxBoxSizer(kToolFormOrientation);
  sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  sizer->Add(control,
#ifdef __OCPN__ANDROID__
             0,
#else
             1,
#endif
             wxEXPAND);
  return sizer;
}

wxDateTime PickerUtc(wxDatePickerCtrl* date, CelestialTimePicker* time) {
  const wxDateTime d = date->GetValue();
  const wxDateTime t = time->GetValue();
#ifdef __OCPN__ANDROID__
  const auto fields = UtcDateTime::Fields(t);
  return UtcDateTime::FromCalendar(d, fields.hour, fields.min,
                                  fields.sec + fields.msec / 1000.0);
#else
  return wxDateTime(d.GetDay(), d.GetMonth(), d.GetYear(), t.GetHour(),
                    t.GetMinute(), t.GetSecond());
#endif
}

double AngularDistance(double lat1, double lon1, double lat2, double lon2) {
  constexpr double to_rad = 3.14159265358979323846 / 180.0;
  const double cosine = std::sin(lat1 * to_rad) * std::sin(lat2 * to_rad) +
                        std::cos(lat1 * to_rad) * std::cos(lat2 * to_rad) *
                            std::cos((lon1 - lon2) * to_rad);
  return std::acos(std::max(-1.0, std::min(1.0, cosine))) / to_rad;
}

wxString FormatPlannerAltitude(double degrees) {
  return degrees < 0.0 ? wxString("-") + FormatNavigationAngle(-degrees)
                       : FormatNavigationAngle(degrees);
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
  m_notebook->AddPage(celestial_navigation::CreateDut1UpdatePanel(m_notebook), _("Advanced"));
  UpdateUtcEntryVisibility();
  m_entryFormat->Bind(wxEVT_CHOICE, &LunarToolsDialog::ChangeUtcEntryFormat,
                      this);
  top->Add(m_notebook, 1, wxEXPAND | wxALL, 8);
  auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); });
  auto* bottom = new wxBoxSizer(wxHORIZONTAL);
  auto* saved = new wxButton(this, wxID_ANY, _("Saved lunar solutions"));
  saved->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    m_parentDialog->ShowLunarSolutions(this);
  });
  bottom->Add(saved, 0, wxALL, 8);
  bottom->AddStretchSpacer();
  bottom->Add(close, 0, wxALL, 8);
  top->Add(bottom, 0, wxEXPAND);
  SetSizer(top);
  SetMinSize(wxSize(880, 650));
  dialog_geometry::Restore(this, _T("LunarTools"), wxSize(1120, 780));
  LoadProfiles();
}

LunarToolsDialog::~LunarToolsDialog() {
  dialog_geometry::Save(this, _T("LunarTools"));
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
      new CelestialTimePicker(controls->timeContainer, wxID_ANY);
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
#ifdef __OCPN__ANDROID__
  controls->nativeDate->SetValue(UtcDateTime::CalendarDate(utc));
#else
  controls->nativeDate->SetValue(utc);
#endif
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
      CelestialMessageBox(_("Enter UTC date as YYYY-MM-DD and 24-hour time as "
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
  auto* upper = new wxBoxSizer(kToolFormOrientation);
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
        CN_UTF8_("%s  Moon–%s  %s"),
        UtcDateTime::FormatUtc(sight.m_DateTime, "%Y-%m-%d %H:%M:%S"),
        sight.m_Body, FormatNavigationAngle(sight.m_Measurement).c_str()));
    if (highlightedUtc.IsValid() &&
        std::fabs(UtcDateTime::SecondsBetween(sight.m_DateTime,
                                              highlightedUtc)) <= 6.0 * 3600.0)
      m_sequenceSights->Check(m_sequenceSights->GetCount() - 1, true);
  }
  auto* sightColumn = new wxBoxSizer(wxVERTICAL);
#ifdef __OCPN__ANDROID__
  m_sequenceSights->Hide();
  m_androidSequenceCards = new wxPanel(page);
  m_androidSequenceCards->SetSizer(new wxBoxSizer(wxVERTICAL));
  sightColumn->Add(m_androidSequenceCards, 0, wxEXPAND | wxALL, 5);
#else
  sightColumn->Add(m_sequenceSights, 1, wxEXPAND | wxALL, 5);
#endif
  auto* selectionButtons = new wxBoxSizer(kToolFormOrientation);
  auto* selectVisible =
      new wxButton(page, wxID_ANY, _("Select visible sights"));
  auto* clearSelection = new wxButton(page, wxID_ANY, _("Clear selection"));
  selectionButtons->Add(selectVisible, 0, wxRIGHT, 6);
  selectionButtons->Add(clearSelection, 0);
  sightColumn->Add(selectionButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  m_sequenceReference = new wxStaticText(page, wxID_ANY, wxEmptyString);
  sightColumn->Add(m_sequenceReference, 0,
                   wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);
  upper->Add(sightColumn,
#ifdef __OCPN__ANDROID__
             0,
#else
             1,
#endif
             wxEXPAND);
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
  auto* positionRow = new wxBoxSizer(kToolFormOrientation);
  m_sequencePositionSource = new wxStaticText(page, wxID_ANY, wxEmptyString);
  auto* useEarliest =
      new wxButton(page, wxID_ANY, _("Use earliest selected DR"));
  positionRow->Add(m_sequencePositionSource, 1,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  positionRow->Add(useEarliest, 0);
  settings->Add(positionRow, 0, wxEXPAND | wxALL, 3);
  m_sequenceSearchHours = Spin(page, 0.25, 24.0, 12.0, 0.5, 1);
  settings->Add(
      LabelControl(page, CN_UTF8_("Additional watch correction search ±"),
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
  auto* motion = new wxBoxSizer(kToolFormOrientation);
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
  top->Add(upper,
#ifdef __OCPN__ANDROID__
           0,
#else
           1,
#endif
           wxEXPAND);

  auto* resultHeader = new wxBoxSizer(kToolFormOrientation);
  m_sequenceCandidate = new wxChoice(page, wxID_ANY);
  m_sequenceCandidate->Bind(wxEVT_CHOICE, &LunarToolsDialog::SelectCandidate,
                            this);
  resultHeader->Add(new wxStaticText(page, wxID_ANY, _("Solution candidate")),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  resultHeader->Add(m_sequenceCandidate, 0, wxRIGHT, 10);
  resultHeader->AddStretchSpacer();
  m_applySequence = new wxButton(page, wxID_ANY, _("Save lunar solution"));
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
#ifdef __OCPN__ANDROID__
  m_sequenceResiduals->Hide();
  m_androidSequenceResiduals = new wxPanel(page);
  m_androidSequenceResiduals->SetSizer(new wxBoxSizer(wxVERTICAL));
  top->Add(m_androidSequenceResiduals, 0, wxEXPAND | wxALL, 6);
  // Remove desktop horizontal/stretch flags also from nested labelled groups.
  const std::vector<wxSizer*> groups = {sightColumn, selectionButtons, settings,
      positionRow, motion, resultHeader};
  for (auto* group : groups)
    for (auto* item : group->GetChildren()) {
      item->SetProportion(0);
      item->SetFlag(wxEXPAND | wxALL);
      item->SetBorder(8);
    }
#else
  top->Add(m_sequenceResiduals, 1, wxEXPAND | wxALL, 6);
#endif
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
#ifdef __OCPN__ANDROID__
    InvalidateAndroidSequence();
#endif
  });
  m_sequenceLongitude->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
    m_sequencePositionAutomatic = false;
    m_sequencePositionSource->SetLabel(_("Manual position"));
#ifdef __OCPN__ANDROID__
    InvalidateAndroidSequence();
#endif
  });
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): changing solver inputs invalidates its saved snapshot.
  m_sequenceMode->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    InvalidateAndroidSequence();
  });
  for (auto* control : {m_sequenceRobust, m_sequenceBias, m_sequenceMotion})
    control->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
      InvalidateAndroidSequence();
    });
  for (auto* control : {m_sequenceSearchHours, m_sequenceCog, m_sequenceSog})
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(control->GetHandle())) {
      wxWeakRef<LunarToolsDialog> weak(this);
      QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       GetHandle(), [weak](double) {
        if (!weak) return;
        QTimer::singleShot(0, weak->GetHandle(), [weak]() {
          if (weak) weak->InvalidateAndroidSequence();
        });
      });
    }
#endif
  UpdateSequenceSelection();
}

void LunarToolsDialog::BuildPlannerPage(wxWindow* page) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* note = new wxStaticText(
      page, wxID_ANY,
      CN_UTF8_("Pairs use the same ordering as Bodies & Best Sights: fewest "
               "planning cautions, then 0.1′ time; timing-first is optional. "
               "Below-horizon pairs come last. The time is the approximate "
               "UTC change corresponding to 0.1′ of lunar distance. "
               "Visibility and instrument range require your judgement."));
  note->Wrap(820);
  top->Add(note, 0, wxEXPAND | wxALL, 8);
  auto* controls = new wxBoxSizer(wxVERTICAL);
  auto* positionRow = new wxBoxSizer(kToolFormOrientation);
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
  auto* calculate = new wxButton(page, wxID_ANY, _("Calculate pairs"));
  calculate->Bind(wxEVT_BUTTON, &LunarToolsDialog::CalculatePlanner, this);
  positionRow->Add(calculate, 0);
  controls->Add(positionRow, 0, wxEXPAND | wxBOTTOM, 5);
  auto* timeRow = new wxBoxSizer(kToolFormOrientation);
  timeRow->Add(LabelControl(page, _("UTC date"), m_plannerUtc.dateContainer), 1,
               wxRIGHT, 6);
  timeRow->Add(LabelControl(page, _("UTC time"), m_plannerUtc.timeContainer), 1,
               wxRIGHT, 6);
  controls->Add(timeRow, 0, wxEXPAND);
  top->Add(controls, 0, wxEXPAND | wxALL, 6);
  m_plannerOrder = new wxChoice(page, wxID_ANY);
  m_plannerOrder->Append(_("Lunar order: fewest cautions, then timing"));
  m_plannerOrder->Append(_("Lunar order: timing sensitivity"));
  m_plannerOrder->SetSelection(0);
  m_plannerOrder->Bind(wxEVT_CHOICE, &LunarToolsDialog::CalculatePlanner, this);
  top->Add(m_plannerOrder, 0, wxALL, 6);
  m_plannerList = new wxListCtrl(page, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, wxLC_REPORT | wxBORDER_SUNKEN);
  const wxString columns[] = {
      _("Body"),          _("Below horizon"), _("Distance"),
      _("Rate"),          CN_UTF8_("0.1′ time"),
      _("Moon altitude"), _("Body altitude"),
      _("Moon Zn true"),  _("Body Zn true"),
      _("Moon illum."),   _("Magnitude"),
      _("Ecliptic lat"),  _("Guidance")};
  const int widths[] = {145, 125, 125, 100, 100, 130, 130, 105, 105,
                        100, 95, 110, 320};
  for (int index = 0; index < 13; ++index) {
    m_plannerList->InsertColumn(index, columns[index]);
    m_plannerList->SetColumnWidth(index, widths[index]);
  }
#ifdef __OCPN__ANDROID__
  m_plannerList->Hide();
  m_androidPairCards = new wxPanel(page);
  m_androidPairCards->SetSizer(new wxBoxSizer(wxVERTICAL));
  top->Add(m_androidPairCards, 0, wxEXPAND | wxALL, 6);
  const std::vector<wxSizer*> groups = {positionRow, timeRow};
  for (auto* group : groups)
    for (auto* item : group->GetChildren()) {
      item->SetProportion(0);
      item->SetFlag(wxEXPAND | wxALL);
      item->SetBorder(8);
    }
#else
  top->Add(m_plannerList, 1, wxEXPAND | wxALL, 6);
#endif
  page->SetSizer(top);
}

void LunarToolsDialog::BuildCalibrationPage(wxWindow* page) {
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): the surface owns one scrolling form. Keep each
  // labelled field full-width and render repeats from their numerical model.
  const int formOrientation = wxVERTICAL;
#else
  const int formOrientation = wxHORIZONTAL;
#endif
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* note = new wxStaticText(
      page, wxID_ANY,
      CN_UTF8_("Observational check—not mechanical adjustment. Correct "
               "perpendicularity, side, collimation and index error first. "
               "Similar-altitude stars reveal scale/centering; Moon pairs "
               "require accurate UTC and position."));
#ifndef __OCPN__ANDROID__
  note->Wrap(800);
#endif
  top->Add(note, 0, wxEXPAND | wxALL, 8);
  auto* prediction =
      new wxStaticBoxSizer(wxVERTICAL, page, _("Offline pair prediction"));
  auto* row1 = new wxBoxSizer(formOrientation);
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
  auto* utcRow = new wxBoxSizer(formOrientation);
  utcRow->Add(LabelControl(page, _("UTC date"), m_calUtc.dateContainer), 1,
              wxRIGHT, 5);
  utcRow->Add(LabelControl(page, _("UTC time"), m_calUtc.timeContainer), 1);
  prediction->Add(utcRow, 0, wxEXPAND | wxALL, 3);
  auto* row2 = new wxBoxSizer(formOrientation);
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
  auto* row3 = new wxBoxSizer(formOrientation);
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  m_calPressure = Spin(page, 0.0, 1100.0, defaults.pressure, 1.0, 1);
  m_calTemperature = Spin(page, -60.0, 60.0, defaults.temperature, 1.0, 1);
  row3->Add(LabelControl(page, _("Pressure hPa"), m_calPressure), 0, wxRIGHT,
            8);
  row3->Add(LabelControl(page, CN_UTF8_("Temperature °C"), m_calTemperature), 0,
            wxRIGHT, 12);
  m_calIndexError =
      Spin(page, -60.0, 60.0, defaults.indexError, 0.1, 2);
  m_calIndexError->SetToolTip(
      _("Enter an independently measured index error. On the arc is positive; "
        "the corrected apparent angle is raw minus IE."));
  row3->Add(LabelControl(page, CN_UTF8_("Measured IE (on arc +) ′"),
                         m_calIndexError),
            0, wxRIGHT, 12);
  prediction->Add(row3, 0, wxEXPAND | wxALL, 3);
  m_calPrediction = new wxStaticText(page, wxID_ANY, _("Not calculated"));
  prediction->Add(m_calPrediction, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                  6);
  top->Add(prediction, 0, wxEXPAND | wxALL, 5);

  auto* entry = new wxBoxSizer(formOrientation);
  m_calObservedAngle = new NavigationAngleCtrl(
      page, NavigationAngleKind::Generic, 0.0, 0.0, 180.0, wxSize(165, -1));
  m_calUncertainty = Spin(page, 0.05, 10.0, 0.2, 0.05, 2);
  m_calNote = new wxTextCtrl(page, wxID_ANY);
  entry->Add(LabelControl(page, _("Observed angle"), m_calObservedAngle), 2,
             wxRIGHT, 5);
  entry->Add(LabelControl(page, CN_UTF8_("uncertainty ±′"), m_calUncertainty),
             1, wxRIGHT, 5);
  entry->Add(LabelControl(page, _("note / shade"), m_calNote), 2, wxRIGHT, 5);
  auto* add = new wxButton(page, wxID_ANY, _("Add repeat"));
  add->Bind(wxEVT_BUTTON, &LunarToolsDialog::AddCalibrationReading, this);
  entry->Add(add, 0);
  top->Add(entry, 0, wxEXPAND | wxALL, 6);
  m_calReadings =
      new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, 170),
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
  m_calReadings->SetMinSize(wxSize(-1, 70));
  const wxString columns[] = {
      _("Predicted apparent"), _("Raw observed"), _("IE (on arc +)"),
      _("After IE"), _("Residual to add"), _("Uncertainty"), _("Note")};
  const int widths[] = {145, 135, 105, 135, 135, 110, 230};
  for (int index = 0; index < 7; ++index) {
    m_calReadings->InsertColumn(index, columns[index]);
    m_calReadings->SetColumnWidth(index, widths[index]);
  }
#ifdef __OCPN__ANDROID__
  m_calReadings->Hide();
  m_androidCalReadings = new wxPanel(page);
  m_androidCalReadings->SetSizer(new wxBoxSizer(wxVERTICAL));
  top->Add(m_androidCalReadings, 0, wxEXPAND | wxALL, 6);
#else
  top->Add(m_calReadings, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
#endif
  auto* profile = new wxStaticBoxSizer(formOrientation, page,
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
      CN_UTF8_("Active-profile correction at the entered observed angle: —"));
  top->Add(m_profileCorrection, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  m_profileSummary =
      new wxStaticText(page, wxID_ANY,
                       _("No profile built. New profiles contain residual "
                         "scale/centering corrections applied after index "
                         "error; raw observations are never rewritten."));
  m_profileSummary->Wrap(1000);
  top->Add(m_profileSummary, 0, wxEXPAND | wxALL, 6);
  m_calObservedAngle->Bind(
      wxEVT_TEXT, [this](wxCommandEvent&) { UpdateProfileCorrection(); });
  m_calIndexError->Bind(
      wxEVT_SPINCTRLDOUBLE,
      [this](wxSpinDoubleEvent&) { UpdateProfileCorrection(); });
  m_calIndexError->Bind(
      wxEVT_TEXT, [this](wxCommandEvent&) { UpdateProfileCorrection(); });
#ifdef __OCPN__ANDROID__
  // Nested sizer items retain their desktop width flags even when their
  // orientation changes. Expand the labelled groups as well as each control.
  const std::vector<wxSizer*> fieldGroups{row1, utcRow, row2, row3, entry, profile};
  for (wxSizer* group : fieldGroups)
    for (auto* item : group->GetChildren()) {
      item->SetProportion(0);
      item->SetFlag(wxEXPAND | wxALL);
      item->SetBorder(8);
    }
  if (auto* spin = qobject_cast<QDoubleSpinBox*>(m_calIndexError->GetHandle()))
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     GetHandle(), [this](double) {
      QTimer::singleShot(0, GetHandle(), [this]() { UpdateProfileCorrection(); });
    });
  // POBsoft (1985-2026): native wxQt edits can omit wx command events.
  // Compare semantic prediction inputs on the next owned GUI turn, after
  // the native control has finished updating its model.
  wxWeakRef<LunarToolsDialog> weak(this);
  const auto changed = [weak]() {
    if (!weak) return;
    QTimer::singleShot(0, weak->GetHandle(), [weak]() {
      if (weak) weak->CheckAndroidCalibrationPrediction();
    });
  };
  const std::vector<wxWindow*> predictionInputs{
      m_calLatitude, m_calLongitude, m_calUtc.dateContainer,
      m_calUtc.timeContainer, m_calFirstBody, m_calSecondBody,
      m_calContact, m_calPressure, m_calTemperature};
  for (auto* input : predictionInputs) {
    auto* widget = input->GetHandle();
    auto edits = widget->findChildren<QLineEdit*>();
    if (auto* edit = qobject_cast<QLineEdit*>(widget)) edits.append(edit);
    for (auto* edit : edits)
      QObject::connect(edit, &QLineEdit::textChanged, GetHandle(), changed);
    if (auto* choice = qobject_cast<QComboBox*>(widget))
      QObject::connect(choice, QOverload<int>::of(&QComboBox::currentIndexChanged),
                       GetHandle(), changed);
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget))
      QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       GetHandle(), changed);
  }
#endif
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
#ifdef __OCPN__ANDROID__
  m_applySequence->Enable(false);
  m_sequenceResult.valid = false;
  m_sequenceCandidate->Clear();
  m_sequenceSummary->SetLabel(_("Select at least two saved lunar observations, then solve the session."));
  RefreshAndroidSequence();
#endif
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
#ifdef __OCPN__ANDROID__
  m_sequenceResult.valid = false;
  m_sequenceCandidate->Clear();
  RefreshAndroidSequence();
#endif
  m_applySequence->Enable(false);
  m_sequenceRecords.clear();
  double sequenceLatitude = 0.0;
  double sequenceLongitude = 0.0;
  if (!m_sequenceLatitude->GetAngle(&sequenceLatitude) ||
      !m_sequenceLongitude->GetAngle(&sequenceLongitude)) {
    CelestialMessageBox(_("Enter a valid initial or known position."),
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
    CelestialMessageBox(_("Select at least two lunar observations."),
                 _("Lunar sequence"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  if (selectedLists.size() > 12) {
    CelestialMessageBox(
        _("A lunar sequence is one watch/session. Select no more than 12 "
          "observations; use Clear selection or Select visible sights to "
          "choose a coherent group."),
        _("Lunar sequence"), wxOK | wxICON_WARNING, this);
    return;
  }
  const double sessionSpan =
      std::fabs(UtcDateTime::SecondsBetween(latest, reference));
  if (sessionSpan > 24.0 * 3600.0) {
    CelestialMessageBox(
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
#ifdef __OCPN__ANDROID__
    // POBsoft (1985-2026): bind the model to this retained snapshot, including
    // cold-loaded inputs which have never run an individual lunar search.
    snapshot->m_CorrectedDateTime = UtcDateTime::AddSeconds(
        snapshot->m_DateTime, m_parentDialog->GetClockCorrection());
    snapshot->RecomputeLunar(-1, true);
#else
    snapshot->Recompute(m_parentDialog->GetClockCorrection());
#endif
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
#ifdef __OCPN__ANDROID__
    // POBsoft (1985-2026): ToStdString in the pinned wxQt narrows Unicode
    // code points. Preserve UTF8 before these labels enter the saved XML trail.
    const wxString label = UtcDateTime::FormatUtc(
        sight.m_CorrectedDateTime, "%H:%M:%S") + CN_UTF8_(" Moon–") + sight.m_Body;
    entry.label = label.ToUTF8().data();
#else
    entry.label = wxString::Format(CN_UTF8_("%s Moon–%s"),
                                   UtcDateTime::FormatUtc(
                                       sight.m_CorrectedDateTime, "%H:%M:%S"),
                                   sight.m_Body)
                      .ToStdString();
#endif
    entry.settings = sight.LunarObservation();
    entry.epoch_offset_seconds =
        UtcDateTime::SecondsBetween(sight.m_CorrectedDateTime, reference);
    auto reading_id = [&](const wxString& body, double offset) {
      return (body + "@" +
              UtcDateTime::FormatUtc(
                  UtcDateTime::AddSeconds(sight.m_DateTime, offset),
                  "%Y-%m-%d %H:%M:%S.%l"))
          .ToStdString();
    };
    entry.reading_ids[0] = reading_id("LD:Moon-" + sight.m_Body, 0);
    entry.reading_ids[1] =
        reading_id("Hs:Moon", entry.settings.moon_time_offset_seconds);
    entry.reading_ids[2] = reading_id("Hs:" + sight.m_Body,
                                      entry.settings.body_time_offset_seconds);
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
#ifdef __OCPN__ANDROID__
  bool userCancelled = false;
  wxString workerError;
  lunar_session::Result candidate;
  const bool completedJob = celestial_android::RunJob(this, _("Solve lunar session"),
      [&](celestial_android::JobState& state) {
        options.cancel_requested = [&]() { return state.cancel.load(); };
        options.progress = [&](std::size_t completed, std::size_t total) {
          state.Progress("Testing bounded solution " + std::to_string(completed) + " of " + std::to_string(total));
        };
        candidate = lunar_session::Solve(entries, options);
      }, &workerError);
  if (!completedJob) {
    m_sequenceSummary->SetLabel(workerError.empty() ? _("Lunar session cancelled. Observations unchanged.") : workerError);
    celestial_android::LayoutScrolls(this);
    return;
  }
  m_sequenceResult = std::move(candidate);
#else
  celestial_navigation::LunarSessionWorker worker;
  wxString workerError;
  if (!worker.Start(entries, options, &workerError)) {
    m_sequenceSummary->SetLabel(workerError);
    return;
  }
  wxProgressDialog progress(
      _("Lunar sequence"), CN_UTF8_("Preparing bounded multi-start solution…"),
      100, this,
      wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME |
          wxPD_REMAINING_TIME | wxPD_SMOOTH | wxPD_AUTO_HIDE);
  bool userCancelled = false;
  while (!worker.TryTakeResult(&m_sequenceResult)) {
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
                : CN_UTF8_("Preparing bounded multi-start solution…"))) {
      userCancelled = true;
      cancelRequested.store(true);
    }
    wxMilliSleep(75);
    wxYieldIfNeeded();
  }
  if (!userCancelled) progress.Update(100, _("Lunar sequence complete."));
#endif
  if (std::any_of(snapshots.begin(),snapshots.end(),
                 [](const std::shared_ptr<Sight>& sight) { return sight->m_LunarDut1Fallback; }))
    m_sequenceResult.warnings.push_back(
        "Some trial dates lack Earth-rotation data (DUT1); UT1=UTC fallback, reduced accuracy.");
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
  for (std::size_t index = 0; index < m_sequenceResult.candidates.size();
       ++index) {
    const auto& candidate = m_sequenceResult.candidates[index];
    LunarSolutionRecord record;
    record.reference_time = UtcDateTime::FormatUtc(
        UtcDateTime::AddSeconds(reference,
                                -m_parentDialog->GetClockCorrection()),
        "%Y-%m-%d %H:%M:%S");
    record.method = options.solve_position
                        ? "WGS84 sequence: time and position"
                        : "WGS84 sequence: time at known position";
    record.base_correction_seconds = m_parentDialog->GetClockCorrection();
    record.additional_correction_seconds = candidate.clock_correction_seconds;
    record.time_sigma_seconds = candidate.time_uncertainty_seconds;
    for (const auto& snapshot : snapshots)
      record.inputs.push_back(LunarInputSnapshot(*snapshot));
    record.report = wxString::Format(
        "Candidate %zu; reference position %.9f, %.9f; position sigma %.3f NM\n"
        "Motion %d; COG %.3f; SOG %.3f; robust fit %d; fit index bias %d\n"
        "Index bias %.6f arcmin; weighted RMS %.6f; condition %.6g\n"
        "Residual nan means a shared/excluded reading, not a zero error.\n",
        index + 1, candidate.reference_position.latitude_deg,
        candidate.reference_position.longitude_deg,
        candidate.position_uncertainty_nm, int(options.moving_observer),
        options.course_true_deg, options.speed_knots, int(options.robust_fit),
        int(options.estimate_common_index_bias),
        candidate.common_index_bias_arcmin, candidate.weighted_rms,
        candidate.condition_number);
#ifdef __OCPN__ANDROID__
    if (!options.solve_position)
      record.report.Replace(wxString::Format("position sigma %.3f NM",
                                             candidate.position_uncertainty_nm),
                            _("position held fixed"));
#endif
    for (const auto& residual : candidate.residuals)
      record.report +=
          wxString::FromUTF8(residual.label.c_str()) +
          wxString::Format(
              " residuals arcmin: LD %+.6f Moon %+.6f body %+.6f; outlier %d\n",
              residual.distance_arcmin, residual.moon_altitude_arcmin,
              residual.body_altitude_arcmin, int(residual.possible_outlier));
    for (const auto& warning : m_sequenceResult.warnings)
      record.report += wxString::FromUTF8(warning.c_str()) + "\n";
    m_sequenceRecords.push_back(record);
  }
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
      CN_UTF8_(
          "Additional watch correction %+0.1f s; reference position %.5f, "
          "%.5f; RMS %.2f′ (weighted %.2f); σtime %.1f s; σposition %.1f NM"),
      candidate.clock_correction_seconds,
      candidate.reference_position.latitude_deg,
      candidate.reference_position.longitude_deg, candidate.angular_rms_arcmin,
      candidate.weighted_rms, candidate.time_uncertainty_seconds,
      candidate.position_uncertainty_nm);
#ifdef __OCPN__ANDROID__
  // No covariance is fitted for a position explicitly held fixed.
  if (!std::isfinite(candidate.position_uncertainty_nm)) {
    const wxString value = wxString::Format(CN_UTF8_("σposition %.1f NM"),
                                            candidate.position_uncertainty_nm);
    const bool fixed = index < m_sequenceRecords.size() &&
        m_sequenceRecords[index].method == "WGS84 sequence: time at known position";
    summary.Replace(value, fixed ? _("Position held fixed")
                                 : _("Position uncertainty unavailable"));
  }
#endif
  if (candidate.common_index_bias_arcmin != 0.0)
    summary += wxString::Format(CN_UTF8_("; common index bias %+0.2f′"),
                                candidate.common_index_bias_arcmin);
  for (const auto& warning : m_sequenceResult.warnings)
    summary += CN_UTF8_(" — ") + wxString::FromUTF8(warning.c_str());
  if (m_sequenceResult.candidates.size() > 1)
    summary += _(" Multiple solutions remain: these are unresolved alternatives, "
                 "not repeated measurements to average. Use another observation "
                 "or independent position/time evidence to distinguish them.");
  m_sequenceSummary->SetLabel(summary);
#ifndef __OCPN__ANDROID__
  m_sequenceSummary->Wrap(1000);
#endif
  m_sequenceResiduals->DeleteAllItems();
  auto residual_text = [](double value) {
    return std::isfinite(value) ? wxString::Format("%+0.2f'", value)
                                : _("Shared/not used");
  };
  for (std::size_t row = 0; row < candidate.residuals.size(); ++row) {
    const auto& residual = candidate.residuals[row];
    const long item = m_sequenceResiduals->InsertItem(
        static_cast<long>(row), wxString::FromUTF8(residual.label.c_str()));
    m_sequenceResiduals->SetItem(item, 1,
                                 residual_text(residual.distance_arcmin));
    m_sequenceResiduals->SetItem(item, 2,
                                 residual_text(residual.moon_altitude_arcmin));
    m_sequenceResiduals->SetItem(item, 3,
                                 residual_text(residual.body_altitude_arcmin));
    m_sequenceResiduals->SetItem(
        item, 4,
        residual.possible_outlier ? wxString::Format(CN_UTF8_("Inspect: %.1fσ"),
                                                     residual.standardized_max)
                                  : _("Consistent"));
  }
#ifdef __OCPN__ANDROID__
  RefreshAndroidSequence();
#endif
}

void LunarToolsDialog::ApplySequenceCorrection(wxCommandEvent&) {
  const int selection = m_sequenceCandidate->GetSelection();
  if (selection == wxNOT_FOUND ||
      std::size_t(selection) >= m_sequenceRecords.size())
    return;
  if (m_parentDialog->SaveLunarSolution(m_sequenceRecords[selection])) {
    m_applySequence->Enable(false);
  }
}

void LunarToolsDialog::CalculatePlanner(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  m_androidPairCards->GetSizer()->Clear(true);
  celestial_android::LayoutScrolls(this);
#endif
  m_plannerList->DeleteAllItems();
  double observerLatitude = 0.0;
  double observerLongitude = 0.0;
  if (!m_plannerLatitude->GetAngle(&observerLatitude) ||
      !m_plannerLongitude->GetAngle(&observerLongitude)) {
    CelestialMessageBox(_("Enter a valid planner latitude and longitude."),
                 _("Lunar planner"), wxOK | wxICON_ERROR, this);
    return;
  }
  m_plannerLatitude->Normalize();
  m_plannerLongitude->Normalize();
  const wxDateTime utc = ReadUtcEntry(
      m_plannerUtc, m_entryFormat->GetSelection(), true, _("Lunar planner"));
  if (!utc.IsValid()) return;
  const wxDateTime instant = UtcDateTime::ToInstant(utc);
  const PlanningResult plan = PlannerRecommendations::Calculate(
      instant, observerLatitude, observerLongitude);
  const BodyState moon = CelestialEphemeris::Evaluate(
      "Moon", instant, observerLatitude, observerLongitude);
  auto rows = PlannerRecommendations::Order(
      plan, PlanningMode::LunarCandidates, true,
      m_plannerOrder->GetSelection() == 1);
  rows.erase(std::remove_if(rows.begin(), rows.end(),
      [](const RankedBody& b) { return !b.lunarValid; }), rows.end());
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const RankedBody& row = rows[index];
#ifdef __OCPN__ANDROID__
    wxString caption = row.state.body + "\n" + _("Below horizon: ") +
        (moon.geometricAltitude < 0.0 && row.state.geometricAltitude < 0.0
             ? _("Both") : moon.geometricAltitude < 0.0 ? _("Moon")
             : row.state.geometricAltitude < 0.0 ? _("Body") : _("Neither"));
    caption += "\n" + _("Distance: ") + FormatNavigationAngle(row.lunarDistance);
    caption += wxString::Format(CN_UTF8_("\nRate: %+.1f′/h\n0.1′ time: "),
                                row.lunarRateArcminHour);
    caption += std::isfinite(row.lunarTimingSeconds)
        ? wxString::Format("%.1f s", row.lunarTimingSeconds) : CN_UTF8_("—");
    caption += "\n" + _("Moon altitude: ") +
               FormatPlannerAltitude(moon.geometricAltitude);
    caption += "\n" + _("Body altitude: ") +
               FormatPlannerAltitude(row.state.geometricAltitude);
    caption += wxString::Format(
        CN_UTF8_("\nMoon Zn true: %.1f°\nBody Zn true: %.1f°"
                 "\nMoon illumination: %.1f%%\nMagnitude: %.1f"
                 "\nEcliptic latitude: %+.1f°"),
        moon.azimuthTrue, row.state.azimuthTrue,
        plan.moon.illuminatedFraction * 100.0,
        row.state.visualMagnitude, row.eclipticLatitude);
    if (!row.lunarReason.empty()) caption += "\n" + row.lunarReason;
    m_androidPairCards->GetSizer()->Add(
        new wxStaticText(m_androidPairCards, wxID_ANY, caption),
        0, wxEXPAND | wxALL, 12);
#else
    const long item = m_plannerList->InsertItem(index, row.state.body);
    if (moon.geometricAltitude < 0.0 && row.state.geometricAltitude < 0.0)
      m_plannerList->SetItem(item, 1, _("Both"));
    else if (moon.geometricAltitude < 0.0)
      m_plannerList->SetItem(item, 1, _("Moon"));
    else if (row.state.geometricAltitude < 0.0)
      m_plannerList->SetItem(item, 1, _("Body"));
    m_plannerList->SetItem(item, 2,
                           FormatNavigationAngle(row.lunarDistance));
    m_plannerList->SetItem(item, 3,
                           wxString::Format(CN_UTF8_("%+.1f′/h"),
                                            row.lunarRateArcminHour));
    m_plannerList->SetItem(item, 4,
                           std::isfinite(row.lunarTimingSeconds)
                               ? wxString::Format("%.1f s",
                                                  row.lunarTimingSeconds)
                               : CN_UTF8_("—"));
    m_plannerList->SetItem(item, 5,
                           FormatPlannerAltitude(moon.geometricAltitude));
    m_plannerList->SetItem(item, 6,
                           FormatPlannerAltitude(row.state.geometricAltitude));
    m_plannerList->SetItem(item, 7,
                           wxString::Format(CN_UTF8_("%.1f°"),
                                            moon.azimuthTrue));
    m_plannerList->SetItem(item, 8,
                           wxString::Format(CN_UTF8_("%.1f°"),
                                            row.state.azimuthTrue));
    m_plannerList->SetItem(
        item, 9,
        wxString::Format("%.1f%%",
                         plan.moon.illuminatedFraction * 100.0));
    m_plannerList->SetItem(item, 10,
                           wxString::Format("%.1f", row.state.visualMagnitude));
    m_plannerList->SetItem(item, 11,
                           wxString::Format(CN_UTF8_("%+.1f°"),
                                            row.eclipticLatitude));
    m_plannerList->SetItem(item, 12, row.lunarReason);
#endif
  }
#ifdef __OCPN__ANDROID__
  CN_StyleAndroidControls(m_androidPairCards);
  celestial_android::LayoutScrolls(this);
#endif
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
#ifdef __OCPN__ANDROID__
  RefreshAndroidCalibration();
#endif
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
      CN_UTF8_("%s %s; centre %s; altitudes %s / %s (Δ %s)%s"),
      m_calContact->GetStringSelection(),
      FormatNavigationAngle(m_lastPredictionDeg).c_str(),
      FormatNavigationAngle(result.apparent_center_distance_deg).c_str(),
      FormatNavigationAngle(result.first_altitude_deg).c_str(),
      FormatNavigationAngle(result.second_altitude_deg).c_str(),
      FormatNavigationAngle(result.altitude_difference_deg).c_str(),
      result.altitude_difference_deg > 15.0
          ? CN_UTF8_(" — prefer a more equal-altitude star pair")
          : wxString()));
  m_calObservedAngle->SetAngle(m_lastPredictionDeg);
#ifdef __OCPN__ANDROID__
  m_androidCalibrationPredictionKey = AndroidCalibrationPredictionKey();
  // POBsoft (1985-2026): programmatic Qt edits do not reliably deliver the
  // wx text event. Refresh the advisory for this new observed angle as well
  // as the repeat cards, before the user saves or interprets the profile.
  UpdateProfileCorrection();
#endif
}

void LunarToolsDialog::AddCalibrationReading(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  CheckAndroidCalibrationPrediction();
#endif
  if (!std::isfinite(m_lastPredictionDeg)) {
    CelestialMessageBox(_("Calculate a valid pair prediction first."),
                 _("Sextant check"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  sextant_calibration::CheckReading reading;
  reading.predicted_deg = m_lastPredictionDeg;
  if (!m_calObservedAngle->GetAngle(&reading.observed_deg)) {
    CelestialMessageBox(_("Enter a valid observed angle."), _("Sextant check"),
                 wxOK | wxICON_ERROR, this);
    return;
  }
  m_calObservedAngle->Normalize();
  reading.uncertainty_arcmin = m_calUncertainty->GetValue();
  reading.note = ProfileText(m_calNote->GetValue());
  reading.index_error_arcmin = m_calIndexError->GetValue();
  m_calibrationReadings.push_back(reading);
  const long row =
      m_calReadings->InsertItem(m_calReadings->GetItemCount(),
                                FormatNavigationAngle(reading.predicted_deg));
  m_calReadings->SetItem(row, 1, FormatNavigationAngle(reading.observed_deg));
  m_calReadings->SetItem(
      row, 2, wxString::Format(CN_UTF8_("%+.2f′"), reading.index_error_arcmin));
  m_calReadings->SetItem(
      row, 3,
      FormatNavigationAngle(
          sextant_calibration::IndexCorrectedObservedDegrees(reading)));
  m_calReadings->SetItem(
      row, 4,
      wxString::Format(CN_UTF8_("%+0.2f′"),
                       sextant_calibration::ResidualCorrectionArcmin(reading)));
  m_calReadings->SetItem(
      row, 5, wxString::Format(CN_UTF8_("±%.2f′"), reading.uncertainty_arcmin));
  m_calReadings->SetItem(row, 6, wxString::FromUTF8(reading.note.c_str()));
#ifdef __OCPN__ANDROID__
  m_androidSelectedReading = row;
  RefreshAndroidCalibration();
#endif
}

void LunarToolsDialog::RemoveCalibrationReading(wxCommandEvent&) {
  const long selected =
#ifdef __OCPN__ANDROID__
      m_androidSelectedReading;
#else
      m_calReadings->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
#endif
  if (selected < 0) return;
  m_calibrationReadings.erase(m_calibrationReadings.begin() + selected);
  m_calReadings->DeleteItem(selected);
#ifdef __OCPN__ANDROID__
  m_androidSelectedReading = m_calibrationReadings.empty() ? -1
      : std::min<long>(selected, m_calibrationReadings.size() - 1);
  RefreshAndroidCalibration();
#endif
}

void LunarToolsDialog::SaveCalibrationProfile(wxCommandEvent&) {
  if (m_calibrationReadings.size() < 2) {
    CelestialMessageBox(_("Add at least two repeated or multi-angle checks."),
                 _("Sextant profile"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  const std::string name = ProfileText(m_profileName->GetValue());
  auto profile = sextant_calibration::BuildProfile(
      name, ProfileText(m_profileSerial->GetValue()),
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
    points += wxString::Format(CN_UTF8_("%s: %+0.2f′ ±%.2f′ (%d)"),
                               FormatNavigationAngle(point.angle_deg).c_str(),
                               point.correction_arcmin,
                               point.uncertainty_arcmin, point.reading_count);
  }
  m_profileSummary->SetLabel(wxString::Format(
      CN_UTF8_("Saved profile “%s” (%s). First subtract the independently "
               "measured IE from the raw reading, then add this residual "
               "scale/centering correction. Repeatability %.2f′. Points: %s. "
               "Never extrapolate this table as evidence that mechanical "
               "adjustment is unnecessary."),
      wxString::FromUTF8(profile.name.c_str()),
      wxString::FromUTF8(profile.serial_number.c_str()),
      profile.repeatability_arcmin, points));
#ifndef __OCPN__ANDROID__
  m_profileSummary->Wrap(1000);
#endif
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
      CN_UTF8_("Profile “%s”: %zu correction points; repeatability %.2f′; "
               "created %s. %s Corrections are advisory and never rewrite "
               "observations."),
      wxString::FromUTF8(profile.name.c_str()), profile.points.size(),
      profile.repeatability_arcmin,
      wxString::FromUTF8(profile.created_utc.c_str()),
      profile.excludes_index_error
          ? CN_UTF8_("Apply after independently measured IE.")
          : _("Legacy total correction: apply directly to the raw reading; "
              "do not apply IE separately.")));
#ifndef __OCPN__ANDROID__
  m_profileSummary->Wrap(1000);
#endif
  UpdateProfileCorrection();
}

void LunarToolsDialog::UpdateProfileCorrection() {
#ifdef __OCPN__ANDROID__
  RefreshAndroidCalibration();
#endif
  const int selected = m_profileChoice->GetSelection();
  if (selected < 0 || static_cast<std::size_t>(selected) >= m_profiles.size()) {
    m_profileCorrection->SetLabel(
        CN_UTF8_("Active-profile correction at the entered observed angle: —"));
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
  const double lookupAngle =
      profile.excludes_index_error
          ? angle - m_calIndexError->GetValue() / 60.0
          : angle;
  const double correction = sextant_calibration::CorrectionAt(
      profile, lookupAngle, &uncertainty);
  // POBsoft (1985-2026): persisted points use ten significant digits.
  // Do not label the identical predicted endpoint as extrapolation solely
  // because its serialization rounded by a fraction of 1e-7 degrees.
#ifdef __OCPN__ANDROID__
  constexpr double endpointRoundingDegrees = 1e-7;
#else
  constexpr double endpointRoundingDegrees = 0.0;
#endif
  const bool outside =
      lookupAngle < profile.points.front().angle_deg - endpointRoundingDegrees ||
      lookupAngle > profile.points.back().angle_deg + endpointRoundingDegrees;
  m_profileCorrection->SetLabel(wxString::Format(
      profile.excludes_index_error
          ? CN_UTF8_("Active residual at %s after IE: %+0.2f′ ±%.2f′%s")
          : CN_UTF8_("Active legacy total correction at raw %s: %+0.2f′ "
                     "±%.2f′%s"),
      FormatNavigationAngle(lookupAngle).c_str(), correction, uncertainty,
      outside ? CN_UTF8_(" — outside tested range; nearest endpoint only")
              : wxString()));
}

#ifdef __OCPN__ANDROID__
wxString LunarToolsDialog::AndroidCalibrationPredictionKey() const {
  double latitude = 0.0, longitude = 0.0;
  const auto utc = ReadUtcEntry(m_calUtc, m_activeEntryFormat, false, wxString());
  if (!utc.IsValid() || !m_calLatitude->GetAngle(&latitude) ||
      !m_calLongitude->GetAngle(&longitude)) return wxString();
  return wxString::Format("%d|%d|%d|%.17g|%.17g|%.17g|%.17g|%s",
      m_calFirstBody->GetSelection(), m_calSecondBody->GetSelection(),
      m_calContact->GetSelection(), latitude, longitude,
      m_calPressure->GetValue(), m_calTemperature->GetValue(),
      UtcDateTime::FormatIsoUtc(utc).c_str());
}

void LunarToolsDialog::CheckAndroidCalibrationPrediction() {
  if (m_androidCalibrationPredictionKey.empty() ||
      m_androidCalibrationPredictionKey == AndroidCalibrationPredictionKey())
    return;
  m_androidCalibrationPredictionKey.clear();
  m_lastPredictionDeg = NAN;
  m_calPrediction->SetLabel(_("Inputs changed. Predict distance again."));
}

void LunarToolsDialog::InvalidateAndroidSequence() {
  if (!m_sequenceResult.valid) return;
  m_sequenceResult.valid = false;
  m_sequenceResult.candidates.clear();
  m_sequenceRecords.clear();
  m_sequenceCandidate->Clear();
  m_applySequence->Disable();
  m_sequenceSummary->SetLabel(_("Inputs changed. Solve the session again."));
  RefreshAndroidSequence();
}

void LunarToolsDialog::RefreshAndroidSequence() {
  if (!m_androidSequenceCards || m_androidSequencePending) return;
  m_androidSequencePending = true;
  wxWeakRef<LunarToolsDialog> weak(this);
  // POBsoft (1985-2026): finish the originating touch callback before rebuilding.
  QTimer::singleShot(0, GetHandle(), [weak]() {
    if (!weak) return;
    auto* self = weak.get();
    self->m_androidSequencePending = false;
    auto* selection = self->m_androidSequenceCards->GetSizer();
    selection->Clear(true);
    if (!self->m_sequenceSights->GetCount())
      selection->Add(new wxStaticText(self->m_androidSequenceCards, wxID_ANY,
          _("No saved lunar observations.")), 0, wxEXPAND | wxALL, 8);
    for (unsigned index = 0; index < self->m_sequenceSights->GetCount(); ++index) {
      auto* card = new wxPanel(self->m_androidSequenceCards, wxID_ANY);
      auto* fields = new wxBoxSizer(wxVERTICAL);
      auto* toggle = new wxButton(card, wxID_ANY, wxString::Format(
          self->m_sequenceSights->IsChecked(index) ? _("Deselect observation %u") : _("Select observation %u"), index + 1));
      toggle->Bind(wxEVT_BUTTON, [weak, index](wxCommandEvent&) {
        if (!weak || index >= weak->m_sequenceSights->GetCount()) return;
        weak->m_sequenceSights->Check(index, !weak->m_sequenceSights->IsChecked(index));
        weak->UpdateSequenceSelection();
      });
      fields->Add(toggle, 0, wxEXPAND | wxALL, 8);
      fields->Add(new wxStaticText(card, wxID_ANY, self->m_sequenceSights->GetString(index)),
          0, wxEXPAND | wxALL, 8);
      card->SetSizer(fields);
      selection->Add(card, 0, wxEXPAND | wxALL, 8);
    }
    auto* residuals = self->m_androidSequenceResiduals->GetSizer();
    residuals->Clear(true);
    const int selected = self->m_sequenceCandidate->GetSelection();
    if (self->m_sequenceResult.valid && selected >= 0 &&
        static_cast<size_t>(selected) < self->m_sequenceResult.candidates.size()) {
      auto number = [](double value) {
        return std::isfinite(value) ? wxString::Format(CN_UTF8_("%+.2f′"), value) : _("Shared/not used");
      };
      for (const auto& value : self->m_sequenceResult.candidates[selected].residuals) {
        wxString caption = wxString::FromUTF8(value.label.c_str());
        caption += "\n" + _("Distance residual: ") + number(value.distance_arcmin);
        caption += "\n" + _("Moon-alt residual: ") + number(value.moon_altitude_arcmin);
        caption += "\n" + _("Body-alt residual: ") + number(value.body_altitude_arcmin);
        caption += "\n" + _("Assessment: ") + (value.possible_outlier
            ? wxString::Format(CN_UTF8_("Inspect: %.1fσ"), value.standardized_max) : _("Consistent"));
        residuals->Add(new wxStaticText(self->m_androidSequenceResiduals, wxID_ANY, caption),
            0, wxEXPAND | wxALL, 12);
      }
    }
    CN_StyleAndroidControls(self->m_androidSequenceCards);
    CN_StyleAndroidControls(self->m_androidSequenceResiduals);
    celestial_android::LayoutScrolls(self);
  });
}

void LunarToolsDialog::RefreshAndroidCalibration() {
  if (!m_androidCalReadings || m_androidCalibrationPending) return;
  m_androidCalibrationPending = true;
  wxWeakRef<LunarToolsDialog> weak(this);
  // A selected card must finish its native click before it is destroyed.
  QTimer::singleShot(0, GetHandle(), [weak]() {
    if (!weak) return;
    auto* self = weak.get();
    self->m_androidCalibrationPending = false;
    auto* list = self->m_androidCalReadings->GetSizer();
    list->Clear(true);
    if (self->m_calibrationReadings.empty())
      list->Add(new wxStaticText(self->m_androidCalReadings, wxID_ANY,
                                _("No repeat readings added.")),
                0, wxEXPAND | wxALL, 8);
    for (size_t index = 0; index < self->m_calibrationReadings.size(); ++index) {
      const auto& reading = self->m_calibrationReadings[index];
      auto* panel = new wxPanel(self->m_androidCalReadings, wxID_ANY,
          wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
      auto* fields = new wxBoxSizer(wxVERTICAL);
      auto* select = new wxButton(panel, wxID_ANY,
          wxString::Format(self->m_androidSelectedReading == static_cast<long>(index)
              ? _("Selected repeat %lu") : _("Select repeat %lu"),
              static_cast<unsigned long>(index + 1)));
      select->Bind(wxEVT_BUTTON, [weak, index](wxCommandEvent&) {
        if (!weak || index >= weak->m_calibrationReadings.size()) return;
        weak->m_androidSelectedReading = index;
        weak->RefreshAndroidCalibration();
      });
      fields->Add(select, 0, wxEXPAND | wxALL, 8);
      wxString caption = _("Predicted apparent: ") + FormatNavigationAngle(reading.predicted_deg);
      caption += "\n" + _("Raw observed: ") + FormatNavigationAngle(reading.observed_deg);
      caption += wxString::Format(CN_UTF8_("\nIE (on arc +): %+.2f′"), reading.index_error_arcmin);
      caption += "\n" + _("After IE: ") + FormatNavigationAngle(
          sextant_calibration::IndexCorrectedObservedDegrees(reading));
      caption += wxString::Format(CN_UTF8_("\nResidual to add: %+.2f′\nUncertainty: ±%.2f′"),
          sextant_calibration::ResidualCorrectionArcmin(reading), reading.uncertainty_arcmin);
      caption += "\n" + _("Note / shade: ") + wxString::FromUTF8(reading.note.c_str());
      fields->Add(new wxStaticText(panel, wxID_ANY, caption), 0, wxEXPAND | wxALL, 8);
      panel->SetSizer(fields);
      list->Add(panel, 0, wxEXPAND | wxALL, 6);
    }
    CN_StyleAndroidControls(self->m_androidCalReadings);
    celestial_android::LayoutScrolls(self);
  });
}
#endif

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
    profile.name = ProfileText(value);
    config->Read(prefix + _("Serial"), &value);
    profile.serial_number = ProfileText(value);
    config->Read(prefix + _("Created"), &value);
    profile.created_utc = value.ToStdString();
    config->Read(prefix + _("Repeatability"), &profile.repeatability_arcmin,
                 0.0);
    config->Read(prefix + _("ExcludesIndexError"),
                 &profile.excludes_index_error, false);
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
        CN_UTF8_("Loaded %zu saved profile(s); active “%s”, %zu correction "
                 "points, repeatability %.2f′."),
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
    config->Write(prefix + _("ExcludesIndexError"),
                  profile.excludes_index_error);
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

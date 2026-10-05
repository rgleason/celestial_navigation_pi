/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2016 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 *
 */

#include <wx/wx.h>
#include "CelestialNavigationDialog.h"
#include "DialogGeometry.h"
#include "FixDialog.h"

#include "OcpnApiCompat.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "celestial_navigation_pi.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>
#include <wx/choice.h>
#include <wx/panel.h>
#include "FixDrSource.h"
#include <wx/datectrl.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/timectrl.h>

#ifdef __OCPN__ANDROID__
#include "AndroidFixMath.h"
#include "AndroidSurface.h"
#include <wx/scrolwin.h>
#include <wx/qt/private/wxQtGesture.h>
#endif

using namespace std;

namespace {
// Do not inherit the enlarged geometry saved by the regressed desktop form.
constexpr const wxChar* kFixGeometryKey = _T("Fix297");
}  // namespace

FixDialog::FixDialog(CelestialNavigationDialog* parent)
    : FixDialogBase(parent),
      m_clock_offset(parent->GetClockCorrection()),
      m_fixlat(NAN),
      m_fixlon(NAN),
      m_fixerror(NAN),
      m_Parent(parent),
      m_motionMode(NULL),
      m_epochTimeBasis(NULL),
      m_epochDate(NULL),
      m_epochTime(NULL),
      m_courseTrue(NULL),
      m_speedKnots(NULL),
      m_runningSummary(NULL),
      m_residuals(NULL),
      m_lastEpochTimeBasis(0) {
  // Reuse the generated result and algorithm controls in the compact form.
  std::function<void(wxWindow*)> hideTree = [&](wxWindow* window) {
    for (auto* child : window->GetChildren()) hideTree(child);
    window->Hide();
  };
  for (auto* child : GetChildren()) hideTree(child);
  for (auto* control :
       std::vector<wxWindow*>{m_stLatitude, m_stLongitude, m_stFixError,
                              m_cbFixAlgorithm, m_bGo, m_sdbSizer8OK}) {
    control->Reparent(this);
    control->Show();
#ifdef __OCPN__ANDROID__
    control->GetHandle()->show();
#endif
  }
  auto* root = new wxBoxSizer(wxVERTICAL);
  SetSizer(root);
  auto* drBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Starting DR"));
  auto* source = new wxBoxSizer(wxHORIZONTAL);
  source->Add(new wxStaticText(this, wxID_ANY, _("DR source")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_drSource = new wxChoice(this, wxID_ANY);
  m_drSource->SetName("FixDrSource");
  source->Add(m_drSource, 1, wxEXPAND);
  drBox->Add(source, 0, wxALL | wxEXPAND, 6);
  m_drExplanation = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_drExplanation->SetName("FixDrExplanation");
  drBox->Add(m_drExplanation, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 6);
#ifdef __OCPN__ANDROID__
  auto* drRow = new wxBoxSizer(wxVERTICAL);
#else
  auto* drRow = new wxBoxSizer(wxHORIZONTAL);
#endif
  m_initialLatitude =
      new NavigationAngleCtrl(this, NavigationAngleKind::Latitude, 0, -90, 90);
  m_initialLongitude = new NavigationAngleCtrl(
      this, NavigationAngleKind::Longitude, 0, -180, 180);
  m_initialLatitude->UsePreferredDisplayFormat();
  m_initialLongitude->UsePreferredDisplayFormat();
  m_initialLatitude->SetName("FixDrLatitude");
  m_initialLongitude->SetName("FixDrLongitude");
#ifndef __OCPN__ANDROID__
  for (auto* entry :
       {static_cast<wxTextCtrl*>(m_initialLatitude), m_stLatitude})
    entry->SetMinSize(
        wxSize(entry->GetTextExtent(FormatNavigationAngle(
                                        90, NavigationAngleKind::Latitude))
                       .x +
                   32,
               -1));
  for (auto* entry :
       {static_cast<wxTextCtrl*>(m_initialLongitude), m_stLongitude})
    entry->SetMinSize(
        wxSize(entry->GetTextExtent(FormatNavigationAngle(
                                        -180, NavigationAngleKind::Longitude))
                       .x +
                   32,
               -1));
  m_stFixError->SetMinSize(
      wxSize(m_stFixError->GetTextExtent("999.99' RMS").x + 28, -1));
#endif

  auto addAngle = [this](wxBoxSizer* row, const wxString& label,
                         wxWindow* entry) {
#ifdef __OCPN__ANDROID__
    row->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALL, 5);
    row->Add(entry, 0, wxALL | wxEXPAND, 5);
#else
    row->Add(new wxStaticText(this, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL | wxALL, 5);
    row->Add(entry, 1, wxALL | wxEXPAND, 5);
#endif
  };
  addAngle(drRow, _("DR latitude"), m_initialLatitude);
  addAngle(drRow, _("DR longitude"), m_initialLongitude);
  auto* calculate = new wxButton(this, wxID_ANY, _("Calculate"));
  calculate->SetName("FixCalculate");
  calculate->Bind(wxEVT_BUTTON,
                  [this](wxCommandEvent&) { Update(m_clock_offset); });
  drRow->Add(calculate, 0, wxALL | wxEXPAND, 5);
  drBox->Add(drRow, 0, wxEXPAND);
  root->Add(drBox, 0, wxALL | wxEXPAND, 5);

  auto* results =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Fix from included sights"));
#ifdef __OCPN__ANDROID__
  auto* resultRow = new wxBoxSizer(wxVERTICAL);
#else
  auto* resultRow = new wxBoxSizer(wxHORIZONTAL);
#endif
  addAngle(resultRow, _("FIX latitude"), m_stLatitude);
  addAngle(resultRow, _("FIX longitude"), m_stLongitude);
  addAngle(resultRow, _("Error"), m_stFixError);
  m_stLatitude->SetName("FixLatitude");
  m_stLongitude->SetName("FixLongitude");
  m_stFixError->SetName("FixError");
  results->Add(resultRow, 0, wxEXPAND);
  auto* algorithm = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  algorithm->Add(new wxStaticText(this, wxID_ANY, _("Algorithm")), 0,
                 wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_cbFixAlgorithm->SetName("FixAlgorithm");
  algorithm->Add(m_cbFixAlgorithm, 0, wxALL | wxEXPAND, 5);
  m_bGo->SetLabel(_("Show fix on chart"));
  m_bGo->SetName("FixShowOnChart");
  m_bGo->InvalidateBestSize();
  m_bGo->SetMinSize(m_bGo->GetBestSize());
#ifndef __OCPN__ANDROID__
  algorithm->AddStretchSpacer();
#endif
  algorithm->Add(m_bGo, 0, wxALL | wxEXPAND, 5);
  results->Add(algorithm, 0, wxEXPAND);
  root->Add(results, 0, wxALL | wxEXPAND, 5);

  auto* motionRow = new wxBoxSizer(wxHORIZONTAL);
  motionRow->Add(new wxStaticText(this, wxID_ANY, _("Vessel motion")), 0,
                 wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_motionMode = new wxChoice(this, wxID_ANY);
  m_motionMode->SetName("FixMotion");
  m_motionMode->Append(_("Stationary"));
  m_motionMode->Append(_("One COG and SOG"));
  m_motionMode->Append(_("Each sight's DR Shift"));
  m_motionMode->SetSelection(0);
  motionRow->Add(m_motionMode, 1, wxALL | wxEXPAND, 5);
  root->Add(motionRow, 0, wxEXPAND);
  m_motionInputs = new wxPanel(this);
  auto* motion = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  motion->Add(new wxStaticText(m_motionInputs, wxID_ANY, _("COG true")), 0,
              wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_courseTrue =
      new wxSpinCtrlDouble(m_motionInputs, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(95, -1), wxSP_ARROW_KEYS, 0, 359.9, 0, 0.1);
  m_courseTrue->SetName("FixCourse");
  m_courseTrue->SetDigits(1);
  motion->Add(m_courseTrue, 0, wxALL, 5);
  motion->Add(new wxStaticText(m_motionInputs, wxID_ANY, _("SOG kn")), 0,
              wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_speedKnots =
      new wxSpinCtrlDouble(m_motionInputs, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(95, -1), wxSP_ARROW_KEYS, 0, 100, 0, 0.1);
  m_speedKnots->SetName("FixSpeed");
  m_speedKnots->SetDigits(1);
  motion->Add(m_speedKnots, 0, wxALL, 5);
  m_motionInputs->SetSizer(motion);
  root->Add(m_motionInputs, 0, wxEXPAND);
  m_epochSummary = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_epochSummary->SetName("FixEpochSummary");
  root->Add(m_epochSummary, 0, wxALL | wxEXPAND, 6);
  m_runningSummary = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_runningSummary->SetName("FixStatus");
  root->Add(m_runningSummary, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 6);

  m_residuals = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
                               wxSize(-1, 125), wxLC_REPORT | wxLC_HRULES);
  m_residuals->SetName("FixSightDetails");
  const std::vector<wxString> headings = {
      _("UTC"),           _("Body"),        _("Hc"),
      _("Ho-Hc"),         _("DR shift NM"), _("Saved bearing"),
      _("Used bearing T")};
  const std::vector<wxString> samples = {
      "12-31 23:59:59",
      "Menkent",
      wxString::FromUTF8("000\xc2\xb0 00.0000'"),
      "+999.99'",
      "999.99",
      wxString::FromUTF8("359.9\xc2\xb0 M"),
      wxString::FromUTF8("359.9\xc2\xb0 T")};
  int tableWidth = 0;
  for (size_t index = 0; index < headings.size(); ++index) {
    const int width = std::max(m_residuals->GetTextExtent(headings[index]).x,
                               m_residuals->GetTextExtent(samples[index]).x) +
                      20;
    tableWidth += width;
    m_residuals->InsertColumn(index, headings[index], wxLIST_FORMAT_LEFT,
                              width);
  }
#ifndef __OCPN__ANDROID__
  m_residuals->SetMinSize(wxSize(tableWidth + 6, 125));
#endif
#ifndef __OCPN__ANDROID__
  root->Add(m_residuals, 1, wxALL | wxEXPAND, 5);
#else
  m_residuals->Hide();
  m_androidResiduals = new wxStaticText(this, wxID_ANY, wxEmptyString);
  root->Add(m_androidResiduals, 0, wxALL | wxEXPAND, 5);
#endif
  auto* more = new wxButton(this, wxID_ANY, _("More options..."));
  more->SetName("FixMoreOptions");
  root->Add(more, 0, wxALL, 5);
  m_moreOptions = new wxPanel(this);
  auto* advanced = new wxBoxSizer(wxVERTICAL);
#ifdef __OCPN__ANDROID__
  auto* epoch = new wxBoxSizer(wxVERTICAL);
#else
  auto* epoch = new wxBoxSizer(wxHORIZONTAL);
#endif
  epoch->Add(new wxStaticText(m_moreOptions, wxID_ANY, _("Fix time")), 0, wxALL,
             5);
  m_epochTimeBasis = new wxChoice(m_moreOptions, wxID_ANY);
  m_epochTimeBasis->Append(_("UTC"));
  m_epochTimeBasis->Append(_("Computer local time"));
  m_epochTimeBasis->SetSelection(0);
  epoch->Add(m_epochTimeBasis, 0, wxALL, 5);
  m_epochDate = new wxDatePickerCtrl(m_moreOptions, wxID_ANY);
  m_epochTime = new CelestialTimePicker(m_moreOptions, wxID_ANY);
  m_epochDate->SetName("FixEpochDate");
  m_epochTime->SetName("FixEpochTime");
  epoch->Add(m_epochDate, 0, wxALL, 5);
  epoch->Add(m_epochTime, 0, wxALL, 5);
  auto* latest = new wxButton(m_moreOptions, wxID_ANY, _("Latest sight"));
  latest->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    SetEpochToLatestVisibleSight(m_clock_offset);
    Update(m_clock_offset);
  });
  epoch->Add(latest, 0, wxALL, 5);
  advanced->Add(epoch, 0, wxEXPAND);
  advanced->Add(new wxStaticText(m_moreOptions, wxID_ANY,
                                 _("Clock correction for this fix. Use sights "
                                   "from the same watch and clock.")),
                0, wxALL, 5);
  m_lunarSolution = new wxChoice(m_moreOptions, wxID_ANY);
  m_lunarSolution->Append(_("Use existing manual clock correction"));
  for (const auto& record : parent->LunarSolutions())
    m_lunarSolution->Append(record.Summary());
  m_lunarSolution->SetSelection(0);
  advanced->Add(m_lunarSolution, 0, wxALL | wxEXPAND, 5);
  m_runningDetails = new wxStaticText(m_moreOptions, wxID_ANY, wxEmptyString);
  advanced->Add(m_runningDetails, 0, wxALL | wxEXPAND, 5);
  m_moreOptions->SetSizer(advanced);
  m_moreOptions->Hide();
  root->Add(m_moreOptions, 0, wxALL | wxEXPAND, 5);
  more->Bind(wxEVT_BUTTON, [this, more](wxCommandEvent&) {
    const bool show = !m_moreOptions->IsShown();
    m_moreOptions->Show(show);
    more->SetLabel(show ? _("Fewer options") : _("More options..."));
    Layout();
#ifndef __OCPN__ANDROID__
    GetSizer()->Fit(this);
#else
    celestial_android::LayoutScrolls(this);
#endif
  });
  m_sdbSizer8OK->SetLabel(_("Close"));
  auto* close = new wxBoxSizer(wxHORIZONTAL);
  close->AddStretchSpacer();
  close->Add(m_sdbSizer8OK, 0, wxALL, 5);
  root->Add(close, 0, wxEXPAND);

  const auto boat = parent->GetPlugin()->GetBoatNavigationSnapshot();
  if (boat.valid) {
    m_courseTrue->SetValue(boat.cogTrue);
    m_speedKnots->SetValue(boat.sogKnots);
  }
  SetEpochControls(wxDateTime::UNow());
  const bool savedShift = std::any_of(
      parent->m_Sights.begin(), parent->m_Sights.end(), [](const Sight& s) {
        return s.IsVisible() &&
               (s.m_Type == Sight::ALTITUDE || s.m_Type == Sight::HORIZON) &&
               s.m_ShiftNm != 0;
      });
  if (savedShift) m_motionMode->SetSelection(2);
  SetEpochToLatestVisibleSight(m_clock_offset);
  UpdateMotionControls();
  RefreshDrSources();
  m_drSource->Bind(wxEVT_CHOICE, &FixDialog::ChangeDrSource, this);
  m_initialLatitude->Bind(wxEVT_TEXT, &FixDialog::EditDr, this);
  m_initialLongitude->Bind(wxEVT_TEXT, &FixDialog::EditDr, this);
  m_motionMode->Bind(wxEVT_CHOICE, &FixDialog::ChangeMotionMode, this);
  m_lunarSolution->Bind(wxEVT_CHOICE,
                        [this](wxCommandEvent&) { Update(m_clock_offset); });
  m_epochTimeBasis->Bind(wxEVT_CHOICE, &FixDialog::ChangeEpochTimeBasis, this);
  m_epochDate->Bind(wxEVT_DATE_CHANGED,
                    [this](wxDateEvent&) { Update(m_clock_offset); });
  m_epochTime->Bind(wxEVT_TIME_CHANGED,
                    [this](wxDateEvent&) { Update(m_clock_offset); });
  m_courseTrue->Bind(wxEVT_SPINCTRLDOUBLE, &FixDialog::OnRunningControl, this);
  m_speedKnots->Bind(wxEVT_SPINCTRLDOUBLE, &FixDialog::OnRunningControl, this);
  m_courseTrue->Bind(wxEVT_TEXT, &FixDialog::OnRunningControl, this);
  m_speedKnots->Bind(wxEVT_TEXT, &FixDialog::OnRunningControl, this);
#ifdef __OCPN__ANDROID__
  if (auto* combo = qobject_cast<QComboBox*>(m_cbFixAlgorithm->GetHandle()))
    combo->setEditable(false);
  auto* motionUpdate = new QTimer(GetHandle());
  motionUpdate->setSingleShot(true);
  wxWeakRef<FixDialog> weakFix(this);
  QObject::connect(motionUpdate, &QTimer::timeout, GetHandle(), [weakFix]() {
    if (weakFix) weakFix->Update(weakFix->m_clock_offset);
  });
  for (auto* control : {m_courseTrue, m_speedKnots})
    if (auto* native = qobject_cast<QDoubleSpinBox*>(control->GetHandle()))
      QObject::connect(native,
                       static_cast<void (QDoubleSpinBox::*)(double)>(
                           &QDoubleSpinBox::valueChanged),
                       motionUpdate,
                       [motionUpdate](double) { motionUpdate->start(0); });
#endif
  GetSizer()->Fit(this);
#ifndef __OCPN__ANDROID__
  SetMinSize(GetSize());
  dialog_geometry::Restore(this, kFixGeometryKey, GetSize());
#endif
  Bind(wxEVT_CLOSE_WINDOW, &FixDialog::OnWindowClose, this);
#ifdef __OCPN__ANDROID__
  celestial_android::Decorate(this, GetTitle(),
                              [this]() { m_Parent->OnFixClose(); });
  // The reused controls predate the new Qt group boxes. Raise them above the
  // opaque group-box backgrounds after reparenting to the scrolling surface.
  for (auto* control :
       std::vector<wxWindow*>{m_stLatitude, m_stLongitude, m_stFixError,
                              m_cbFixAlgorithm, m_bGo}) {
    GetSizer()->Show(control, true, true);
    control->GetHandle()->setParent(control->GetParent()->GetHandle());
    control->GetHandle()->show();
    control->GetHandle()->raise();
  }
#endif
}

void FixDialog::RefreshDrSources() {
  std::vector<fix_dr::DrRecord> records;
  std::vector<wxString> labels;
  std::ostringstream signature;
  signature.imbue(std::locale::classic());
  signature << std::setprecision(17);
  std::vector<std::string> keys;
  for (const auto& sight : m_Parent->m_Sights) {
    if (!sight.IsVisible() ||
        (sight.m_Type != Sight::ALTITUDE && sight.m_Type != Sight::HORIZON) ||
        (!IsRunning() && sight.m_ShiftNm != 0) || !sight.m_DateTime.IsValid())
      continue;
    const auto time = UtcDateTime::ToInstant(sight.m_DateTime);
    const fix_dr::Position position{sight.m_DRLat, sight.m_DRLon};
    if (!fix_dr::Valid(position)) continue;
    std::ostringstream key;
    key.imbue(std::locale::classic());
    key << std::setprecision(17) << sight.m_Body.ToStdString() << ':'
        << time.GetValue().GetValue() << ':' << position.latitude << ':'
        << position.longitude;
    records.push_back({position, time.GetValue().GetValue(), true, key.str()});
    keys.push_back(key.str());
    labels.push_back(sight.m_Body + " | " +
                     UtcDateTime::FormatInstant(time, "%Y-%m-%d %H:%M:%S") + " UTC");
  }
  std::sort(keys.begin(), keys.end());
  for (const auto& key : keys) signature << key << ';';
  const auto newKey = wxString::FromUTF8(signature.str().c_str());
  if (m_drSourcesReady && newKey == m_drSourceKey) return;
  m_drSourcesReady = true;
  m_drSourceKey = newKey;
  m_drRecords = records;
  m_drSource->Clear();
  m_drSource->Append(_("Enter a DR position"));
  m_drSource->Append(_("Use current boat position (explicit choice)"));
  for (const auto& label : labels) m_drSource->Append(label);
  const int latest = fix_dr::LatestDr(records);
  m_changingDr = true;
  if (latest >= 0) {
    m_drSource->SetSelection(latest + 2);
    m_initialLatitude->SetAngle(records[latest].position.latitude);
    m_initialLongitude->SetAngle(records[latest].position.longitude);
    m_drExplanation->SetLabel(_("Saved DR from ") + labels[latest]);
    m_hasDr = true;
  } else {
    m_drSource->SetSelection(0);
    m_initialLatitude->ChangeValue(wxEmptyString);
    m_initialLongitude->ChangeValue(wxEmptyString);
    m_drExplanation->SetLabel(
        _("No usable included sight DR. Enter a position or explicitly choose "
          "the boat position."));
    m_hasDr = false;
  }
  m_changingDr = false;
}

void FixDialog::ChangeDrSource(wxCommandEvent&) {
  const int source = m_drSource->GetSelection();
  m_changingDr = true;
  m_hasDr = false;
  if (source >= 2 && std::size_t(source - 2) < m_drRecords.size()) {
    const auto position = m_drRecords[source - 2].position;
    m_initialLatitude->SetAngle(position.latitude);
    m_initialLongitude->SetAngle(position.longitude);
    m_drExplanation->SetLabel(_("Saved DR from ") +
                              m_drSource->GetStringSelection());
    m_hasDr = true;
  } else if (source == 1) {
    double lat, lon;
    if (m_Parent->GetPlugin()->GetBoatPosition(&lat, &lon)) {
      m_initialLatitude->SetAngle(lat);
      m_initialLongitude->SetAngle(lon);
      m_hasDr = true;
      m_drExplanation->SetLabel(
          _("Current boat position selected explicitly. This may differ from "
            "the saved sight DR."));
    } else {
      m_initialLatitude->ChangeValue(wxEmptyString);
      m_initialLongitude->ChangeValue(wxEmptyString);
      m_drExplanation->SetLabel(
          _("No valid boat position is available. Enter a DR position."));
    }
  } else {
    m_drExplanation->SetLabel(_("User-entered DR for this fix"));
    double lat, lon;
    m_hasDr =
        m_initialLatitude->GetAngle(&lat) && m_initialLongitude->GetAngle(&lon);
  }
  m_changingDr = false;
  Update(m_clock_offset);
}
void FixDialog::EditDr(wxCommandEvent&) {
  if (m_changingDr) return;
  m_drSource->SetSelection(0);
  m_drExplanation->SetLabel(_("User-entered DR for this fix"));
  m_hasDr = true;
  Update(m_clock_offset);
}
bool FixDialog::ReadDr(double* latitude, double* longitude) {
  if (m_hasDr && m_initialLatitude->GetAngle(latitude) &&
      m_initialLongitude->GetAngle(longitude))
    return true;
  m_fixlat = m_fixlon = m_fixerror = NAN;
  m_stLatitude->SetValue(_("N/A"));
  m_stLongitude->SetValue(_("N/A"));
  m_stFixError->SetValue(_("N/A"));
  m_runningSummary->SetLabel(_("Enter a valid DR or explicitly choose the boat position."));
  m_bGo->Disable();
  return false;
}
void FixDialog::FocusStartingDr() {
  m_drSource->SetFocus();
#ifdef __OCPN__ANDROID__
  for (auto* window = m_drSource->GetParent(); window;
       window = window->GetParent())
    if (auto* scroll = wxDynamicCast(window, wxScrolledWindow)) scroll->Scroll(0, 0);
#endif
}

FixDialog::~FixDialog() { dialog_geometry::Save(this, kFixGeometryKey); }

void FixDialog::RunIntegrationScenario() {
  m_motionMode->SetSelection(1);
  UpdateMotionControls();
  Update(m_clock_offset);
  Raise();
}

wxDateTime FixDialog::ReadEpochUtc() const {
  const wxDateTime date = m_epochDate->GetValue();
  const wxDateTime time = m_epochTime->GetValue();
  if (!date.IsValid() || !time.IsValid()) return wxDateTime();
#ifdef __OCPN__ANDROID__
  const auto f = UtcDateTime::Fields(time);
  const wxDateTime entered = UtcDateTime::FromCalendar(date, f.hour, f.min,
                                         f.sec + f.msec / 1000.0);
  return m_epochTimeBasis->GetSelection() == 1
             ? UtcDateTime::LocalWallToInstant(entered) : entered;
#else
  wxDateTime entered(date.GetDay(), date.GetMonth(), date.GetYear(),
                     time.GetHour(), time.GetMinute(), time.GetSecond());
  return m_epochTimeBasis->GetSelection() == 1
             ? entered
             : UtcDateTime::ToInstant(entered);
#endif
}

void FixDialog::SetEpochControls(const wxDateTime& utc) {
  if (!utc.IsValid()) return;
  wxDateTime value = m_epochTimeBasis->GetSelection() == 1
                         ? utc
#ifdef __OCPN__ANDROID__
                         : UtcDateTime::FromInstant(utc);
#else
                         : UtcDateTime::CopyFields(utc.ToUTC());
#endif
#ifdef __OCPN__ANDROID__
  value = m_epochTimeBasis->GetSelection() == 1
              ? UtcDateTime::InstantToLocalWall(utc) : utc;
  m_epochDate->SetValue(UtcDateTime::CalendarDate(value));
  m_epochTime->SetValue(value);
#else
  m_epochDate->SetValue(value);
  m_epochTime->SetValue(value);
#endif
}

void FixDialog::ChangeEpochTimeBasis(wxCommandEvent&) {
  const int next = m_epochTimeBasis->GetSelection();
  m_epochTimeBasis->SetSelection(m_lastEpochTimeBasis);
  const wxDateTime utc = ReadEpochUtc();
  m_epochTimeBasis->SetSelection(next);
  m_lastEpochTimeBasis = next;
  SetEpochControls(utc);
  Update(m_clock_offset);
}

bool FixDialog::IsRunning() const { return m_motionMode->GetSelection() > 0; }

void FixDialog::UpdateMotionControls() {
  const bool course = m_motionMode->GetSelection() == 1;
  m_motionInputs->Show(course);
  m_courseTrue->Enable(course); m_speedKnots->Enable(course);
  m_cbFixAlgorithm->Enable(!IsRunning());
  m_epochDate->Enable(IsRunning()); m_epochTime->Enable(IsRunning());
  m_epochTimeBasis->Enable(IsRunning());
  m_epochSummary->Show(IsRunning());
  Layout();
#ifndef __OCPN__ANDROID__
  if (GetSizer()) GetSizer()->Fit(this);
#endif
}

void FixDialog::ChangeMotionMode(wxCommandEvent&) {
  UpdateMotionControls();
  Update(m_clock_offset);
}

void FixDialog::SetEpochToLatestVisibleSight(double clock_offset) {
  double effective_correction = clock_offset;
  const int selection = m_lunarSolution->GetSelection();
  if (selection > 0 &&
      std::size_t(selection - 1) < m_Parent->LunarSolutions().size())
    effective_correction =
        m_Parent->LunarSolutions()[selection - 1].TotalCorrection();
  wxDateTime latest;
  for (const Sight& sight : m_Parent->m_Sights) {
    if (!sight.IsVisible() ||
        (sight.m_Type != Sight::ALTITUDE && sight.m_Type != Sight::HORIZON))
      continue;
    const wxDateTime sightUtc = UtcDateTime::ToInstant(
        UtcDateTime::AddSeconds(sight.m_DateTime, effective_correction));
    if (!latest.IsValid() || sightUtc.IsLaterThan(latest)) latest = sightUtc;
  }
  if (latest.IsValid()) SetEpochControls(latest);
}

#ifdef __OCPN__ANDROID__
void FixDialog::OnEvtPanGesture(wxQT_PanGestureEvent& event) {
  int x = event.GetOffset().x;
  int y = event.GetOffset().y;

  int dx = x - m_lastPanX;
  int dy = y - m_lastPanY;

  if (event.GetState() == GestureUpdated) {
    wxPoint p = GetPosition();
    wxSize s = GetSize();
    p.x = wxMax(0, p.x + dx);
    p.y = wxMax(0, p.y + dy);
    p.x = wxMin(p.x, ::wxGetDisplaySize().x - s.x);
    p.y = wxMin(p.y, ::wxGetDisplaySize().y - s.y);
    SetPosition(p);
  }
  m_lastPanX = x;
  m_lastPanY = y;
}
#endif

double dist(wxRealPoint a, wxRealPoint b) {
  double x = a.x - b.x;
  double y = a.y - b.y;
  if (y > 180) y -= 360;
  if (y < -180) y += 360;

  return x * x + y * y;
}

/* find smallest circle that fits set of points */
double MinCircle(double& x, double& y, std::vector<wxRealPoint> points) {
  if (points.size() < 2) return NAN;

  double maxdist = 0;
  /* find farthest points */
  wxRealPoint maxa, maxb;
  for (unsigned int i = 1; i < points.size(); i++) {
    for (unsigned int j = 0; j < i; j++) {
      wxRealPoint a = points[i], b = points[j];
      double d = dist(a, b);
      if (d > maxdist) maxa = a, maxb = b, maxdist = d;
    }
  }

  /* now we should expand to include any points not bounded */

  x = (maxa.x + maxb.x) / 2;
  y = (maxa.y + maxb.y) / 2;
  return maxdist / 4;
}

double MinCirclePoints(double& mincx, double& mincy,
                       std::list<std::list<wxRealPoint> >& all_points,
                       std::vector<wxRealPoint>& points) {
  if (all_points.size() == 0) return MinCircle(mincx, mincy, points);

  std::list<wxRealPoint> cpoints = all_points.front();
  all_points.pop_front();

  int s = points.size();
  points.push_back(wxRealPoint());
  double mind = INFINITY;
  for (std::list<wxRealPoint>::iterator it = cpoints.begin();
       it != cpoints.end(); it++) {
    points[s] = *it;

    double cx, cy, cd;
    cd = MinCirclePoints(cx, cy, all_points, points);
    if (cd < mind) {
      mind = cd;
      mincx = cx;
      mincy = cy;
    }
  }
  points.pop_back();
  all_points.push_front(cpoints);

  if (isinf(mind)) return NAN;
  return mind;
}

/* this algorithm works for any size, but c++ is sadly not supporting
   pointers to variable size arrays like gcc does for c */
int matrix_invert3(double a[3][3]) {
  int n = 3;

  int i, j, k;
  // main cycle for columns of A matrix
  for (k = 0; k < n; k++) {
    // make current element 1
    double aa = a[k][k];
    if (aa == 0.0f) return 0;

    double aainv = 1.0f / aa;

    a[k][k] = 1.0f;
    for (i = 0; i < n; i++) a[k][i] *= aainv;

    // make all rows zero
    for (j = 0; j < n; j++) {
      if (j == k) continue;

      aa = a[j][k];
      a[j][k] = 0.0f;

      for (i = 0; i < n; i++) a[j][i] -= a[k][i] * aa;
    }
  }
  return 1;
}

void FixDialog::Update(int clock_offset) {
#ifdef __OCPN__ANDROID__
  m_androidResiduals->SetLabel(wxEmptyString);
  // Result labels change after calculation, including validation failures.
  // Recompute their Android font heights and the actual scrolling content.
  struct RefreshLayout {
    FixDialog* dialog;
    ~RefreshLayout() { celestial_android::LayoutScrolls(dialog); }
  } refreshLayout{this};
#else
  struct RefreshLayout {
    FixDialog* dialog;
    ~RefreshLayout() {
      dialog->m_runningSummary->Wrap(dialog->GetClientSize().x - 20);
      dialog->Layout();
    }
  } refreshLayout{this};
#endif
  m_clock_offset = clock_offset;
  m_bGo->Disable();
  m_fixlat = m_fixlon = m_fixerror = NAN;
  m_Parent->SetLastFix(NAN, NAN);
  m_stLatitude->ChangeValue(_("N/A"));
  m_stLongitude->ChangeValue(_("N/A"));
  m_stFixError->ChangeValue(_("N/A"));
  m_runningDetails->SetLabel(wxEmptyString);
  RefreshDrSources();
  double effective_correction = clock_offset;
  const LunarSolutionRecord* record = nullptr;
  const int selection = m_lunarSolution->GetSelection();
  if (selection > 0 &&
      std::size_t(selection - 1) < m_Parent->LunarSolutions().size()) {
    record = &m_Parent->LunarSolutions()[selection - 1];
    effective_correction = record->TotalCorrection();
  }
  wxString error;
  if (!PrepareLunarFixSights(m_Parent->m_Sights, record, effective_correction,
                             &m_workingSights, &error)) {
    m_fixlat = m_fixlon = m_fixerror = NAN;
    m_stLatitude->SetValue(_("N/A"));
    m_stLongitude->SetValue(_("N/A"));
    m_stFixError->SetValue(_("Watch mismatch"));
    m_runningSummary->SetLabel(error);
    m_bGo->Disable();
    return;
  }
  if (IsRunning()) {
    m_epochSummary->SetLabel(_("Fix time: ") +
        UtcDateTime::FormatInstant(ReadEpochUtc(), "%Y-%m-%d %H:%M:%S") + " UTC");
    UpdateRunningFix(effective_correction);
    return;
  }
  if (m_residuals) m_residuals->DeleteAllItems();
  unsigned shifted = 0;
  for (const Sight& sight : m_workingSights)
    if (sight.IsVisible() &&
        (sight.m_Type == Sight::ALTITUDE || sight.m_Type == Sight::HORIZON) &&
        sight.m_ShiftNm != 0.0)
      ++shifted;
  if (m_runningSummary)
    m_runningSummary->SetLabel(
        shifted ? wxString::Format(
                      _("%u visible DR-shifted sights are excluded from the "
                        "stationary fix. Select per-sight DR Shift above "
                        "to calculate their common-epoch fix."), shifted)
                : wxString());
  std::list<std::vector<double> > J;
  std::list<double> R;

  double X[3]; /* result */

  double initiallat, initiallon;
  if (!ReadDr(&initiallat, &initiallon)) return;
  X[0] = cos(d_to_r(initiallat)) * cos(d_to_r(initiallon));
  X[1] = cos(d_to_r(initiallat)) * sin(d_to_r(initiallon));
  X[2] = sin(d_to_r(initiallat));

  m_clock_offset = clock_offset;
  int iterations = 0;
again:
  for (Sight& s : m_workingSights) {
    if (!s.IsVisible() ||
        (s.m_Type != Sight::ALTITUDE && s.m_Type != Sight::HORIZON))
      continue;

    if (s.m_ShiftNm) {
      continue;
    }

    double lat, lon;
    s.BodyLocation(UtcDateTime::AddSeconds(s.m_DateTime, effective_correction),
                   &lat, &lon, 0, 0, 0);

    /* take vector from body location of length equal to
       normalized measurement (so the plane this vector
       describes intersects the unit sphere along the positions
       the sight is valid) */
    std::vector<double> v;
    double x = cos(d_to_r(lat)) * cos(d_to_r(lon));
    double y = cos(d_to_r(lat)) * sin(d_to_r(lon));
    double z = sin(d_to_r(lat));

    double sm = sin(d_to_r(s.m_ObservedAltitude));
    double cm = cos(d_to_r(s.m_ObservedAltitude));

    double d = NAN;

    switch (m_cbFixAlgorithm->GetSelection()) {
      case 0: /* plane */
      plane:
        /* plane */
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
        d = sm - (X[0] * x + X[1] * y + X[2] * z);
        break;
      case 1: /* sphere */
      {
        double xc = X[0] - x, yc = X[1] - y, zc = X[2] - z;
        v.push_back(2 * xc);
        v.push_back(2 * yc);
        v.push_back(2 * zc);
        d = cm * cm + (1 - sm) * (1 - sm) - xc * xc - yc * yc - zc * zc;
      } break;
      case 2: /* cone */
      {
        double t2 = X[0] * X[0] + X[1] * X[1] + X[2] * X[2], t = sqrt(t2);
        if (t < .1) goto plane;
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
        d = sm - (X[0] * x + X[1] * y + X[2] * z) / t;
      } break;
      case 3: /* cone 2 */
      {
        double t2 = X[0] * X[0] + X[1] * X[1] + X[2] * X[2], t = sqrt(t2);
        if (t < .1) goto plane;
#ifdef __OCPN__ANDROID__
        const auto gradient = celestial_android::ConeAltitudeGradient(
            {{x, y, z}}, {{X[0], X[1], X[2]}});
        v.assign(gradient.begin(), gradient.end());
#else
        v.push_back(x / t - x * X[0] * X[0] / (t * t2));
        v.push_back(y / t - y * X[1] * X[1] / (t * t2));
        v.push_back(z / t - z * X[2] * X[2] / (t * t2));
#endif
        d = sm - (X[0] * x + X[1] * y + X[2] * z) / t;
      } break;
    }

    J.push_back(v);
    R.push_back(d);
  }

  /* it takes at least 2 visible sights to have a fix */
  if (J.size() < 2) {
    m_fixerror = NAN;
    m_stLatitude->SetValue(_("   N/A   "));
    m_stLongitude->SetValue(_("   N/A   "));
    m_stFixError->SetValue(_("   N/A   "));
    m_bGo->Disable();
    return;
  }

  /* fit to unit sphere (keep results on surface of earth) */
  {
    std::vector<double> v;
    v.push_back(2 * X[0]);
    v.push_back(2 * X[1]);
    v.push_back(2 * X[2]);
    J.push_back(v);
    double d = 1 - X[0] * X[0] - X[1] * X[1] - X[2] * X[2];
    R.push_back(d);
  }

  /* now use least squares to find the point where all these planes
     intersect, it is our fix */

  /* least squares all the great_circles to find fix
     JtJ^-1 * JtRt */

  double S[3][3]; /* S = Jt*J */
  std::list<std::vector<double> >::iterator it;
  std::list<double>::iterator it2;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) {
      S[i][j] = 0;
      for (it = J.begin(); it != J.end(); it++) S[i][j] += (*it)[i] * (*it)[j];
    }

  double N[3]; /* N = (JtRt)t  R is all 1's since this is linear */
  for (int i = 0; i < 3; i++) {
    N[i] = 0;
    for (it = J.begin(), it2 = R.begin(); it != J.end(); it++, it2++)
      N[i] += (*it)[i] * (*it2);
  }

  /* invert S */
  double d, err;
  if (!matrix_invert3(S)) goto fail;

  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) X[i] += S[i][j] * N[j];

  d = sqrt(X[0] * X[0] + X[1] * X[1] + X[2] * X[2]);
  err = fabs(d - 1);

  /* 20 is overkill to converge. there are better ways to determine the number
     of iterations, but this routine runs so fast it doesn't matter */
  if (iterations++ < 20) {
    if (err > 100) /* we are diverging */
      goto fail;
    J.clear();
    R.clear();
    goto again;
  }

  /* normalize */
  X[0] /= d, X[1] /= d, X[2] /= d;

  m_fixlat = r_to_d(asin(X[2]));
  m_fixlon = r_to_d(atan2(X[1], X[0]));

  if (err <= .1) {
    m_fixerror = 0;
    for (it2 = R.begin(); it2 != R.end(); it2++) m_fixerror += (*it2) * (*it2);
    m_fixerror = sqrt(m_fixerror);

    m_stLatitude->SetValue(toSDMM_PlugIn(1, m_fixlat, true));
    m_stLongitude->SetValue(toSDMM_PlugIn(2, m_fixlon, true));
    m_stFixError->SetValue(wxString::Format(_T("%.3g"), m_fixerror));
    m_Parent->SetLastFix(m_fixlat, m_fixlon);
    FillStationaryDetails(effective_correction);
    m_bGo->Enable();
  } else {
  fail:
    m_fixerror = NAN;
    m_stLatitude->SetValue(_("   N/A   "));
    m_stLongitude->SetValue(_("   N/A   "));
    m_stFixError->SetValue(_("   N/A   "));
    m_bGo->Disable();
  }

  RequestRefresh(GetParent()->GetParent());
}

void FixDialog::FillStationaryDetails(double correction) {
  double squared = 0;
  unsigned count = 0;
#ifdef __OCPN__ANDROID__
  wxString details;
#endif
  for (Sight& sight : m_workingSights) {
    if (!sight.IsVisible() || sight.m_ShiftNm != 0 ||
        (sight.m_Type != Sight::ALTITUDE && sight.m_Type != Sight::HORIZON))
      continue;
    double lat, lon;
    sight.BodyLocation(UtcDateTime::AddSeconds(sight.m_DateTime, correction),
                       &lat, &lon, nullptr, nullptr, nullptr);
    const double altitude = r_to_d(std::asin(std::max(
        -1.0,
        std::min(1.0, std::sin(d_to_r(m_fixlat)) * std::sin(d_to_r(lat)) +
                          std::cos(d_to_r(m_fixlat)) * std::cos(d_to_r(lat)) *
                              std::cos(d_to_r(m_fixlon - lon))))));
    const double residual = 60 * (sight.m_ObservedAltitude - altitude);
    squared += residual * residual;
    ++count;
    const wxString utc = UtcDateTime::FormatInstant(
        UtcDateTime::ToInstant(
            UtcDateTime::AddSeconds(sight.m_DateTime, correction)),
        "%m-%d %H:%M:%S");
    const long row = m_residuals->InsertItem(m_residuals->GetItemCount(), utc);
    m_residuals->SetItem(row, 1, sight.m_Body);
    m_residuals->SetItem(row, 2, FormatNavigationAngle(altitude));
    m_residuals->SetItem(row, 3, wxString::Format("%+.2f'", residual));
    m_residuals->SetItem(row, 4, "0.00");
#ifdef __OCPN__ANDROID__
    details += sight.m_Body + " | " + utc + " UTC\n" + _("Hc: ") +
               FormatNavigationAngle(altitude) + _(" | Ho minus Hc: ") +
               wxString::Format("%+.2f'\n\n", residual);
#endif
  }
  if (count)
    m_stFixError->ChangeValue(
        wxString::Format("%.2f' RMS", std::sqrt(squared / count)));
  if (m_runningSummary->GetLabel().empty())
    m_runningSummary->SetLabel(wxString::Format(_("%u included sights"), count));
#ifdef __OCPN__ANDROID__
  m_androidResiduals->SetLabel(details);
#endif
}

void FixDialog::UpdateRunningFix(double clock_offset) {
  const wxDateTime epoch = ReadEpochUtc();
  const bool manual = m_motionMode->GetSelection() == 2;
  m_residuals->DeleteAllItems();
  std::vector<FixObservation> observations;
#ifdef __OCPN__ANDROID__
  std::vector<wxString> androidShiftDetails;
#endif
  for (const Sight& sight : m_workingSights) {
    if (!sight.IsVisible() ||
        (sight.m_Type != Sight::ALTITUDE && sight.m_Type != Sight::HORIZON))
      continue;
    FixObservation observation;
    observation.label = sight.m_Body;
    observation.body = sight.m_Body;
    observation.utc = UtcDateTime::ToInstant(
        UtcDateTime::AddSeconds(sight.m_DateTime, clock_offset));
    observation.observedAltitude = sight.m_ObservedAltitude;
    observation.uncertaintyMinutes =
        sight.m_Type == Sight::HORIZON
            ? std::max(1.0, sight.m_HorizonAltitudeUncertainty)
            : std::max(0.1, sight.m_MeasurementCertainty);
    if (manual) {
      observation.hasManualDisplacement = true;
      observation.displacementNm = sight.m_ShiftNm;
      observation.displacementBearingTrue = sight.m_ShiftBearing;
      if (sight.m_bMagneticShiftBearing && sight.m_ShiftNm != 0.0) {
        // WMM expects geographic longitude in [-180, 180], not a 0-360
        // heading. In particular, 70 W must remain -70 rather than 290.
        const double longitude = std::remainder(sight.m_DRLon, 360.0);
        observation.displacementBearingTrue +=
            celestial_navigation_pi_GetWMM(sight.m_DRLat, longitude,
                                           sight.m_EyeHeight,
                                           sight.m_CorrectedDateTime);
      }
    }
    observations.push_back(observation);
#ifdef __OCPN__ANDROID__
    wxString shift = _("Saved DR shift: ") +
        celestial_android::NumberText(sight.m_ShiftNm) + " NM";
    if (sight.m_ShiftNm != 0.0) {
      shift += " | " + _("Saved bearing: ") +
          celestial_android::NumberText(sight.m_ShiftBearing) +
          (sight.m_bMagneticShiftBearing ? " deg M" : " deg T");
      if (manual)
        shift += " | " + _("Used true bearing: ") +
            celestial_android::NumberText(std::fmod(
                observation.displacementBearingTrue + 360.0, 360.0)) + " deg T";
    }
    androidShiftDetails.push_back(shift);
#endif
    const long row = m_residuals->InsertItem(
        m_residuals->GetItemCount(),
        UtcDateTime::FormatInstant(observation.utc, "%m-%d %H:%M:%S"));
    m_residuals->SetItem(row, 1, observation.body);
    m_residuals->SetItem(row, 4,
                         wxString::Format("%.2f", sight.m_ShiftNm));
    if (sight.m_ShiftNm != 0.0) {
      m_residuals->SetItem(
          row, 5,
          wxString::Format("%.1f%c %c", sight.m_ShiftBearing, 0x00b0,
                           sight.m_bMagneticShiftBearing ? 'M' : 'T'));
      if (manual)
        m_residuals->SetItem(
            row, 6,
            wxString::Format("%.1f%c T",
                             std::fmod(observation.displacementBearingTrue +
                                           360.0,
                                       360.0),
                             0x00b0));
    }
  }
  if (!epoch.IsValid()) {
    m_stLatitude->SetValue(_("   N/A   "));
    m_stLongitude->SetValue(_("   N/A   "));
    m_stFixError->SetValue(_("Bad epoch"));
    m_runningSummary->SetLabel(_("Select a valid common-epoch date and time."));
    m_bGo->Disable();
    return;
  }
  if (manual && observations.size() >= 2) {
    for (const FixObservation& observation : observations) {
      if (observation.utc.IsLaterThan(epoch)) {
        m_stLatitude->SetValue(_("   N/A   "));
        m_stLongitude->SetValue(_("   N/A   "));
        m_stFixError->SetValue(_("Bad epoch"));
        m_runningSummary->SetLabel(
            _("The common epoch must be at or after every selected sight."));
        m_bGo->Disable();
        return;
      }
    }
  }
  ObserverMotion motion;
  motion.referenceUtc = epoch;
  if (!ReadDr(&motion.latitude, &motion.longitude)) return;
  motion.courseTrue = manual ? 0.0 : m_courseTrue->GetValue();
  motion.speedKnots = manual ? 0.0 : m_speedKnots->GetValue();
  motion.moving = motion.speedKnots != 0.0;
  const RunningFixResult fix = RunningFixSolver::Solve(
      observations, motion, motion.latitude, motion.longitude);
  if (!fix.valid) {
    m_fixlat = m_fixlon = m_fixerror = NAN;
    m_stLatitude->SetValue(_("   N/A   "));
    m_stLongitude->SetValue(_("   N/A   "));
    m_stFixError->SetValue(_("   N/A   "));
    m_runningSummary->SetLabel(fix.error);
    m_bGo->Disable();
    return;
  }
  m_fixlat = fix.latitude;
  m_fixlon = fix.longitude;
  m_fixerror = std::max(0.001, fix.semiMajorNm / 60.0);
  m_stLatitude->SetValue(toSDMM_PlugIn(1, m_fixlat, true));
  m_stLongitude->SetValue(toSDMM_PlugIn(2, m_fixlon, true));
  m_stFixError->SetValue(wxString::Format(_("%.2f' RMS"), fix.rmsMinutes));
  m_runningSummary->SetLabel(wxString::Format(_("%u included sights; "), unsigned(observations.size())) +
      m_motionMode->GetStringSelection());
  m_runningDetails->SetLabel(wxString::Format(
      _("Common epoch %s UTC | %u iterations | RMS %.2f' | uncertainty ellipse %.2f x %.2f NM at %.0f%c"),
#ifdef __OCPN__ANDROID__
      UtcDateTime::FormatInstant(fix.epochUtc, "%Y-%m-%d %H:%M:%S.%l").c_str(),
#else
      UtcDateTime::FormatInstant(fix.epochUtc, "%Y-%m-%d %H:%M:%S").c_str(),
#endif
      fix.iterations, fix.rmsMinutes, fix.semiMajorNm, fix.semiMinorNm,
      fix.ellipseBearing, 0x00b0));
  m_runningDetails->Wrap(740);
  for (size_t index = 0; index < fix.residuals.size(); ++index) {
    const auto& residual = fix.residuals[index];
    const long row = static_cast<long>(index);
    m_residuals->SetItem(row, 2,
                         FormatNavigationAngle(residual.calculatedAltitude));
    m_residuals->SetItem(
        row, 3, wxString::Format("%+.2f'", residual.interceptMinutes));
  }
#ifdef __OCPN__ANDROID__
  wxString details;
  for (size_t index = 0; index < fix.residuals.size(); ++index) {
    const auto& residual = fix.residuals[index];
    details += observations[index].body + " | " +
        UtcDateTime::FormatInstant(observations[index].utc, "%Y-%m-%d %H:%M:%S.%l") +
        " UTC\n" + _("Hc: ") + FormatNavigationAngle(residual.calculatedAltitude) +
        " | " + _("Ho minus Hc: ") +
        wxString::Format("%+.2f'", residual.interceptMinutes) +
        "\n" + androidShiftDetails[index];
    details += "\n\n";
  }
  m_androidResiduals->SetLabel(details);
#endif
  Layout();
  m_Parent->SetLastFix(m_fixlat, m_fixlon, fix.epochUtc);
  m_bGo->Enable();
  RequestRefresh(GetParent()->GetParent());
}

void FixDialog::OnGo(wxCommandEvent& event) {
  if (!m_bGo->IsEnabled() || !std::isfinite(m_fixlat) ||
      !std::isfinite(m_fixlon)) return;
  double scale = 1e-5 / m_fixerror;
  if (scale < 1e-4) scale = 1e-4;

  if (scale > 1e-3) scale = 1e-3;

  JumpToPosition(m_fixlat, m_fixlon, scale);
#ifdef __OCPN__ANDROID__
  m_Parent->OnFixClose();
  m_Parent->Hide();
#endif
}

void FixDialog::OnClose(wxCommandEvent& event) { m_Parent->OnFixClose(); }

void FixDialog::OnWindowClose(wxCloseEvent& event) {
  m_Parent->OnFixClose();
}

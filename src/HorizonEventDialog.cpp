#include "PlatformMessageBox.h"
/******************************************************************************
 * Horizon event observation entry for the Celestial Navigation plugin.
 ******************************************************************************/

#include "HorizonEventDialog.h"

#include <algorithm>
#include <cmath>

#include <wx/calctrl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/display.h>
#include <wx/fileconf.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "OcpnApiCompat.h"
#include "DialogGeometry.h"
#include "UtcDateTime.h"
#include "Utf8Translation.h"
#ifdef __OCPN__ANDROID__
#include "AndroidTouch.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#endif

namespace {

wxSpinCtrlDouble* AddNumber(wxWindow* parent, wxFlexGridSizer* grid,
                            const wxString& label, double value, double min,
                            double max, double increment, int digits,
                            const wxString& units = wxString()) {
  grid->Add(new wxStaticText(parent, wxID_ANY, label), 0,
            wxALIGN_CENTER_VERTICAL);
  wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
  wxSpinCtrlDouble* control = new wxSpinCtrlDouble(
      parent, wxID_ANY, wxString::Format("%.*f", digits, value),
      wxDefaultPosition, wxSize(145, -1), wxSP_ARROW_KEYS, min, max, value,
      increment);
  control->SetDigits(digits);
#ifdef __OCPN__ANDROID__
  control->SetDigits(15);
  control->SetValue(value);
#endif
  row->Add(control, 0);
  if (!units.empty())
    row->Add(new wxStaticText(parent, wxID_ANY, units), 0,
             wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  grid->Add(row, 1, wxEXPAND);
  return control;
}

}  // namespace

HorizonEventDialog::HorizonEventDialog(wxWindow* parent, Sight& sight,
                                       int clockOffset,
                                       const wxString& systemTimeSummary,
                                       Mode mode)
    : wxDialog(parent, wxID_ANY, _("Horizon Event"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight),
      m_clockOffset(clockOffset),
      m_systemTimeSummary(systemTimeSummary),
      m_scroller(NULL) {
  wxBoxSizer* dialogRoot = new wxBoxSizer(wxVERTICAL);
  wxWindow* formParent = this;
  wxBoxSizer* formRoot = dialogRoot;

  wxStaticText* explanation = new wxStaticText(
      formParent, wxID_ANY,
      _("Record first upper-limb appearance at sunrise or final upper-limb "
        "disappearance at sunset. The result is an approximate navigation "
        "constraint, not a sextant-quality fix."));
  explanation->Wrap(590);
  formRoot->Add(explanation, 0, wxEXPAND | wxALL, 8);

  wxStaticBoxSizer* observation = new wxStaticBoxSizer(
      wxVERTICAL, formParent,
#ifdef __OCPN__ANDROID__
      _("Observation"));
#else
      CN_UTF8_("Observation — always visible"));
#endif
  wxFlexGridSizer* obsGrid = new wxFlexGridSizer(0, 2, 5, 10);
  obsGrid->AddGrowableCol(1);

  obsGrid->Add(new wxStaticText(formParent, wxID_ANY, _("Event")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_event = new wxChoice(formParent, wxID_ANY, wxDefaultPosition, wxSize(310, -1));
  m_event->Append(CN_UTF8_("Sunrise — first upper limb"));
  m_event->Append(CN_UTF8_("Sunset — last upper limb"));
  m_event->SetSelection(static_cast<int>(sight.m_HorizonEvent));
  obsGrid->Add(m_event, 0);

  obsGrid->Add(new wxStaticText(formParent, wxID_ANY, _("UTC date")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_calendar = new wxCalendarCtrl(formParent, wxID_ANY, UtcDateTime::CalendarDate(sight.m_DateTime));
  obsGrid->Add(m_calendar, 0);

  obsGrid->Add(new wxStaticText(formParent, wxID_ANY, _("UTC time (24-hour)")), 0,
               wxALIGN_CENTER_VERTICAL);
  wxBoxSizer* timeRow = new wxBoxSizer(wxHORIZONTAL);
  m_hours = new wxSpinCtrl(formParent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                           wxSize(65, -1), wxSP_ARROW_KEYS, 0, 23,
                           UtcDateTime::Fields(sight.m_DateTime).hour);
  m_minutes = new wxSpinCtrl(formParent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(65, -1), wxSP_ARROW_KEYS, 0, 59,
                             UtcDateTime::Fields(sight.m_DateTime).min);
#ifdef __OCPN__ANDROID__
  m_seconds = new wxSpinCtrlDouble(formParent, wxID_ANY);
  m_seconds->SetDigits(3);
  m_seconds->SetRange(0, 59.999);
  m_seconds->SetIncrement(.001);
  m_seconds->SetValue(UtcDateTime::Fields(sight.m_DateTime).sec + sight.m_DateTime.GetMillisecond() / 1000.0);
#else
  m_seconds = new wxSpinCtrl(formParent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(65, -1), wxSP_ARROW_KEYS, 0, 59,
                             UtcDateTime::Fields(sight.m_DateTime).sec);
#endif
  timeRow->Add(m_hours);
  timeRow->Add(new wxStaticText(formParent, wxID_ANY, ":"), 0,
               wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
  timeRow->Add(m_minutes);
  timeRow->Add(new wxStaticText(formParent, wxID_ANY, ":"), 0,
               wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
  timeRow->Add(m_seconds);
  wxButton* capture = new wxButton(formParent, wxID_ANY, _("Capture current UTC"));
  timeRow->Add(capture, 0, wxLEFT, 10);
  obsGrid->Add(timeRow, 0);

  m_timeUncertainty =
      AddNumber(formParent, obsGrid, _("Time uncertainty"), sight.m_TimeCertainty, 0,
                600, 1, 1, _("seconds"));

  obsGrid->Add(new wxStaticText(formParent, wxID_ANY, _("Time source")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_timeSource =
      new wxChoice(formParent, wxID_ANY, wxDefaultPosition, wxSize(310, -1));
  m_timeSource->Append(_("System UTC capture"));
  m_timeSource->Append(_("Synchronised watch / manual entry"));
  m_timeSource->Append(_("Other manual entry"));
  m_timeSource->SetSelection(
      sight.m_HorizonTimeSource.StartsWith("System")         ? 0
      : sight.m_HorizonTimeSource.StartsWith("Synchronised") ? 1
                                                             : 2);
  obsGrid->Add(m_timeSource, 0);
  observation->Add(obsGrid, 0, wxEXPAND | wxALL, 6);
  wxStaticText* clock = new wxStaticText(
      formParent, wxID_ANY, _("Current system timing: ") + m_systemTimeSummary);
  clock->Wrap(590);
  observation->Add(clock, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  formRoot->Add(observation, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticText* lowerHint = new wxStaticText(
      formParent, wxID_ANY,
#ifdef __OCPN__ANDROID__
      _("Bearing, horizon conditions and the result follow below."));
#else
      _("Bearing, horizon conditions and the result are below; scroll this "
        "lower section when necessary."));
#endif
  formRoot->Add(lowerHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

#ifndef __OCPN__ANDROID__
  m_scroller =
      new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
  m_scroller->SetScrollRate(0, 12);
  m_scroller->SetBackgroundColour(GetBackgroundColour());
#endif
  // AndroidSurface supplies a single viewport for the complete form. A
  // nested lower scroller otherwise collapses below the large calendar.
  wxWindow* lowerParent = this;
  if (m_scroller) lowerParent = m_scroller;
  wxBoxSizer* root =
#ifdef __OCPN__ANDROID__
      formRoot;
#else
      new wxBoxSizer(wxVERTICAL);
#endif

  wxStaticBoxSizer* bearingBox =
      new wxStaticBoxSizer(wxVERTICAL, lowerParent, _("Bearing (optional)"));
  m_hasBearing = new wxCheckBox(
      lowerParent, wxID_ANY,
      _("Include a bearing to show possible positions along the event LOP"));
  m_hasBearing->SetValue(sight.m_HorizonBearingProvided);
  bearingBox->Add(m_hasBearing, 0, wxALL, 6);
  wxFlexGridSizer* bearingGrid = new wxFlexGridSizer(0, 2, 5, 10);
  bearingGrid->AddGrowableCol(1);
  bearingGrid->Add(
      new wxStaticText(lowerParent, wxID_ANY, _("Bearing reference")), 0,
      wxALIGN_CENTER_VERTICAL);
  m_bearingReference = new wxChoice(lowerParent, wxID_ANY);
  m_bearingReference->Append(_("Magnetic compass"));
  m_bearingReference->Append(_("True bearing"));
  m_bearingReference->SetSelection(sight.m_HorizonBearingMagnetic ? 0 : 1);
  bearingGrid->Add(m_bearingReference, 1, wxEXPAND);
  m_bearing =
      AddNumber(lowerParent, bearingGrid, _("Observed bearing"),
                sight.m_HorizonBearing, 0, 359.99, 0.1, 2, _("degrees"));
  m_variation =
      AddNumber(lowerParent, bearingGrid, _("Magnetic variation (E + / W -)"),
                sight.m_HorizonVariation, -90, 90, 0.1, 2, _("degrees"));
  m_deviation =
      AddNumber(lowerParent, bearingGrid, _("Compass deviation (E + / W -)"),
                sight.m_HorizonDeviation, -45, 45, 0.1, 2, _("degrees"));
  m_bearingUncertainty =
      AddNumber(lowerParent, bearingGrid, _("Bearing uncertainty (+/-)"),
                sight.m_HorizonBearingUncertainty, 0, 30, 0.1, 1, _("degrees"));
  bearingGrid->Add(
      new wxStaticText(lowerParent, wxID_ANY, _("Effective bearing")), 0,
      wxALIGN_CENTER_VERTICAL);
  m_trueBearing = new wxStaticText(lowerParent, wxID_ANY, wxEmptyString);
  bearingGrid->Add(m_trueBearing, 1, wxEXPAND);
  bearingBox->Add(bearingGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  root->Add(bearingBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* conditions =
      new wxStaticBoxSizer(wxVERTICAL, lowerParent, _("Horizon conditions"));
  wxFlexGridSizer* conditionsGrid = new wxFlexGridSizer(0, 2, 5, 10);
  conditionsGrid->AddGrowableCol(1);
  m_eyeHeight = AddNumber(lowerParent, conditionsGrid, _("Height of eye"),
                          sight.m_EyeHeight, 0, 100, 0.1, 2, _("metres"));
  m_temperature = AddNumber(lowerParent, conditionsGrid, _("Air temperature"),
                            sight.m_Temperature, -60, 60, 0.5, 1, _("C"));
  m_pressure = AddNumber(lowerParent, conditionsGrid, _("Pressure"),
                         sight.m_Pressure, 850, 1100, 1, 1, _("hPa"));
  conditionsGrid->Add(
      new wxStaticText(lowerParent, wxID_ANY, _("Horizon quality")), 0,
      wxALIGN_CENTER_VERTICAL);
  m_horizonQuality = new wxChoice(lowerParent, wxID_ANY);
  m_horizonQuality->Append(_("Clear sea horizon"));
  m_horizonQuality->Append(_("Hazy or indistinct horizon"));
  m_horizonQuality->Append(_("Obstructed or land horizon"));
  m_horizonQuality->SetSelection(sight.m_HorizonQuality);
  conditionsGrid->Add(m_horizonQuality, 1, wxEXPAND);
  m_altitudeUncertainty = AddNumber(
      lowerParent, conditionsGrid, _("Horizon/refraction uncertainty (+/-)"),
      sight.m_HorizonAltitudeUncertainty, 0, 180, 1, 1, _("arcminutes"));
  conditions->Add(conditionsGrid, 0, wxEXPAND | wxALL, 6);
  root->Add(conditions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* result =
      new wxStaticBoxSizer(wxVERTICAL, lowerParent, _("Estimated result"));
  m_preview = new wxStaticText(lowerParent, wxID_ANY, wxEmptyString);
  m_preview->Wrap(590);
  result->Add(m_preview, 0, wxEXPAND | wxALL, 6);
  root->Add(result, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

#ifndef __OCPN__ANDROID__
  m_scroller->SetSizer(root);
  root->Layout();
  m_scroller->FitInside();
  dialogRoot->Add(m_scroller, 1, wxEXPAND);
#endif

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer;
  wxButton* ok = new wxButton(this, wxID_OK);
  wxButton* cancel = new wxButton(this, wxID_CANCEL);
  ok->SetLabel(mode == Mode::Create ? _("Create Event")
                                    : _("Save Changes"));
  buttons->AddButton(ok);
  buttons->AddButton(cancel);
  buttons->Realize();
  dialogRoot->Add(buttons, 0, wxEXPAND | wxALL, 8);

  SetSizer(dialogRoot);
  SetMinSize(wxSize(600, 540));
  wxRect displayArea(0, 0, 700, 760);
  const int displayIndex = wxDisplay::GetFromWindow(parent);
  if (displayIndex != wxNOT_FOUND)
    displayArea =
        wxDisplay(static_cast<unsigned int>(displayIndex)).GetClientArea();
  else if (wxDisplay::GetCount() > 0)
    displayArea = wxDisplay(static_cast<unsigned int>(0)).GetClientArea();
  const int width = std::max(600, std::min(700, displayArea.GetWidth() - 40));
  const int height = std::max(540, std::min(760, displayArea.GetHeight() - 80));
  dialog_geometry::Restore(this, _T("HorizonEvent"), wxSize(width, height));
  SetAffirmativeId(wxID_OK);
  SetEscapeId(wxID_CANCEL);
  ok->SetDefault();

  capture->Bind(wxEVT_BUTTON, &HorizonEventDialog::OnCaptureNow, this);
  ok->Bind(wxEVT_BUTTON, &HorizonEventDialog::OnOK, this);
  Bind(wxEVT_CLOSE_WINDOW, &HorizonEventDialog::OnWindowClose, this);
  m_hasBearing->Bind(wxEVT_CHECKBOX, &HorizonEventDialog::OnInputChanged, this);
  m_event->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged, this);
  m_bearingReference->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged,
                           this);
  m_timeSource->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged, this);
  m_calendar->Bind(wxEVT_CALENDAR_SEL_CHANGED,
                   &HorizonEventDialog::OnCalendarChanged, this);
  m_horizonQuality->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnQualityChanged,
                         this);
#ifdef __OCPN__ANDROID__
  for (wxWindow* control : {static_cast<wxWindow*>(m_hours), static_cast<wxWindow*>(m_minutes), static_cast<wxWindow*>(m_seconds)})
#else
  for (wxSpinCtrl* control : {m_hours, m_minutes, m_seconds})
#endif
    control->Bind(wxEVT_TEXT, &HorizonEventDialog::OnInputChanged, this);
  for (wxSpinCtrlDouble* control :
       {m_timeUncertainty, m_bearing, m_variation, m_deviation,
        m_bearingUncertainty, m_eyeHeight, m_temperature, m_pressure,
        m_altitudeUncertainty})
    control->Bind(wxEVT_TEXT, &HorizonEventDialog::OnInputChanged, this);

  UpdateBearingControls();
  UpdatePreview();
  m_transaction.StartTracking();
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): typed wxQt spin values need native notifications.
  // Coalesce preview updates after callbacks return and own them by the dialog.
  auto* previewTimer = new QTimer(GetHandle());
  previewTimer->setSingleShot(true);
  QObject::connect(previewTimer, &QTimer::timeout, GetHandle(),
                   [this]() { UpdatePreview(); });
  const auto changed = [this, previewTimer](bool timeInput) {
    if (m_androidCapturingTime) return;
    if (timeInput && m_timeSource->GetSelection() == 0)
      m_timeSource->SetSelection(2);
    MarkDirty();
    previewTimer->start(0);
  };
  for (auto* control : {m_hours, m_minutes})
    if (auto* spin = qobject_cast<QSpinBox*>(control->GetHandle()))
      QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                       previewTimer, [changed](int) { changed(true); });
  for (auto* control : {m_seconds, m_timeUncertainty, m_bearing, m_variation,
                       m_deviation, m_bearingUncertainty, m_eyeHeight,
                       m_temperature, m_pressure, m_altitudeUncertainty})
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(control->GetHandle())) {
      const bool timeInput = control == m_seconds;
      QObject::connect(spin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), previewTimer,
          [changed, timeInput](double) { changed(timeInput); });
    }
#endif
}

HorizonEventDialog::~HorizonEventDialog() {
  dialog_geometry::Save(this, _T("HorizonEvent"));
}

void HorizonEventDialog::MarkDirty() {
  m_transaction.MarkChanged();
}

void HorizonEventDialog::RelayoutContent() {
#ifdef __OCPN__ANDROID__
  CN_WrapAndroidText(m_preview, m_preview->GetLabel(),
                     std::max(160, GetClientSize().x - 64));
  CN_WrapAndroidText(m_trueBearing, m_trueBearing->GetLabel(),
                     std::max(160, GetClientSize().x - 64));
  for (wxWindow* parent = m_preview->GetParent(); parent && parent != this;
       parent = parent->GetParent()) {
    parent->Layout();
    if (auto* scroll = wxDynamicCast(parent, wxScrolledWindow))
      scroll->FitInside();
  }
#endif
  if (m_scroller && m_scroller->GetSizer()) {
    m_scroller->GetSizer()->Layout();
    m_scroller->FitInside();
  }
  Layout();
}

void HorizonEventDialog::ReadControls(Sight& sight) const {
  sight.m_Type = Sight::HORIZON;
  sight.m_Body = _T("Sun");
  sight.m_BodyLimb = Sight::UPPER;
  sight.m_HorizonEvent =
      static_cast<Sight::HorizonEvent>(m_event->GetSelection());

  wxDateTime date = m_calendar->GetDate();
#ifdef __OCPN__ANDROID__
  date = UtcDateTime::FromCalendar(date, m_hours->GetValue(),
                                  m_minutes->GetValue(), m_seconds->GetValue());
#else
  date.SetHour(m_hours->GetValue());
  date.SetMinute(m_minutes->GetValue());
  date.SetSecond(m_seconds->GetValue());
  date.SetMillisecond(0);
#endif
  sight.m_DateTime = date;
  sight.m_TimeCertainty = m_timeUncertainty->GetValue();
  if (m_timeSource->GetSelection() == 0)
    sight.m_HorizonTimeSource = _T("System UTC - ") + m_systemTimeSummary;
  else
    sight.m_HorizonTimeSource = m_timeSource->GetStringSelection();

  sight.m_HorizonBearingProvided = m_hasBearing->GetValue();
  sight.m_HorizonBearingMagnetic = m_bearingReference->GetSelection() == 0;
  sight.m_HorizonBearing = m_bearing->GetValue();
  sight.m_HorizonVariation = m_variation->GetValue();
  sight.m_HorizonDeviation = m_deviation->GetValue();
  sight.m_HorizonBearingUncertainty = m_bearingUncertainty->GetValue();
  sight.m_EyeHeight = m_eyeHeight->GetValue();
  sight.m_Temperature = m_temperature->GetValue();
  sight.m_Pressure = m_pressure->GetValue();
  sight.m_HorizonQuality = m_horizonQuality->GetSelection();
  sight.m_HorizonAltitudeUncertainty = m_altitudeUncertainty->GetValue();
}

void HorizonEventDialog::UpdateBearingControls() {
  const bool hasBearing = m_hasBearing->GetValue();
  const bool magnetic = hasBearing && m_bearingReference->GetSelection() == 0;
  m_bearingReference->Enable(hasBearing);
  m_bearing->Enable(hasBearing);
  m_bearingUncertainty->Enable(hasBearing);
  m_variation->Enable(magnetic);
  m_deviation->Enable(magnetic);
}

void HorizonEventDialog::UpdatePreview() {
  UpdateBearingControls();
  Sight candidate = m_sight;
  ReadControls(candidate);
  candidate.Recompute(m_clockOffset);

  if (!candidate.m_HorizonBearingProvided) {
    m_trueBearing->SetLabel(_("Not supplied"));
    m_preview->SetLabel(candidate.HorizonPositionSummary());
#ifndef __OCPN__ANDROID__
    m_preview->Wrap(560);
#endif
    RelayoutContent();
    return;
  }

  m_trueBearing->SetLabel(wxString::Format(
      _T("%.2f%c true  (%.2f%c %s %+0.2f%c variation %+0.2f%c deviation)"),
      candidate.HorizonTrueBearing(), 0x00B0, candidate.m_HorizonBearing,
      0x00B0, candidate.m_HorizonBearingMagnetic ? _("magnetic") : _("true"),
      candidate.m_HorizonBearingMagnetic ? candidate.m_HorizonVariation : 0,
      0x00B0,
      candidate.m_HorizonBearingMagnetic ? candidate.m_HorizonDeviation : 0,
      0x00B0));

  wxString warning;
  double sunLat;
  candidate.BodyLocation(candidate.m_CorrectedDateTime, &sunLat, 0, 0, 0, 0);
  if (fabs(sunLat) < 3.0)
    warning += _(" Weak latitude geometry near the equinox.");
  if (candidate.m_HorizonQuality == 1)
    warning += _(" Haze increases event-time and refraction uncertainty.");
  else if (candidate.m_HorizonQuality == 2)
    warning +=
        _(" An obstructed horizon may introduce a large systematic "
          "error.");

  if (candidate.m_HorizonAltitudeUncertainty < 1.0)
    warning += _(" Sub-arcminute horizon uncertainty is very optimistic: "
                 "near-horizon refraction varies with atmospheric conditions.");
  m_preview->SetLabel(candidate.HorizonPositionSummary() + warning);
#ifndef __OCPN__ANDROID__
  m_preview->Wrap(560);
#endif
  RelayoutContent();
}

void HorizonEventDialog::OnCaptureNow(wxCommandEvent& event) {
  MarkDirty();
  const wxDateTime now = UtcDateTime::Now();
#ifdef __OCPN__ANDROID__
  m_androidCapturingTime = true;
#endif
  m_calendar->SetDate(UtcDateTime::CalendarDate(now));
  m_hours->SetValue(UtcDateTime::Fields(now).hour);
  m_minutes->SetValue(UtcDateTime::Fields(now).min);
#ifdef __OCPN__ANDROID__
  m_seconds->SetValue(UtcDateTime::Fields(now).sec + now.GetMillisecond() / 1000.0);
#else
  m_seconds->SetValue(UtcDateTime::Fields(now).sec);
#endif
  m_timeSource->SetSelection(0);
#ifdef __OCPN__ANDROID__
  m_androidCapturingTime = false;
#endif
  UpdatePreview();
}

void HorizonEventDialog::OnInputChanged(wxCommandEvent& event) {
#ifdef __OCPN__ANDROID__
  if (m_androidCapturingTime) return;
  if ((event.GetEventObject() == m_hours || event.GetEventObject() == m_minutes ||
       event.GetEventObject() == m_seconds) && m_timeSource->GetSelection() == 0)
    m_timeSource->SetSelection(2);
#endif
  MarkDirty();
  UpdatePreview();
}

void HorizonEventDialog::OnCalendarChanged(wxCalendarEvent& event) {
#ifdef __OCPN__ANDROID__
  if (m_androidCapturingTime) return;
  if (m_timeSource->GetSelection() == 0) m_timeSource->SetSelection(2);
#endif
  MarkDirty();
  UpdatePreview();
}

void HorizonEventDialog::OnQualityChanged(wxCommandEvent& event) {
  MarkDirty();
  const double defaults[] = {10.0, 20.0, 60.0};
  const int selection = m_horizonQuality->GetSelection();
  if (selection >= 0 && selection < 3)
    m_altitudeUncertainty->SetValue(defaults[selection]);
  UpdatePreview();
}

void HorizonEventDialog::OnOK(wxCommandEvent& event) {
  ReadControls(m_sight);

  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation"));
  config->Write(_T("HorizonMagneticVariation"), m_sight.m_HorizonVariation);
  config->Write(_T("HorizonCompassDeviation"), m_sight.m_HorizonDeviation);
  config->Write(_T("HorizonBearingUncertainty"),
                m_sight.m_HorizonBearingUncertainty);
  config->Write(_T("HorizonAltitudeUncertainty"),
                m_sight.m_HorizonAltitudeUncertainty);
  config->Write(_T("HorizonQuality"), m_sight.m_HorizonQuality);

  EndModal(wxID_OK);
}

void HorizonEventDialog::OnWindowClose(wxCloseEvent& event) {
  if (m_transaction.HasUnsavedChanges() && event.CanVeto()) {
    CelestialMessageDialog confirm(
        this, _("Discard your changes?"), _("Unsaved Horizon Event"),
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    confirm.SetYesNoLabels(_("Discard Changes"), _("Keep Editing"));
    if (confirm.ShowModal() != wxID_YES) {
      event.Veto();
      return;
    }
  }
  if (IsModal())
    EndModal(wxID_CANCEL);
  else
    event.Skip();
}

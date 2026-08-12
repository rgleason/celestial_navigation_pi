/******************************************************************************
 * Horizon event observation entry for the Celestial Navigation plugin.
 ******************************************************************************/

#include "HorizonEventDialog.h"

#include <cmath>

#include <wx/calctrl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/fileconf.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "OcpnApiCompat.h"

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
      wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, min, max, value,
      increment);
  control->SetDigits(digits);
  row->Add(control, 1, wxEXPAND);
  if (!units.empty())
    row->Add(new wxStaticText(parent, wxID_ANY, units), 0,
             wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  grid->Add(row, 1, wxEXPAND);
  return control;
}

}  // namespace

HorizonEventDialog::HorizonEventDialog(wxWindow* parent, Sight& sight,
                                       int clockOffset,
                                       const wxString& systemTimeSummary)
    : wxDialog(parent, wxID_ANY, _("Horizon Event"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight),
      m_clockOffset(clockOffset),
      m_systemTimeSummary(systemTimeSummary) {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

  wxStaticText* explanation = new wxStaticText(
      this, wxID_ANY,
      _("Record first upper-limb appearance at sunrise or final upper-limb "
        "disappearance at sunset. The result is an approximate navigation "
        "constraint, not a sextant-quality fix."));
  explanation->Wrap(590);
  root->Add(explanation, 0, wxEXPAND | wxALL, 8);

  wxStaticBoxSizer* observation =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Observation"));
  wxFlexGridSizer* obsGrid = new wxFlexGridSizer(0, 2, 5, 10);
  obsGrid->AddGrowableCol(1);

  obsGrid->Add(new wxStaticText(this, wxID_ANY, _("Event")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_event = new wxChoice(this, wxID_ANY);
  m_event->Append(_("Sunrise — first upper limb"));
  m_event->Append(_("Sunset — last upper limb"));
  m_event->SetSelection(static_cast<int>(sight.m_HorizonEvent));
  obsGrid->Add(m_event, 1, wxEXPAND);

  obsGrid->Add(new wxStaticText(this, wxID_ANY, _("UTC date")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_calendar = new wxCalendarCtrl(this, wxID_ANY, sight.m_DateTime);
  obsGrid->Add(m_calendar, 1, wxEXPAND);

  obsGrid->Add(new wxStaticText(this, wxID_ANY, _("UTC time")), 0,
               wxALIGN_CENTER_VERTICAL);
  wxBoxSizer* timeRow = new wxBoxSizer(wxHORIZONTAL);
  m_hours = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                           wxSize(65, -1), wxSP_ARROW_KEYS, 0, 23,
                           sight.m_DateTime.GetHour());
  m_minutes = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(65, -1), wxSP_ARROW_KEYS, 0, 59,
                             sight.m_DateTime.GetMinute());
  m_seconds = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                             wxSize(65, -1), wxSP_ARROW_KEYS, 0, 59,
                             sight.m_DateTime.GetSecond());
  timeRow->Add(m_hours);
  timeRow->Add(new wxStaticText(this, wxID_ANY, ":"), 0,
               wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
  timeRow->Add(m_minutes);
  timeRow->Add(new wxStaticText(this, wxID_ANY, ":"), 0,
               wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
  timeRow->Add(m_seconds);
  wxButton* capture = new wxButton(this, wxID_ANY, _("Capture current UTC"));
  timeRow->Add(capture, 0, wxLEFT, 10);
  obsGrid->Add(timeRow, 1, wxEXPAND);

  m_timeUncertainty =
      AddNumber(this, obsGrid, _("Time uncertainty"), sight.m_TimeCertainty, 0,
                600, 1, 1, _("seconds"));

  obsGrid->Add(new wxStaticText(this, wxID_ANY, _("Time source")), 0,
               wxALIGN_CENTER_VERTICAL);
  m_timeSource = new wxChoice(this, wxID_ANY);
  m_timeSource->Append(_("System UTC capture"));
  m_timeSource->Append(_("Synchronised watch / manual entry"));
  m_timeSource->Append(_("Other manual entry"));
  m_timeSource->SetSelection(
      sight.m_HorizonTimeSource.StartsWith("System")         ? 0
      : sight.m_HorizonTimeSource.StartsWith("Synchronised") ? 1
                                                             : 2);
  obsGrid->Add(m_timeSource, 1, wxEXPAND);
  observation->Add(obsGrid, 0, wxEXPAND | wxALL, 6);
  wxStaticText* clock = new wxStaticText(
      this, wxID_ANY, _("Current system timing: ") + m_systemTimeSummary);
  clock->Wrap(590);
  observation->Add(clock, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  root->Add(observation, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* bearingBox =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Bearing (optional)"));
  m_hasBearing = new wxCheckBox(
      this, wxID_ANY,
      _("Include a compass bearing to obtain a rough position estimate"));
  m_hasBearing->SetValue(sight.m_HorizonBearingProvided);
  bearingBox->Add(m_hasBearing, 0, wxALL, 6);
  wxFlexGridSizer* bearingGrid = new wxFlexGridSizer(0, 2, 5, 10);
  bearingGrid->AddGrowableCol(1);
  bearingGrid->Add(new wxStaticText(this, wxID_ANY, _("Bearing reference")), 0,
                   wxALIGN_CENTER_VERTICAL);
  m_bearingReference = new wxChoice(this, wxID_ANY);
  m_bearingReference->Append(_("Magnetic compass"));
  m_bearingReference->Append(_("True bearing"));
  m_bearingReference->SetSelection(sight.m_HorizonBearingMagnetic ? 0 : 1);
  bearingGrid->Add(m_bearingReference, 1, wxEXPAND);
  m_bearing =
      AddNumber(this, bearingGrid, _("Observed bearing"),
                sight.m_HorizonBearing, 0, 359.99, 0.1, 2, _("degrees"));
  m_variation =
      AddNumber(this, bearingGrid, _("Magnetic variation (E + / W -)"),
                sight.m_HorizonVariation, -90, 90, 0.1, 2, _("degrees"));
  m_deviation =
      AddNumber(this, bearingGrid, _("Compass deviation (E + / W -)"),
                sight.m_HorizonDeviation, -45, 45, 0.1, 2, _("degrees"));
  m_bearingUncertainty =
      AddNumber(this, bearingGrid, _("Bearing uncertainty (+/-)"),
                sight.m_HorizonBearingUncertainty, 0, 30, 0.1, 1, _("degrees"));
  bearingGrid->Add(new wxStaticText(this, wxID_ANY, _("Effective bearing")), 0,
                   wxALIGN_CENTER_VERTICAL);
  m_trueBearing = new wxStaticText(this, wxID_ANY, wxEmptyString);
  bearingGrid->Add(m_trueBearing, 1, wxEXPAND);
  bearingBox->Add(bearingGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  root->Add(bearingBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* conditions =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Horizon conditions"));
  wxFlexGridSizer* conditionsGrid = new wxFlexGridSizer(0, 2, 5, 10);
  conditionsGrid->AddGrowableCol(1);
  m_eyeHeight = AddNumber(this, conditionsGrid, _("Height of eye"),
                          sight.m_EyeHeight, 0, 100, 0.1, 2, _("metres"));
  m_temperature = AddNumber(this, conditionsGrid, _("Air temperature"),
                            sight.m_Temperature, -60, 60, 0.5, 1, _("C"));
  m_pressure = AddNumber(this, conditionsGrid, _("Pressure"), sight.m_Pressure,
                         850, 1100, 1, 1, _("hPa"));
  conditionsGrid->Add(new wxStaticText(this, wxID_ANY, _("Horizon quality")), 0,
                      wxALIGN_CENTER_VERTICAL);
  m_horizonQuality = new wxChoice(this, wxID_ANY);
  m_horizonQuality->Append(_("Clear sea horizon"));
  m_horizonQuality->Append(_("Hazy or indistinct horizon"));
  m_horizonQuality->Append(_("Obstructed or land horizon"));
  m_horizonQuality->SetSelection(sight.m_HorizonQuality);
  conditionsGrid->Add(m_horizonQuality, 1, wxEXPAND);
  m_altitudeUncertainty = AddNumber(
      this, conditionsGrid, _("Horizon/refraction uncertainty (+/-)"),
      sight.m_HorizonAltitudeUncertainty, 0, 180, 1, 1, _("arcminutes"));
  conditions->Add(conditionsGrid, 0, wxEXPAND | wxALL, 6);
  root->Add(conditions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* result =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Estimated result"));
  m_preview = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_preview->Wrap(590);
  result->Add(m_preview, 0, wxEXPAND | wxALL, 6);
  root->Add(result, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer;
  wxButton* ok = new wxButton(this, wxID_OK);
  wxButton* cancel = new wxButton(this, wxID_CANCEL);
  buttons->AddButton(ok);
  buttons->AddButton(cancel);
  buttons->Realize();
  root->Add(buttons, 0, wxEXPAND | wxALL, 8);

  SetSizerAndFit(root);
  SetMinSize(wxSize(640, GetSize().y));
  CentreOnParent();

  capture->Bind(wxEVT_BUTTON, &HorizonEventDialog::OnCaptureNow, this);
  ok->Bind(wxEVT_BUTTON, &HorizonEventDialog::OnOK, this);
  m_hasBearing->Bind(wxEVT_CHECKBOX, &HorizonEventDialog::OnInputChanged, this);
  m_event->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged, this);
  m_bearingReference->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged,
                           this);
  m_timeSource->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnInputChanged, this);
  m_calendar->Bind(wxEVT_CALENDAR_SEL_CHANGED,
                   &HorizonEventDialog::OnCalendarChanged, this);
  m_horizonQuality->Bind(wxEVT_CHOICE, &HorizonEventDialog::OnQualityChanged,
                         this);
  for (wxSpinCtrl* control : {m_hours, m_minutes, m_seconds})
    control->Bind(wxEVT_TEXT, &HorizonEventDialog::OnInputChanged, this);
  for (wxSpinCtrlDouble* control :
       {m_timeUncertainty, m_bearing, m_variation, m_deviation,
        m_bearingUncertainty, m_eyeHeight, m_temperature, m_pressure,
        m_altitudeUncertainty})
    control->Bind(wxEVT_TEXT, &HorizonEventDialog::OnInputChanged, this);

  UpdateBearingControls();
  UpdatePreview();
}

void HorizonEventDialog::ReadControls(Sight& sight) const {
  sight.m_Type = Sight::HORIZON;
  sight.m_Body = _T("Sun");
  sight.m_BodyLimb = Sight::UPPER;
  sight.m_HorizonEvent =
      static_cast<Sight::HorizonEvent>(m_event->GetSelection());

  wxDateTime date = m_calendar->GetDate();
  date.SetHour(m_hours->GetValue());
  date.SetMinute(m_minutes->GetValue());
  date.SetSecond(m_seconds->GetValue());
  date.SetMillisecond(0);
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
    m_preview->SetLabel(
        _("This observation will plot a horizon-event line of position. Add "
          "another sight or event to obtain a fix."));
    Layout();
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

  double lat, lon;
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

  if (candidate.HorizonEstimatedPosition(&lat, &lon)) {
    if (fabs(lat) > 65.0)
      warning +=
          _(" High-latitude geometry is unusually sensitive to "
            "refraction.");
    m_preview->SetLabel(wxString::Format(
        _("Rough bearing-derived estimate: %s  %s\n"
          "Conservative uncertainty radius: %.1f NM.%s"),
        toSDMM_PlugIn(1, lat, true), toSDMM_PlugIn(2, lon, true),
        candidate.HorizonEstimateUncertaintyNm(), warning));
  } else {
    m_preview->SetLabel(
        _("The supplied event and bearing did not produce a stable position "
          "estimate. The time-based line of position can still be saved.") +
        warning);
  }
  Layout();
}

void HorizonEventDialog::OnCaptureNow(wxCommandEvent& event) {
  const wxDateTime now = wxDateTime::UNow();
  m_calendar->SetDate(now);
  m_hours->SetValue(now.GetHour());
  m_minutes->SetValue(now.GetMinute());
  m_seconds->SetValue(now.GetSecond());
  m_timeSource->SetSelection(0);
  UpdatePreview();
}

void HorizonEventDialog::OnInputChanged(wxCommandEvent& event) {
  UpdatePreview();
}

void HorizonEventDialog::OnCalendarChanged(wxCalendarEvent& event) {
  UpdatePreview();
}

void HorizonEventDialog::OnQualityChanged(wxCommandEvent& event) {
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

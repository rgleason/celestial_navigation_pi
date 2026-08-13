/******************************************************************************
 * Horizon event observation entry for the Celestial Navigation plugin.
 ******************************************************************************/

#ifndef _HORIZON_EVENT_DIALOG_H_
#define _HORIZON_EVENT_DIALOG_H_

#include <wx/dialog.h>
#include <wx/calctrl.h>

#include "Sight.h"

class wxCheckBox;
class wxChoice;
class wxCalendarEvent;
class wxScrolledWindow;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxStaticText;

class HorizonEventDialog : public wxDialog {
public:
  HorizonEventDialog(wxWindow* parent, Sight& sight, int clockOffset,
                     const wxString& systemTimeSummary);

private:
  void OnCaptureNow(wxCommandEvent& event);
  void OnInputChanged(wxCommandEvent& event);
  void OnCalendarChanged(wxCalendarEvent& event);
  void OnQualityChanged(wxCommandEvent& event);
  void OnOK(wxCommandEvent& event);
  void UpdatePreview();
  void UpdateBearingControls();
  void RelayoutContent();
  void ReadControls(Sight& sight) const;

  Sight& m_sight;
  int m_clockOffset;
  wxString m_systemTimeSummary;

  wxScrolledWindow* m_scroller;
  wxChoice* m_event;
  wxCalendarCtrl* m_calendar;
  wxSpinCtrl* m_hours;
  wxSpinCtrl* m_minutes;
  wxSpinCtrl* m_seconds;
  wxSpinCtrlDouble* m_timeUncertainty;
  wxChoice* m_timeSource;
  wxCheckBox* m_hasBearing;
  wxChoice* m_bearingReference;
  wxSpinCtrlDouble* m_bearing;
  wxSpinCtrlDouble* m_variation;
  wxSpinCtrlDouble* m_deviation;
  wxSpinCtrlDouble* m_bearingUncertainty;
  wxSpinCtrlDouble* m_eyeHeight;
  wxSpinCtrlDouble* m_temperature;
  wxSpinCtrlDouble* m_pressure;
  wxChoice* m_horizonQuality;
  wxSpinCtrlDouble* m_altitudeUncertainty;
  wxStaticText* m_trueBearing;
  wxStaticText* m_preview;
};

#endif

#ifndef CELESTIAL_NAUTICAL_TIME_CTRL_H
#define CELESTIAL_NAUTICAL_TIME_CTRL_H
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/dateevt.h>
#include <wx/timectrl.h>

// Locale-independent 24-hour HH:MM:SS entry. Native time pickers use AM/PM
// on some hosts even when the surrounding sight form says UTC.
class NauticalTimeCtrl : public wxPanel {
public:
  NauticalTimeCtrl(wxWindow* parent, wxWindowID id, const wxDateTime& value)
      : wxPanel(parent, id), date_(value) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    const int values[] = {value.GetHour(), value.GetMinute(),
                          value.GetSecond()};
    for (int i = 0; i < 3; ++i) {
      controls_[i] =
          new wxSpinCtrl(this, wxID_ANY, wxString::Format("%02d", values[i]),
                         wxDefaultPosition, wxSize(65, -1), wxSP_ARROW_KEYS, 0,
                         i == 0 ? 23 : 59, values[i]);
      controls_[i]->SetToolTip(i == 0   ? _("Hours (00-23)")
                               : i == 1 ? _("Minutes (00-59)")
                                        : _("Seconds (00-59)"));
      row->Add(controls_[i], 0);
      if (i < 2)
        row->Add(new wxStaticText(this, wxID_ANY, ":"), 0,
                 wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
    }
    for (int i = 0; i < 3; ++i) {
      controls_[i]->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) {
        wxDateEvent event(this, GetValue(), wxEVT_TIME_CHANGED);
        ProcessWindowEvent(event);
      });
      controls_[i]->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        wxDateEvent event(this, GetValue(), wxEVT_TIME_CHANGED);
        ProcessWindowEvent(event);
      });
    }
    SetSizer(row);
  }
  wxDateTime GetValue() const {
    wxDateTime value = date_;
    value.SetHour(controls_[0]->GetValue());
    value.SetMinute(controls_[1]->GetValue());
    value.SetSecond(controls_[2]->GetValue());
    value.SetMillisecond(0);
    return value;
  }

private:
  wxDateTime date_;
  wxSpinCtrl* controls_[3];
};
#endif

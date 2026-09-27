#ifndef CELESTIAL_NAUTICAL_TIME_CTRL_H
#define CELESTIAL_NAUTICAL_TIME_CTRL_H
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/dateevt.h>
#include <wx/timectrl.h>
#include "UtcDateTime.h"
#ifdef __OCPN__ANDROID__
#include <wx/weakref.h>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#endif

// Locale-independent 24-hour HH:MM:SS entry. Native time pickers use AM/PM
// on some hosts even when the surrounding sight form says UTC.
class NauticalTimeCtrl : public wxPanel {
public:
  NauticalTimeCtrl(wxWindow* parent, wxWindowID id, const wxDateTime& value = UtcDateTime::Now())
      : wxPanel(parent, id), date_(value) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    const auto fields = UtcDateTime::Fields(value);
    const int values[] = {fields.hour, fields.min, fields.sec};
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
#ifdef __OCPN__ANDROID__
        RequestChange();
#else
        wxDateEvent event(this, GetValue(), wxEVT_TIME_CHANGED);
        ProcessWindowEvent(event);
#endif
      });
      controls_[i]->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
        RequestChange();
#else
        wxDateEvent event(this, GetValue(), wxEVT_TIME_CHANGED);
        ProcessWindowEvent(event);
#endif
      });
    }
#ifdef __OCPN__ANDROID__
    row->Detach(controls_[2]);
    controls_[2]->Hide();
    seconds_ = new wxSpinCtrlDouble(this, wxID_ANY);
    seconds_->SetDigits(3);
    seconds_->SetRange(0, 59.999);
    seconds_->SetIncrement(.001);
    seconds_->SetValue(fields.sec + fields.msec / 1000.0);
    seconds_->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) {
      RequestChange();
    });
    row->Add(seconds_, 1, wxEXPAND);
    for (int i = 0; i < 2; ++i) controls_[i]->SetMinSize(wxSize(120, -1));
    // wxQt's typed spin values do not reliably emit the wx spin event.
    // Coalesce native and wx notifications, after input callbacks return.
    // The timer belongs to this control; Close/unload cancels its callback.
    changeTimer_ = new QTimer(GetHandle());
    changeTimer_->setSingleShot(true);
    wxWeakRef<NauticalTimeCtrl> weakTime(this);
    QObject::connect(changeTimer_, &QTimer::timeout, GetHandle(), [weakTime]() {
      if (!weakTime || weakTime->setting_) return;
      wxDateEvent event(weakTime.get(), weakTime->GetValue(), wxEVT_TIME_CHANGED);
      weakTime->ProcessWindowEvent(event);
    });
    for (int i = 0; i < 2; ++i) {
      if (auto* native = qobject_cast<QSpinBox*>(controls_[i]->GetHandle()))
        QObject::connect(native,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            GetHandle(), [this](int) { RequestChange(); });
    }
    if (auto* native = qobject_cast<QDoubleSpinBox*>(seconds_->GetHandle()))
      QObject::connect(native,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          GetHandle(), [this](double) { RequestChange(); });
#endif
    SetSizer(row);
  }
  wxDateTime GetValue() const {
#ifdef __OCPN__ANDROID__
    const auto f = UtcDateTime::Fields(date_);
    const double second = seconds_ ? seconds_->GetValue() : controls_[2]->GetValue();
    return UtcDateTime::Create(f.year, f.mon + 1, f.mday,
      controls_[0]->GetValue(), controls_[1]->GetValue(), static_cast<int>(second),
      std::lround((second - std::floor(second)) * 1000));
#else
    wxDateTime value = date_;
    value.SetHour(controls_[0]->GetValue());
    value.SetMinute(controls_[1]->GetValue());
    value.SetSecond(controls_[2]->GetValue());
    value.SetMillisecond(0);
    return value;
#endif
  }
#ifdef __OCPN__ANDROID__
  void SetValue(const wxDateTime& value) {
    if (!value.IsValid()) return;
    setting_ = true;
    if (changeTimer_) changeTimer_->stop();
    date_ = value;
    const auto f = UtcDateTime::Fields(value);
    controls_[0]->SetValue(f.hour); controls_[1]->SetValue(f.min);
    controls_[2]->SetValue(f.sec);
    if (seconds_) seconds_->SetValue(f.sec + f.msec / 1000.0);
    setting_ = false;
  }
#endif
private:
#ifdef __OCPN__ANDROID__
  void RequestChange() {
    if (!setting_ && changeTimer_) changeTimer_->start(0);
  }
  QTimer* changeTimer_ = nullptr;
  wxSpinCtrlDouble* seconds_ = nullptr;
  bool setting_ = false;
#endif
  wxDateTime date_;
  wxSpinCtrl* controls_[3];
};
#endif

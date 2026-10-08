// Focused touch angle entry. Android only; shared values remain decimal degrees.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include "AndroidAngleText.h"
#include <QLineEdit>
#include <QPushButton>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <cmath>

namespace celestial_android {
inline bool EditAngle(wxWindow* parent, int kind, double minimum, double maximum,
                      double original, double* result) {
  wxDialog sheet(parent, wxID_ANY, kind == 1 ? _("Latitude") : kind == 2
      ? _("Longitude") : _("Measured angle"));
  auto* root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(&sheet, wxID_ANY,
      _("Enter degrees and decimal minutes. The saved value retains full precision.")),
      0, wxEXPAND | wxALL, 12);
  auto* direction = new wxChoice(&sheet, wxID_ANY);
  if (kind == 1) { direction->Append(_("North (+)")); direction->Append(_("South (-)")); }
  else if (kind == 2) { direction->Append(_("East (+)")); direction->Append(_("West (-)")); }
  else { direction->Append(_("Positive")); if (minimum < 0) direction->Append(_("Negative")); }
  direction->SetSelection(original < 0 && direction->GetCount() > 1 ? 1 : 0);
  root->Add(direction, 0, wxEXPAND | wxALL, 12);
  root->Add(new wxStaticText(&sheet, wxID_ANY, _("Degrees")), 0, wxALL, 12);
  const int whole = static_cast<int>(std::floor(std::fabs(original)));
  auto* degrees = new wxSpinCtrl(&sheet, wxID_ANY);
  degrees->SetRange(0, static_cast<int>(std::max(std::fabs(minimum), maximum)));
  degrees->SetValue(whole);
  root->Add(degrees, 0, wxEXPAND | wxALL, 12);
  root->Add(new wxStaticText(&sheet, wxID_ANY, _("Decimal minutes (0 to less than 60)")),
            0, wxALL, 12);
  auto* minutes = new wxSpinCtrlDouble(&sheet, wxID_ANY);
  minutes->SetDigits(12); minutes->SetRange(0, 59.999999999999);
  minutes->SetIncrement(.001); minutes->SetValue((std::fabs(original) - whole) * 60);
  const double initialMinutes = minutes->GetValue();
  root->Add(minutes, 0, wxEXPAND | wxALL, 12);
  auto* error = new wxStaticText(&sheet, wxID_ANY, wxEmptyString);
  root->Add(error, 0, wxEXPAND | wxALL, 12);
  auto* ok = new wxButton(&sheet, wxID_OK, _("Apply angle"));
  ok->GetHandle()->setProperty("cnActionText", QString::fromUtf8("Apply angle"));
  root->Add(ok, 0, wxALL, 12);
  sheet.SetSizer(root);
  bool accepted = false;
  ok->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    CommitNumbers(&sheet);
    double angle = degrees->GetValue() + minutes->GetValue() / 60.0;
    if (direction->GetSelection() == 1) angle = -angle;
    if (!std::isfinite(angle) || angle < minimum || angle > maximum) {
      error->SetLabel(wxString::Format(_("Allowed range: %.0f to %.0f degrees."), minimum, maximum));
      LayoutScrolls(&sheet); return;
    }
    // Opening and applying an unchanged editor must be an exact round trip.
    if (degrees->GetValue() == whole && minutes->GetValue() == initialMinutes &&
        (angle < 0) == (original < 0)) angle = original;
    *result = angle; accepted = true; sheet.EndModal(wxID_OK);
  });
  Decorate(&sheet, sheet.GetTitle());
  sheet.ShowModal();
  return accepted;
}
class AngleButton final : public QObject {
 public:
  AngleButton(wxTextCtrl* field, int kind, double minimum, double maximum)
      : QObject(field->GetHandle()), field_(field) {
    auto* line = qobject_cast<QLineEdit*>(field->GetHandle());
    if (!line) return;
    button_ = new QPushButton(QString::fromUtf8("° / ′"), line);
    button_->setStyleSheet(QString("QPushButton { font-size: %1pt; min-height: %2px; "
        "background: #e6f0f4; color: #173849; border: 1px solid #9fb9c6; }")
        .arg(CN_FontPointSize()).arg(CN_TouchHeight() - 2));
    CN_ApplyAndroidTheme(button_.data());
    new CN_AndroidButtonDragFilter(button_);
    line->setTextMargins(0, 0, 152, 0);
    line->installEventFilter(this);
    QObject::connect(button_, &QPushButton::clicked, this, [this, kind, minimum, maximum]() {
      if (!field_) return;
      double current = 0;
      if (!ParseAngleText(field_->GetValue(), &current)) current = 0;
      double result = current;
      if (EditAngle(field_.get(), kind, minimum, maximum, current, &result) && field_)
        field_->SetValue(celestial_android::NumberText(result));
    });
    Fit(line);
  }
 protected:
  bool eventFilter(QObject* target, QEvent* event) override {
    if (event->type() == QEvent::Resize) Fit(qobject_cast<QWidget*>(target));
    return false;
  }
 private:
  void Fit(QWidget* line) { if (line && button_) button_->setGeometry(line->width() - 148, 0, 148, line->height()); }
  wxWeakRef<wxTextCtrl> field_;
  QPointer<QPushButton> button_;
};
inline void AddAngleEntry(wxTextCtrl* field, int kind, double minimum, double maximum) {
  if (field->GetHandle()->property("cnAngleEntry").toBool()) return;
  field->GetHandle()->setProperty("cnAngleEntry", true);
  new AngleButton(field, kind, minimum, maximum);
}
} // namespace celestial_android
#endif

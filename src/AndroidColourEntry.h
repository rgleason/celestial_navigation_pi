// POBsoft (1985-2026): touch colour entry, Android only. GPL-3.0-or-later.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include <wx/clrpicker.h>

namespace celestial_android {
inline bool EditColour(wxWindow* parent, const wxColour& original,
                       wxColour* result) {
  wxDialog sheet(parent, wxID_ANY, _("Sight colour"));
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* palette = new wxChoice(&sheet, wxID_ANY);
  const wxString names[] = {_("Custom"), _("Black"), _("White"), _("Red"),
      _("Green"), _("Blue"), _("Yellow"), _("Cyan"), _("Magenta")};
  const wxColour colours[] = {original, wxColour(0,0,0), wxColour(255,255,255),
      wxColour(255,0,0), wxColour(0,128,0), wxColour(0,0,255),
      wxColour(255,255,0), wxColour(0,255,255), wxColour(255,0,255)};
  for (const auto& name : names) palette->Append(name);
  palette->SetSelection(0);
  root->Add(palette, 0, wxEXPAND | wxALL, 12);
  wxSpinCtrl* channels[3];
  const wxString labels[] = {_("Red (0 to 255)"), _("Green (0 to 255)"), _("Blue (0 to 255)")};
  const int values[] = {original.Red(), original.Green(), original.Blue()};
  for (int i = 0; i < 3; ++i) {
    root->Add(new wxStaticText(&sheet, wxID_ANY, labels[i]), 0, wxALL, 12);
    channels[i] = new wxSpinCtrl(&sheet, wxID_ANY);
    channels[i]->SetRange(0,255); channels[i]->SetValue(values[i]);
    root->Add(channels[i], 0, wxEXPAND | wxALL, 12);
  }
  auto* swatch = new wxStaticText(&sheet, wxID_ANY, _("Colour preview"));
  swatch->SetMinSize(wxSize(0, CN_TouchHeight()));
  root->Add(swatch, 0, wxEXPAND | wxALL, 12);
  const auto preview = [=]() {
    const wxColour colour(channels[0]->GetValue(), channels[1]->GetValue(),
                          channels[2]->GetValue());
    const wxString hex = colour.GetAsString(wxC2S_HTML_SYNTAX);
    swatch->SetLabel(_("Colour preview: ") + hex);
    const char* ink = (299*colour.Red()+587*colour.Green()+114*colour.Blue() > 128000)
                         ? "black" : "white";
    swatch->GetHandle()->setStyleSheet(QString("QLabel { background: %1; color: %2; "
        "font-size: %3pt; padding: 12px; border: 1px solid #74818a; }")
        .arg(QString::fromUtf8(hex.utf8_str())).arg(ink).arg(CN_FontPointSize()));
  };
  palette->Bind(wxEVT_CHOICE, [=](wxCommandEvent&) {
    const wxColour colour = colours[palette->GetSelection()];
    channels[0]->SetValue(colour.Red()); channels[1]->SetValue(colour.Green());
    channels[2]->SetValue(colour.Blue()); preview();
  });
  for (auto* channel : channels) {
    channel->Bind(wxEVT_SPINCTRL, [=](wxSpinEvent&) { palette->SetSelection(0); preview(); });
    channel->Bind(wxEVT_TEXT, [=](wxCommandEvent&) { palette->SetSelection(0); preview(); });
  }
  auto* apply = new wxButton(&sheet, wxID_OK, _("Apply colour"));
  apply->GetHandle()->setProperty("cnActionText", QString::fromUtf8("Apply colour"));
  root->Add(apply, 0, wxALL, 12);
  sheet.SetSizer(root);
  bool accepted = false;
  apply->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    CommitNumbers(&sheet);
    *result = wxColour(channels[0]->GetValue(), channels[1]->GetValue(), channels[2]->GetValue());
    accepted = true; sheet.EndModal(wxID_OK);
  });
  Decorate(&sheet, sheet.GetTitle()); preview(); sheet.ShowModal();
  return accepted;
}
inline void AddColourEntry(wxColourPickerCtrl* picker, std::function<void()> changed) {
  auto* row = picker->GetContainingSizer();
  auto* button = new wxButton(picker->GetParent(), wxID_ANY,
      _("Choose sight colour: ") + picker->GetColour().GetAsString(wxC2S_HTML_SYNTAX));
  row->Replace(picker, button); picker->Hide();
  button->Bind(wxEVT_BUTTON, [=](wxCommandEvent&) {
    wxColour colour = picker->GetColour();
    if (EditColour(button, colour, &colour)) {
      picker->SetColour(colour);
      const wxString caption = _("Choose sight colour: ") + colour.GetAsString(wxC2S_HTML_SYNTAX);
      button->SetLabel(caption);
      if (auto* native = qobject_cast<QAbstractButton*>(button->GetHandle()))
        native->setText(QString::fromUtf8(caption.utf8_str()));
      changed();
    }
  });
}
} // namespace celestial_android
#endif

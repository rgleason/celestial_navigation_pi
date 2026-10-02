#pragma once
#include <wx/msgdlg.h>
#ifndef __OCPN__ANDROID__
// Desktop preprocessing retains the original wx calls and dialog class.
#define CelestialMessageBox wxMessageBox
#define CelestialMessageDialog wxMessageDialog
#else
#include "AndroidSurface.h"

class CelestialMessageDialog {
 public:
  CelestialMessageDialog(wxWindow* parent, const wxString& message,
      const wxString& caption, long style = wxOK | wxCENTRE)
      : parent_(parent), message_(message), caption_(caption), style_(style) {}
  void SetYesNoLabels(const wxString& yes, const wxString& no) {
    yes_ = yes; no_ = no;
  }
  int ShowModal() {
    using namespace celestial_android;
    wxDialog sheet(parent_, wxID_ANY, caption_);
    sheet.GetHandle()->setProperty("cnCancellable", true);
    auto* body = new wxBoxSizer(wxVERTICAL);
    auto* text = new wxStaticText(&sheet, wxID_ANY, message_);
    text->Wrap(520);
    body->Add(text, 0, wxEXPAND | wxALL, 16);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto add = [&](int result, const wxString& label) {
      auto* button = new wxButton(&sheet, wxID_ANY, label);
      button->Bind(wxEVT_BUTTON, [&sheet, result](wxCommandEvent&) {
        sheet.EndModal(result);
      });
      row->Add(button, 1, wxEXPAND | wxALL, 8);
    };
    if (style_ & wxYES_NO) {
      add(wxID_NO, no_); add(wxID_YES, yes_);
    } else add(wxID_OK, _("OK"));
    if (style_ & wxCANCEL) add(wxID_CANCEL, _("Cancel"));
    body->Add(row, 0, wxEXPAND);
    sheet.SetSizer(body);
    const int result = ModalResult(sheet);
    if (result != wxID_CANCEL || (style_ & wxCANCEL)) return result;
    return style_ & wxYES_NO ? wxID_NO : wxID_OK;
  }
 private:
  wxWindow* parent_;
  wxString message_, caption_, yes_{_("Yes")}, no_{_("No")};
  long style_;
};

inline int CelestialMessageBox(const wxString& message,
    const wxString& caption = _("Celestial Navigation"),
    long style = wxOK | wxCENTRE, wxWindow* parent = nullptr,
    int = wxDefaultCoord, int = wxDefaultCoord) {
  const int result = CelestialMessageDialog(parent, message, caption, style).ShowModal();
  if (result == wxID_YES) return wxYES;
  if (result == wxID_NO) return wxNO;
  return result == wxID_CANCEL ? wxCANCEL : wxOK;
}
#endif

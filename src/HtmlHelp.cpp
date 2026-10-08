#include "PlatformMessageBox.h"
/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 *
 ***************************************************************************
 *   Copyright (C) 2026 by the Celestial Navigation plugin contributors
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 ***************************************************************************/

#include "HtmlHelp.h"
#ifdef __OCPN__ANDROID__
#include "AndroidDocument.h"
#include "AndroidPdf.h"
#endif

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

#include "CelestialNavigationUI.h"
#include "DialogGeometry.h"
#include "celestial_navigation_pi.h"

namespace {

wxString BundledDataPath(const wxString& filename) {
  return celestial_navigation_pi_DataDir() + _T("/data/") + filename;
}

}  // namespace

bool ShowBundledHtmlHelp(wxWindow* parent, const wxString& title,
                         const wxString& filename) {
  const wxString path = BundledDataPath(filename);
  if (!wxFileName::FileExists(path)) {
    CelestialMessageBox(wxString::Format(
                     _("The documentation file could not be found:\n%s"), path),
                 title, wxOK | wxICON_ERROR, parent);
    return false;
  }

#ifdef __OCPN__ANDROID__
  celestial_android::ShowDocument(parent, title, path, true);
  return true;
#else
  const bool practicalGuide = filename == _T("Practical_Guide.html");
  const wxSize initialSize = practicalGuide ? wxSize(900, 760) : wxSize(760, 650);
  InformationDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
                           initialSize);
  if (wxWindow* close = dialog.FindWindow(wxID_OK))
    close->SetLabel(_("Close"));
  dialog.SetMinSize(wxSize(600, 450));
  const wxString geometryKey = practicalGuide ? _T("PracticalGuide") : _T("Documentation");
  dialog_geometry::Restore(&dialog, geometryKey, initialSize);
  if (practicalGuide)
    dialog.m_htmlInformation->SetStandardFonts(12);
  if (practicalGuide) {
    dialog.m_htmlInformation->SetBorders(12);
    dialog.m_htmlInformation->Bind(wxEVT_HTML_LINK_CLICKED,
        [&dialog, title](wxHtmlLinkEvent& event) {
          const wxString href = event.GetLinkInfo().GetHref();
          if (href.StartsWith("https://") || href.StartsWith("http://")) {
            if (!wxLaunchDefaultBrowser(href))
              CelestialMessageBox(_("The web browser could not be opened."),
                                  title, wxOK | wxICON_ERROR, &dialog);
          } else {
            event.Skip();
          }
        });
  }
  if (!dialog.m_htmlInformation->LoadPage(path)) {
    CelestialMessageBox(wxString::Format(
                     _("The documentation file could not be opened:\n%s"), path),
                 title, wxOK | wxICON_ERROR, parent);
    return false;
  }

  dialog.ShowModal();
  dialog_geometry::Save(&dialog, geometryKey);
  return true;
#endif
}

bool OpenBundledDocumentExternally(const wxString& filename,
                                   const wxString& title) {
  const wxString path = BundledDataPath(filename);
#ifdef __OCPN__ANDROID__
  return wxFileName::FileExists(path) && celestial_android::ShowPdf(
      GetCanvasByIndex(0), title.empty() ? _("Celestial Navigation manual") : title, path);
#else
  (void)title;
  return wxFileName::FileExists(path) && wxLaunchDefaultApplication(path);
#endif
}

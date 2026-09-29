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
  InformationDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
                           wxSize(760, 650));
  if (wxWindow* close = dialog.FindWindow(wxID_OK))
    close->SetLabel(_("Close"));
  dialog.SetMinSize(wxSize(600, 450));
  dialog_geometry::Restore(&dialog, _T("Documentation"), wxSize(760, 650));
  if (!dialog.m_htmlInformation->LoadPage(path)) {
    CelestialMessageBox(wxString::Format(
                     _("The documentation file could not be opened:\n%s"), path),
                 title, wxOK | wxICON_ERROR, parent);
    return false;
  }

  dialog.ShowModal();
  dialog_geometry::Save(&dialog, _T("Documentation"));
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

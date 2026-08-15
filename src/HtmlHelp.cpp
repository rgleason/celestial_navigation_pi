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

#include <wx/filename.h>
#include <wx/msgdlg.h>

#include "CelestialNavigationUI.h"
#include "celestial_navigation_pi.h"

bool ShowBundledHtmlHelp(wxWindow* parent, const wxString& title,
                         const wxString& filename) {
  const wxString path = celestial_navigation_pi_DataDir() + _T("/data/") +
                        filename;
  if (!wxFileName::FileExists(path)) {
    wxMessageBox(wxString::Format(
                     _("The documentation file could not be found:\n%s"), path),
                 title, wxOK | wxICON_ERROR, parent);
    return false;
  }

  InformationDialog dialog(parent, wxID_ANY, title, wxDefaultPosition,
                           wxSize(760, 650));
  if (!dialog.m_htmlInformation->LoadPage(path)) {
    wxMessageBox(wxString::Format(
                     _("The documentation file could not be opened:\n%s"), path),
                 title, wxOK | wxICON_ERROR, parent);
    return false;
  }

  dialog.ShowModal();
  return true;
}

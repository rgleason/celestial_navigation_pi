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

#ifndef CELESTIAL_NAVIGATION_HTML_HELP_H
#define CELESTIAL_NAVIGATION_HTML_HELP_H

#include <wx/string.h>

class wxWindow;

// Display a help file from the plugin data directory in an internal,
// scrollable window.  This avoids relying on the operating system's HTML file
// association, which is not guaranteed to point at a web browser.
bool ShowBundledHtmlHelp(wxWindow* parent, const wxString& title,
                         const wxString& filename);

#endif  // CELESTIAL_NAVIGATION_HTML_HELP_H

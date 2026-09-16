/***************************************************************************
 * UTF-8-safe translation helpers for user-interface source strings.
 *
 * wxWidgets' traditional _() macro converts narrow string literals using
 * the active system code page.  That corrupts UTF-8 punctuation and
 * scientific symbols on Windows systems whose code page is not UTF-8.
 ***************************************************************************/

#ifndef CELESTIAL_NAVIGATION_UTF8_TRANSLATION_H
#define CELESTIAL_NAVIGATION_UTF8_TRANSLATION_H

#include <wx/string.h>
#include <wx/translation.h>

inline wxString CelestialTranslateUtf8(const char* source) {
  return wxGetTranslation(wxString::FromUTF8(source));
}

// Keep this as a macro so xgettext can extract the original source literal.
#define CN_UTF8_(source) CelestialTranslateUtf8(source)

#endif  // CELESTIAL_NAVIGATION_UTF8_TRANSLATION_H

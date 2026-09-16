#ifndef CELESTIAL_NAVIGATION_DIALOG_GEOMETRY_H
#define CELESTIAL_NAVIGATION_DIALOG_GEOMETRY_H

#include <wx/gdicmn.h>
#include <wx/string.h>

class wxTopLevelWindow;

namespace dialog_geometry {

// Return a usable rectangle wholly contained by workArea. Keeping this
// calculation independent of wxDisplay makes platform edge cases testable
// without opening a GUI.
wxRect ClampToWorkArea(const wxRect& requested, const wxSize& minimum,
                       const wxRect& workArea);

void Restore(wxTopLevelWindow* dialog, const wxString& key,
             const wxSize& defaultSize);
void EnsureVisible(wxTopLevelWindow* dialog);
void Save(const wxTopLevelWindow* dialog, const wxString& key);

}  // namespace dialog_geometry

#endif

/******************************************************************************
 * Safe, UI-independent XML persistence helpers.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_ATOMIC_XML_FILE_H
#define CELESTIAL_NAVIGATION_ATOMIC_XML_FILE_H

#include <functional>

#include <wx/file.h>
#include <wx/string.h>

class TiXmlDocument;

namespace celestial_navigation {

using TemporaryFileWriter = std::function<bool(wxTempFile&)>;

// Replace destination only after writer has completed successfully.  A failed
// writer is discarded by wxTempFile, leaving any previous destination intact.
bool ReplaceFileAtomically(const wxString& destination,
                           const TemporaryFileWriter& writer,
                           wxString* error = nullptr);

// Copy an already validated file through a same-directory temporary file so
// interruption cannot expose a partial destination.
bool CopyFileAtomically(const wxString& source, const wxString& destination,
                        wxString* error = nullptr);

// Serialize a TinyXML document and safely replace destination.
bool SaveXmlDocumentAtomically(const TiXmlDocument& document,
                               const wxString& destination,
                               wxString* error = nullptr);

}  // namespace celestial_navigation

#endif  // CELESTIAL_NAVIGATION_ATOMIC_XML_FILE_H

#include "AtomicXmlFile.h"

#include <wx/filename.h>

#include "tinyxml.h"

namespace celestial_navigation {
namespace {

void SetError(wxString* error, const wxString& message) {
  if (error) *error = message;
}

}  // namespace

bool ReplaceFileAtomically(const wxString& destination,
                           const TemporaryFileWriter& writer, wxString* error) {
  if (destination.empty()) {
    SetError(error, "The destination path is empty.");
    return false;
  }

  const wxFileName target(destination);
  const wxString parent = target.GetPath();
  if (!parent.empty() && !wxFileName::DirExists(parent) &&
      !wxFileName::Mkdir(parent, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
    SetError(error, "The destination directory could not be created.");
    return false;
  }

  wxTempFile temporary;
  if (!temporary.Open(destination)) {
    SetError(error, "A temporary file could not be opened.");
    return false;
  }
  if (!writer || !writer(temporary)) {
    temporary.Discard();
    SetError(error, "The temporary file could not be written.");
    return false;
  }
  if (!temporary.Flush()) {
    temporary.Discard();
    SetError(error, "The temporary file could not be flushed.");
    return false;
  }
  if (!temporary.Commit()) {
    temporary.Discard();
    SetError(error,
             "The completed temporary file could not replace the destination.");
    return false;
  }
  if (error) error->clear();
  return true;
}

bool SaveXmlDocumentAtomically(const TiXmlDocument& document,
                               const wxString& destination, wxString* error) {
  TiXmlPrinter printer;
  if (!document.Accept(&printer)) {
    SetError(error, "The XML document could not be serialized.");
    return false;
  }
  return ReplaceFileAtomically(
      destination,
      [&printer](wxTempFile& temporary) {
        return temporary.Write(printer.CStr(), printer.Size());
      },
      error);
}

}  // namespace celestial_navigation

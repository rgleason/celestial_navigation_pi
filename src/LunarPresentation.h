#pragma once

#include "StringVector.h"
#include <wx/datetime.h>
#include <wx/string.h>
#include <vector>

class Sight;

namespace lunar_presentation {
struct Table {
  wxString title;
  celnav::StringVector columns;
  std::vector<celnav::StringVector> rows;
  wxString note;
};
struct Report {
  wxString heading;
  std::vector<Table> overview, almanac, search;
};

wxString SolutionId(std::size_t index);
// Round the instant before formatting, including carry into the next UTC day.
wxString ResultUtc(const wxDateTime& utc);
Report BuildReport(const Sight& sight, int selected);
}  // namespace lunar_presentation

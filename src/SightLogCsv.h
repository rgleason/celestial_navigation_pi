#ifndef CELESTIAL_NAVIGATION_SIGHT_LOG_CSV_H
#define CELESTIAL_NAVIGATION_SIGHT_LOG_CSV_H

#include <vector>
#include <wx/string.h>

class Sight;

// Export observations in their current log order without changing the sights.
// UTC includes the log's clock correction. Unsolved lunar watch times are
// blank.
wxString SightLogCsv(const std::vector<Sight>& sights, int clockCorrection);

#endif

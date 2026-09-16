/******************************************************************************
 * Eclipse data-file metadata and installation planning.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_ECLIPSE_DATA_FILES_H
#define CELESTIAL_NAVIGATION_ECLIPSE_DATA_FILES_H

#include <cstdint>
#include <string>
#include <vector>

#include <wx/string.h>

#include "eclipse/data_pack.h"

namespace celestial_navigation {

enum class EclipseDataKind { De440s = 0, LunarOrientation = 1, LolaLimb = 2 };

struct EclipseDataFileSpec {
  EclipseDataKind kind;
  const char* filename;
  const char* display_name;
  std::uint64_t bytes;
  const char* sha256;
  bool optional;
  std::vector<std::string> sources;
};

enum class InstalledDataState { Missing, VerificationRequired, Verified };

const EclipseDataFileSpec& GetEclipseDataFileSpec(EclipseDataKind kind);
eclipse::DataPackStatus VerifyEclipseDataFile(EclipseDataKind kind,
                                              const std::string& path);

// LOLA refinement requires the lunar-orientation kernel.  Nothing else is
// added implicitly to a requested installation.
std::vector<EclipseDataKind> BuildEclipseDataInstallPlan(
    EclipseDataKind requested, bool lunar_orientation_verified);

std::uint64_t DownloadBytes(
    const std::vector<EclipseDataKind>& installation_plan);

// Downloads are verified in a staging file and copied through wxTempFile
// before publication.  Conservatively allow the final files plus the largest
// selected file as temporary working space.
std::uint64_t RequiredWorkingSpaceBytes(
    const std::vector<EclipseDataKind>& installation_plan);

wxString VerificationRecordPath(const wxString& data_path);
InstalledDataState InspectInstalledData(EclipseDataKind kind,
                                        const wxString& data_path);
bool RecordVerifiedDataFile(EclipseDataKind kind, const wxString& data_path,
                            wxString* error = nullptr);
void ForgetVerifiedDataFile(const wxString& data_path);

}  // namespace celestial_navigation

#endif  // CELESTIAL_NAVIGATION_ECLIPSE_DATA_FILES_H

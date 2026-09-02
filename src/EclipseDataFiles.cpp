#include "EclipseDataFiles.h"

#include "AtomicXmlFile.h"

#include <algorithm>
#include <sstream>

#include <wx/file.h>
#include <wx/filename.h>

namespace celestial_navigation {
namespace {

const EclipseDataFileSpec& De440sSpec() {
  static const EclipseDataFileSpec spec = {
      EclipseDataKind::De440s,
      "de440s.bsp",
      "DE440s",
      eclipse::ExpectedDe440sBytes(),
      eclipse::ExpectedDe440sSha256(),
      false,
      {"https://github.com/pob220/celestial_navigation_pi/releases/download/"
       "eclipse-data-2026.1/de440s.bsp",
       "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/"
       "de440s.bsp",
       "https://naif.jpl.nasa.gov/pub/naif/pds/pds4/psyche/psyche_spice/"
       "spice_kernels/spk/de440s.bsp"}};
  return spec;
}

const EclipseDataFileSpec& LunarOrientationSpec() {
  static const EclipseDataFileSpec spec = {
      EclipseDataKind::LunarOrientation,
      "moon_pa_de440_200625.bpc",
      "lunar orientation",
      eclipse::ExpectedLunarOrientationBytes(),
      eclipse::ExpectedLunarOrientationSha256(),
      true,
      {"https://github.com/pob220/celestial_navigation_pi/releases/download/"
       "eclipse-data-2026.1/moon_pa_de440_200625.bpc",
       "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/"
       "moon_pa_de440_200625.bpc",
       "https://naif.jpl.nasa.gov/pub/naif/pds/wgc/kernels/pck/"
       "moon_pa_de440_200625.bpc"}};
  return spec;
}

const EclipseDataFileSpec& LolaSpec() {
  static const EclipseDataFileSpec spec = {
      EclipseDataKind::LolaLimb,
      "lola64-pa.bin",
      "LOLA lunar limb",
      eclipse::ExpectedLola64PaBytes(),
      eclipse::ExpectedLola64PaSha256(),
      true,
      {"https://github.com/pob220/celestial_navigation_pi/releases/download/"
       "eclipse-data-2026.1/lola64-pa.bin"}};
  return spec;
}

wxString ExpectedRecord(EclipseDataKind kind, const wxString& data_path) {
  const wxFileName file(data_path);
  if (!file.FileExists()) return wxString();
  const wxDateTime modified = file.GetModificationTime();
  if (!modified.IsValid()) return wxString();
  const EclipseDataFileSpec& spec = GetEclipseDataFileSpec(kind);
  std::ostringstream record;
  record << "version=1\nkind=" << static_cast<int>(kind)
         << "\nbytes=" << spec.bytes << "\nmtime=" << modified.GetTicks()
         << "\nsha256=" << spec.sha256 << "\n";
  return wxString::FromUTF8(record.str().c_str());
}

}  // namespace

const EclipseDataFileSpec& GetEclipseDataFileSpec(EclipseDataKind kind) {
  if (kind == EclipseDataKind::LunarOrientation) return LunarOrientationSpec();
  if (kind == EclipseDataKind::LolaLimb) return LolaSpec();
  return De440sSpec();
}

eclipse::DataPackStatus VerifyEclipseDataFile(EclipseDataKind kind,
                                              const std::string& path) {
  if (kind == EclipseDataKind::LunarOrientation)
    return eclipse::VerifyLunarOrientationPck(path);
  if (kind == EclipseDataKind::LolaLimb) return eclipse::VerifyLola64Pa(path);
  return eclipse::VerifyDe440s(path);
}

std::vector<EclipseDataKind> BuildEclipseDataInstallPlan(
    EclipseDataKind requested, bool lunar_orientation_verified) {
  std::vector<EclipseDataKind> plan;
  if (requested == EclipseDataKind::LolaLimb && !lunar_orientation_verified)
    plan.push_back(EclipseDataKind::LunarOrientation);
  plan.push_back(requested);
  return plan;
}

std::uint64_t DownloadBytes(
    const std::vector<EclipseDataKind>& installation_plan) {
  std::uint64_t total = 0;
  for (std::vector<EclipseDataKind>::const_iterator iterator =
           installation_plan.begin();
       iterator != installation_plan.end(); ++iterator)
    total += GetEclipseDataFileSpec(*iterator).bytes;
  return total;
}

std::uint64_t RequiredWorkingSpaceBytes(
    const std::vector<EclipseDataKind>& installation_plan) {
  std::uint64_t largest = 0;
  for (std::vector<EclipseDataKind>::const_iterator iterator =
           installation_plan.begin();
       iterator != installation_plan.end(); ++iterator)
    largest = std::max(largest, GetEclipseDataFileSpec(*iterator).bytes);
  return DownloadBytes(installation_plan) + largest + 16u * 1024u * 1024u;
}

wxString VerificationRecordPath(const wxString& data_path) {
  return data_path + ".verified";
}

InstalledDataState InspectInstalledData(EclipseDataKind kind,
                                        const wxString& data_path) {
  const wxFileName file(data_path);
  if (!file.FileExists()) return InstalledDataState::Missing;
  const EclipseDataFileSpec& spec = GetEclipseDataFileSpec(kind);
  if (file.GetSize() != wxULongLong(spec.bytes))
    return InstalledDataState::VerificationRequired;

  const wxString record_path = VerificationRecordPath(data_path);
  if (!wxFileExists(record_path))
    return InstalledDataState::VerificationRequired;
  wxFile record(record_path);
  wxString contents;
  if (!record.IsOpened() || !record.ReadAll(&contents, wxConvUTF8) ||
      contents != ExpectedRecord(kind, data_path))
    return InstalledDataState::VerificationRequired;
  return InstalledDataState::Verified;
}

bool RecordVerifiedDataFile(EclipseDataKind kind, const wxString& data_path,
                            wxString* error) {
  const wxString contents = ExpectedRecord(kind, data_path);
  if (contents.empty()) {
    if (error) *error = "The verified data file is unavailable.";
    return false;
  }
  return ReplaceFileAtomically(
      VerificationRecordPath(data_path),
      [&contents](wxTempFile& temporary) {
        const wxCharBuffer utf8 = contents.utf8_str();
        return utf8.data() && temporary.Write(utf8.data(), utf8.length());
      },
      error);
}

void ForgetVerifiedDataFile(const wxString& data_path) {
  const wxString record = VerificationRecordPath(data_path);
  if (wxFileExists(record)) wxRemoveFile(record);
}

}  // namespace celestial_navigation

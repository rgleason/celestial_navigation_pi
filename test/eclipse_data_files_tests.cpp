#include <gtest/gtest.h>

#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include "EclipseDataFiles.h"

namespace {

class EclipseDataTemporaryDirectory {
public:
  EclipseDataTemporaryDirectory() {
    path = wxFileName::CreateTempFileName("celestial-eclipse-data-test-");
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(wxRemoveFile(path));
    EXPECT_TRUE(wxMkdir(path));
  }

  ~EclipseDataTemporaryDirectory() {
    if (!path.empty()) wxFileName::Rmdir(path, wxPATH_RMDIR_RECURSIVE);
  }

  wxString File(const wxString& name) const {
    return wxFileName(path, name).GetFullPath();
  }

  wxString path;
};

void CreateSparseFile(const wxString& path, std::uint64_t bytes) {
  wxFile file(path, wxFile::write);
  ASSERT_TRUE(file.IsOpened());
  ASSERT_GT(bytes, 0u);
  ASSERT_NE(wxInvalidOffset,
            file.Seek(static_cast<wxFileOffset>(bytes - 1), wxFromStart));
  const unsigned char zero = 0;
  ASSERT_EQ(1u, file.Write(&zero, 1));
  ASSERT_TRUE(file.Flush());
}

}  // namespace

TEST(EclipseDataFiles, PublishesPinnedMetadataAndTrustedSources) {
  using celestial_navigation::EclipseDataKind;
  const celestial_navigation::EclipseDataFileSpec& de =
      celestial_navigation::GetEclipseDataFileSpec(EclipseDataKind::De440s);
  EXPECT_STREQ("de440s.bsp", de.filename);
  EXPECT_EQ(32726016u, de.bytes);
  EXPECT_FALSE(de.optional);
  ASSERT_GE(de.sources.size(), 3u);
  EXPECT_NE(std::string::npos, de.sources[0].find("github.com/pob220/"));
  EXPECT_NE(std::string::npos, de.sources[1].find("naif.jpl.nasa.gov/"));

  const celestial_navigation::EclipseDataFileSpec& pck =
      celestial_navigation::GetEclipseDataFileSpec(
          EclipseDataKind::LunarOrientation);
  EXPECT_EQ(12863488u, pck.bytes);
  EXPECT_TRUE(pck.optional);

  const celestial_navigation::EclipseDataFileSpec& lola =
      celestial_navigation::GetEclipseDataFileSpec(EclipseDataKind::LolaLimb);
  EXPECT_EQ(530841624u, lola.bytes);
  EXPECT_TRUE(lola.optional);
  ASSERT_EQ(1u, lola.sources.size());
}

TEST(EclipseDataFiles, NeverAddsOptionalFilesToADe440Installation) {
  using celestial_navigation::EclipseDataKind;
  const std::vector<EclipseDataKind> plan =
      celestial_navigation::BuildEclipseDataInstallPlan(EclipseDataKind::De440s,
                                                        false);
  ASSERT_EQ(1u, plan.size());
  EXPECT_EQ(EclipseDataKind::De440s, plan[0]);
  EXPECT_EQ(32726016u, celestial_navigation::DownloadBytes(plan));
}

TEST(EclipseDataFiles, LolaAddsOnlyItsRequiredOrientationDependency) {
  using celestial_navigation::EclipseDataKind;
  const std::vector<EclipseDataKind> without_pck =
      celestial_navigation::BuildEclipseDataInstallPlan(
          EclipseDataKind::LolaLimb, false);
  ASSERT_EQ(2u, without_pck.size());
  EXPECT_EQ(EclipseDataKind::LunarOrientation, without_pck[0]);
  EXPECT_EQ(EclipseDataKind::LolaLimb, without_pck[1]);
  EXPECT_EQ(543705112u, celestial_navigation::DownloadBytes(without_pck));
  EXPECT_GT(celestial_navigation::RequiredWorkingSpaceBytes(without_pck),
            1024u * 1024u * 1024u);

  const std::vector<EclipseDataKind> with_pck =
      celestial_navigation::BuildEclipseDataInstallPlan(
          EclipseDataKind::LolaLimb, true);
  ASSERT_EQ(1u, with_pck.size());
  EXPECT_EQ(EclipseDataKind::LolaLimb, with_pck[0]);
}

TEST(EclipseDataFiles, VerificationRecordTracksFileIdentityAndChanges) {
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::InstalledDataState;
  EclipseDataTemporaryDirectory directory;
  const wxString path = directory.File("de440s.bsp");
  EXPECT_EQ(InstalledDataState::Missing,
            celestial_navigation::InspectInstalledData(EclipseDataKind::De440s,
                                                       path));

  CreateSparseFile(path, 32726016u);
  EXPECT_EQ(InstalledDataState::VerificationRequired,
            celestial_navigation::InspectInstalledData(EclipseDataKind::De440s,
                                                       path));
  wxString error;
  ASSERT_TRUE(celestial_navigation::RecordVerifiedDataFile(
      EclipseDataKind::De440s, path, &error))
      << error;
  EXPECT_EQ(InstalledDataState::Verified,
            celestial_navigation::InspectInstalledData(EclipseDataKind::De440s,
                                                       path));

  wxFile changed(path, wxFile::write_append);
  ASSERT_TRUE(changed.IsOpened());
  ASSERT_EQ(1u, changed.Write("x", 1));
  changed.Close();
  EXPECT_EQ(InstalledDataState::VerificationRequired,
            celestial_navigation::InspectInstalledData(EclipseDataKind::De440s,
                                                       path));

  celestial_navigation::ForgetVerifiedDataFile(path);
  EXPECT_FALSE(
      wxFileExists(celestial_navigation::VerificationRecordPath(path)));
}

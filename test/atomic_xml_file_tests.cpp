#include <gtest/gtest.h>

#include <wx/filefn.h>
#include <wx/filename.h>

#include "AtomicXmlFile.h"
#include "tinyxml.h"

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    path = wxFileName::CreateTempFileName("celestial-xml-test-");
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(wxRemoveFile(path));
    EXPECT_TRUE(wxMkdir(path));
  }

  ~TemporaryDirectory() {
    if (!path.empty()) wxFileName::Rmdir(path, wxPATH_RMDIR_RECURSIVE);
  }

  wxString File(const wxString& name) const {
    return wxFileName(path, name).GetFullPath();
  }

  wxString path;
};

void BuildStableSightsDocument(TiXmlDocument* document, const char* body) {
  document->LinkEndChild(new TiXmlDeclaration("1.0", "utf-8", ""));
  TiXmlElement* root = new TiXmlElement("OpenCPNCelestialNavigation");
  root->SetAttribute("version", "2.8");
  root->SetAttribute("creator", "Opencpn Celestial Navigation plugin");
  document->LinkEndChild(root);

  TiXmlElement* clock = new TiXmlElement("ClockError");
  clock->SetAttribute("Seconds", 12);
  root->LinkEndChild(clock);

  TiXmlElement* sight = new TiXmlElement("Sight");
  sight->SetAttribute("Visible", 1);
  sight->SetAttribute("Type", 0);
  sight->SetAttribute("Body", body);
  sight->SetAttribute("Date", "2026-09-02");
  sight->SetAttribute("Time", "12:34:56");
  sight->SetDoubleAttribute("Measurement", 42.25);
  root->LinkEndChild(sight);
}

const TiXmlElement* LoadedSight(const wxString& path, TiXmlDocument* document) {
  if (!document->LoadFile(path.mb_str())) return nullptr;
  const TiXmlElement* root = document->RootElement();
  return root ? root->FirstChildElement("Sight") : nullptr;
}

}  // namespace

TEST(AtomicXmlFile, SavesAndLoadsTheExistingStableSchema) {
  TemporaryDirectory directory;
  const wxString path = directory.File("Sights.xml");
  TiXmlDocument document;
  BuildStableSightsDocument(&document, "Sun");

  wxString error;
  ASSERT_TRUE(
      celestial_navigation::SaveXmlDocumentAtomically(document, path, &error))
      << error;

  TiXmlDocument loaded;
  const TiXmlElement* sight = LoadedSight(path, &loaded);
  ASSERT_NE(nullptr, sight) << loaded.ErrorDesc();
  EXPECT_STREQ("OpenCPNCelestialNavigation", loaded.RootElement()->Value());
  EXPECT_STREQ("2.8", loaded.RootElement()->Attribute("version"));
  EXPECT_STREQ("Sun", sight->Attribute("Body"));
  EXPECT_STREQ("2026-09-02", sight->Attribute("Date"));
  EXPECT_STREQ("12:34:56", sight->Attribute("Time"));
}

TEST(AtomicXmlFile, FailedWriteLeavesThePreviousFileLoadableAndUnchanged) {
  TemporaryDirectory directory;
  const wxString path = directory.File("Sights.xml");
  TiXmlDocument original;
  BuildStableSightsDocument(&original, "Sun");
  ASSERT_TRUE(celestial_navigation::SaveXmlDocumentAtomically(original, path));

  wxString error;
  EXPECT_FALSE(celestial_navigation::ReplaceFileAtomically(
      path,
      [](wxTempFile& temporary) {
        const char partial[] = "<OpenCPNCelestialNavigation><Sight";
        EXPECT_TRUE(temporary.Write(partial, sizeof(partial) - 1));
        return false;
      },
      &error));
  EXPECT_FALSE(error.empty());

  TiXmlDocument loaded;
  const TiXmlElement* sight = LoadedSight(path, &loaded);
  ASSERT_NE(nullptr, sight) << loaded.ErrorDesc();
  EXPECT_STREQ("Sun", sight->Attribute("Body"));
}

TEST(AtomicXmlFile, FailedFirstWriteDoesNotPublishAPartialFile) {
  TemporaryDirectory directory;
  const wxString path = directory.File("Sights.xml");

  EXPECT_FALSE(celestial_navigation::ReplaceFileAtomically(
      path, [](wxTempFile& temporary) {
        return temporary.Write("partial", 7) && false;
      }));
  EXPECT_FALSE(wxFileExists(path));
}

TEST(AtomicXmlFile, CopiesValidatedInputWithoutExposingAPartialDestination) {
  TemporaryDirectory directory;
  const wxString source = directory.File("verified-download.bin");
  const wxString destination = directory.File("installed.bin");
  {
    wxFile file(source, wxFile::write);
    ASSERT_TRUE(file.IsOpened());
    ASSERT_TRUE(file.Write("verified astronomy data", 23));
  }
  {
    wxFile file(destination, wxFile::write);
    ASSERT_TRUE(file.IsOpened());
    ASSERT_TRUE(file.Write("old", 3));
  }

  wxString error;
  ASSERT_TRUE(
      celestial_navigation::CopyFileAtomically(source, destination, &error))
      << error;
  wxFile installed(destination);
  wxString contents;
  ASSERT_TRUE(installed.ReadAll(&contents, wxConvUTF8));
  EXPECT_EQ("verified astronomy data", contents);
}

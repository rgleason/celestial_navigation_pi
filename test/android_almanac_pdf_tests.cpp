// POBsoft (1985-2026): small-paper output must retain every reference row.
#include <gtest/gtest.h>
#include "AlmanacGenerator.h"
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <regex>

class AndroidAlmanacPdfWriter {
 public:
  static bool Write(const AlmanacDocument&, const AlmanacRequest&,
                    const wxString&, wxString*);
};

TEST(AndroidAlmanacPdf, SmallPaperAndBookletRetainFinalDenseTableRow) {
  AlmanacDocument document;
  AlmanacPage page;
  page.title = "Friday 21 June 2024 - lunar-distance opportunity";
  page.section = "Acceptance reference";
  AlmanacTable table;
  table.headings = {"Row", "Reference"};
  for (unsigned i = 0; i < 61; ++i)
    table.rows.push_back({wxString::Format("%u", i),
                         wxString::Format("REF-%03u", i)});
  page.tables.push_back(table);
  AlmanacTable second;
  second.headings = {"Reference"};
  for (unsigned i = 0; i < 4; ++i)
    second.rows.push_back({wxString::Format("SECOND-%03u", i)});
  page.tables.push_back(second);
  document.pages.push_back(page);
  for (const auto paper : {AlmanacPaper::A4, AlmanacPaper::Letter,
                           AlmanacPaper::A5}) {
    for (bool booklet : {false, true}) {
      AlmanacRequest request;
      request.paper = paper;
      request.booklet = booklet;
      wxString output;
      const bool keep = wxGetEnv("CELESTIAL_ANDROID_LAYOUT_PDF", &output) &&
                        paper == AlmanacPaper::A5 && booklet;
      if (!keep) output = wxFileName::CreateTempFileName("celestial-layout-");
      wxString error;
      ASSERT_TRUE(AndroidAlmanacPdfWriter::Write(document, request, output,
                                                &error)) << error;
      wxFile file(output);
      wxString bytes;
      ASSERT_TRUE(file.ReadAll(&bytes, wxConvISO8859_1));
      // This is the lost-data regression, rather than an assertion about the
      // implementation's scale or chosen line breaks.
      for (unsigned i = 0; i < 61; ++i)
        EXPECT_NE(wxNOT_FOUND, bytes.Find(wxString::Format("REF-%03u", i)));
      for (unsigned i = 0; i < 4; ++i)
        EXPECT_NE(wxNOT_FOUND, bytes.Find(wxString::Format("SECOND-%03u", i)));
      EXPECT_NE(wxNOT_FOUND, bytes.Find("%%EOF"));
      // Scientific notation is invalid in a PDF transformation matrix.
      EXPECT_FALSE(std::regex_search(bytes.ToStdString(),
          std::regex("[0-9]\\.[0-9]+[eE][+-][0-9]+")));
      file.Close();
      if (!keep) wxRemoveFile(output);
    }
  }
}

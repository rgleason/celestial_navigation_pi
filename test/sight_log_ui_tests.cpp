#include <gtest/gtest.h>

#include <wx/app.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/textctrl.h>

#include <cstdlib>

#include "CelestialNavigationDialog.h"
#include "HorizonEventDialog.h"
#include "SightDialog.h"
#include "mock_plugin_api.h"

#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

namespace {
wxString ReadFile(const wxString& path) {
  wxFFile file(path, "rb");
  wxString contents;
  EXPECT_TRUE(file.IsOpened());
  EXPECT_TRUE(file.ReadAll(&contents, wxConvUTF8));
  return contents;
}

void WriteFile(const wxString& path, const wxString& contents) {
  wxFFile file(path, "wb");
  ASSERT_TRUE(file.IsOpened());
  ASSERT_TRUE(file.Write(contents, wxConvUTF8));
}

wxListCtrl* SightList(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* list = dynamic_cast<wxListCtrl*>(child)) return list;
    if (auto* list = SightList(child)) return list;
  }
  return nullptr;
}

bool HasTextValue(wxWindow* root, const wxString& value) {
  for (auto* child : root->GetChildren()) {
    if (auto* field = dynamic_cast<wxTextCtrl*>(child))
      if (field->GetValue() == value) return true;
    if (HasTextValue(child, value)) return true;
  }
  return false;
}

wxNotebook* Notebook(wxWindow* root) {
  for (auto* child : root->GetChildren()) {
    if (auto* book = dynamic_cast<wxNotebook*>(child)) return book;
    if (auto* book = Notebook(child)) return book;
  }
  return nullptr;
}

wxString OldFile(const char* body, int clockCorrection = 0) {
  return wxString::Format(
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<OpenCPNCelestialNavigation version=\"2.8\">"
      "<ClockError Seconds=\"%d\"/>"
      "<Sight Visible=\"0\" Type=\"0\" Body=\"%s\" BodyLimb=\"0\" "
      "Date=\"2026-09-18\" Time=\"12:00:00\" Measurement=\"42\" "
      "ColourName=\"red\" Colour=\"RED\"/>"
      "</OpenCPNCelestialNavigation>", clockCorrection,
      wxString::FromUTF8(body).c_str());
}
}  // namespace

TEST(SightLogUi, RemarksAndManagerRoundTripOldAndNewXml) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Run alone with CELESTIAL_RUN_UI_TESTS=1 and a display";
  int argc = 1;
  char name[] = "celestial-sight-log-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  delete wxLog::SetActiveTarget(new wxLogStderr);
  const wxString root = wxFileName::CreateTempFileName("celestial-sight-log-");
  ASSERT_TRUE(wxRemoveFile(root));
  ASSERT_TRUE(wxFileName::Mkdir(root + "/plugins/celestial_navigation", 0777,
                                wxPATH_MKDIR_FULL));
  SetTestPrivateDataPath(root);
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  wxInitAllImageHandlers();
  const wxString active = root + "/plugins/celestial_navigation/Sights.xml";
  const wxString incoming = root + "/incoming.xml";
  const wxString backup = root + "/downloaded-backup.xml";
  const wxString invalid = root + "/invalid.xml";
  const wxString invalidDate = root + "/invalid-date.xml";
  WriteFile(active, OldFile("Sun"));
  WriteFile(incoming, OldFile("Moon"));
  WriteFile(invalid, "<not-a-sight-log/>");
  wxString badDate = OldFile("Mars");
  badDate.Replace("2026-09-18", "not-a-date");
  WriteFile(invalidDate, badDate);
  const wxString remark = wxString::FromUTF8("Cloud & spray \"N\" — café");
  {
    wxFrame host(nullptr, wxID_ANY, "Sight log test host");
    celestial_navigation_pi plugin(nullptr);
    PlugIn_Position_Fix_Ex boat{};
    boat.Lat = 42.0;
    boat.Lon = -70.0;
    plugin.SetPositionFixEx(boat);
    CelestialNavigationDialog main(&host, &plugin);
    ASSERT_EQ(1u, main.m_Sights.size());
    EXPECT_TRUE(main.m_Sights[0].m_Remarks.empty());
    auto* list = SightList(&main);
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(1, list->GetItemCount());
    ASSERT_GT(list->GetColumnCount(), 6);
    wxListItem remarksColumn;
    ASSERT_TRUE(list->GetColumn(6, remarksColumn));
    EXPECT_EQ("Remarks", remarksColumn.GetText());
    main.m_Sights[0].m_Remarks = remark;
    main.UpdateSights();
    EXPECT_EQ(remark, list->GetItemText(0, 6));
    EXPECT_TRUE(ReadFile(active).Contains("Cloud &amp; spray"));
    EXPECT_TRUE(ReadFile(active).Contains("&quot;N&quot;"));

    Sight ordinary = main.m_Sights[0];
    wxDateTime markedUtc;
    SightDialog sightEditor(&main, ordinary, 0, markedUtc,
                            SightDialog::Mode::Edit);
    EXPECT_TRUE(HasTextValue(&sightEditor, remark));
    auto* book = Notebook(&sightEditor);
    ASSERT_NE(nullptr, book);
    for (size_t page = 0; page < book->GetPageCount(); ++page)
      if (book->GetPageText(page) == "Parameters")
        book->SetSelection(page);
    sightEditor.Show();
    wxTheApp->Yield(true);
#ifdef __WXGTK3__
    const auto editorSize = sightEditor.GetSize();
    GtkAllocation editorAllocation{0, 0, editorSize.x, editorSize.y};
    gtk_widget_size_allocate(GTK_WIDGET(sightEditor.GetHandle()),
                             &editorAllocation);
    auto* editorSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                      editorSize.x, editorSize.y);
    auto* editorCr = cairo_create(editorSurface);
    gtk_widget_draw(GTK_WIDGET(sightEditor.GetHandle()), editorCr);
    EXPECT_EQ(cairo_surface_write_to_png(editorSurface,
                                        "/tmp/celestial-remarks-29.png"),
              CAIRO_STATUS_SUCCESS);
    cairo_destroy(editorCr);
    cairo_surface_destroy(editorSurface);
#endif
    sightEditor.Hide();

    Sight eventSight = ordinary;
    eventSight.m_Type = Sight::HORIZON;
    HorizonEventDialog eventEditor(&main, eventSight, 0, "test clock",
                                   HorizonEventDialog::Mode::Edit);
    EXPECT_TRUE(HasTextValue(&eventEditor, remark));

    wxString error;
    EXPECT_TRUE(main.BackupSightsTo(backup, &error)) << error;
    EXPECT_EQ(ReadFile(active), ReadFile(backup));
    wxString safety;
    EXPECT_TRUE(main.ImportSightsFile(backup, true, &safety, &error)) << error;
    ASSERT_EQ(1u, main.m_Sights.size());
    EXPECT_EQ(remark, main.m_Sights[0].m_Remarks);
    EXPECT_TRUE(main.ImportSightsFile(incoming, false, &safety, &error)) << error;
    ASSERT_EQ(2u, main.m_Sights.size());
    EXPECT_TRUE(wxFileExists(safety));
    EXPECT_TRUE(ReadFile(safety).Contains("Cloud &amp; spray"));
    const wxString beforeFailure = ReadFile(active);
    EXPECT_FALSE(main.ImportSightsFile(invalid, true, &safety, &error));
    EXPECT_EQ(beforeFailure, ReadFile(active));
    EXPECT_FALSE(main.ImportSightsFile(invalidDate, false, &safety, &error));
    EXPECT_EQ(beforeFailure, ReadFile(active));
    WriteFile(incoming, OldFile("Venus", 12));
    EXPECT_FALSE(main.ImportSightsFile(incoming, false, &safety, &error));
    EXPECT_EQ(beforeFailure, ReadFile(active));
    EXPECT_TRUE(main.ImportSightsFile(incoming, true, &safety, &error)) << error;
    ASSERT_EQ(1u, main.m_Sights.size());
    EXPECT_EQ("Venus", main.m_Sights[0].m_Body);
    EXPECT_EQ(12, main.GetClockCorrection());
    EXPECT_TRUE(wxFileExists(safety));
    main.Show();
    wxTheApp->Yield(true);
#ifdef __WXGTK3__
    const auto size = main.GetSize();
    GtkAllocation allocation{0, 0, size.x, size.y};
    gtk_widget_size_allocate(GTK_WIDGET(main.GetHandle()), &allocation);
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                size.x, size.y);
    auto* cr = cairo_create(surface);
    gtk_widget_draw(GTK_WIDGET(main.GetHandle()), cr);
    EXPECT_EQ(cairo_surface_write_to_png(surface,
                                        "/tmp/celestial-sight-log-29.png"),
              CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
#endif
    main.Hide();
  }
  wxString mixedLegacy = OldFile("Sun");
  mixedLegacy.Replace(
      "</OpenCPNCelestialNavigation>",
      "<Sight Visible=\"0\" Type=\"0\" Body=\"Mars\" "
      "Date=\"bad-date\" Time=\"12:00:00\"/>"
      "</OpenCPNCelestialNavigation>");
  WriteFile(active, mixedLegacy);
  {
    wxFrame host(nullptr, wxID_ANY, "Legacy sight log host");
    celestial_navigation_pi plugin(nullptr);
    CelestialNavigationDialog reopened(&host, &plugin);
    ASSERT_EQ(1u, reopened.m_Sights.size());
    EXPECT_EQ("Sun", reopened.m_Sights[0].m_Body);
    EXPECT_TRUE(reopened.m_Sights[0].m_Remarks.empty());
  }
  SetTestPrivateDataPath(wxString());
  SetTestPluginDataRoot(wxString());
  wxTheApp->OnExit();
  wxEntryCleanup();
}

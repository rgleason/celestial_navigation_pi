#include <gtest/gtest.h>

#include "HtmlHelp.h"
#include "CelestialNavigationUI.h"
#include "mock_plugin_api.h"
#include <wx/app.h>
#include <wx/filename.h>
#include <wx/modalhook.h>
#include <wx/html/htmlpars.h>
#include <wx/file.h>
#include <algorithm>
#include <cstdlib>
#ifdef __WXGTK3__
#include <gtk/gtk.h>

namespace {
void CaptureGuide(wxWindow* viewer, const wxString& path) {
  const auto size = viewer->GetSize();
  GtkAllocation allocation{0, 0, size.x, size.y};
  gtk_widget_size_allocate(GTK_WIDGET(viewer->GetHandle()), &allocation);
  auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
  auto* cr = cairo_create(surface);
  gtk_widget_draw(GTK_WIDGET(viewer->GetHandle()), cr);
  EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()), CAIRO_STATUS_SUCCESS);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}
}  // namespace
#endif

TEST(HtmlHelp, MissingExternalDocumentFailsSafely) {
  SetTestPluginDataRoot(wxFileName(__FILE__).GetPath() + "/..");
  EXPECT_FALSE(OpenBundledDocumentExternally(
      "document-that-is-deliberately-not-bundled.pdf"));
  EXPECT_FALSE(OpenBundledDocumentExternally(
      "document-that-is-deliberately-not-bundled.pdf", "How to Guide"));
}

TEST(HtmlHelp, PracticalGuideOfflineNavigationAndRendering) {
  if (!std::getenv("CELESTIAL_RUN_UI_TESTS"))
    GTEST_SKIP() << "Set CELESTIAL_RUN_UI_TESTS=1 on a GUI display";
  int argc = 1;
  char name[] = "celestial-html-guide-test";
  char* argv[] = {name, nullptr};
  wxApp::SetInstance(new wxApp);
  ASSERT_TRUE(wxEntryStart(argc, argv));
  ASSERT_TRUE(wxTheApp->CallOnInit());
  const wxString root = wxFileName(__FILE__).GetPath() + "/..";
  SetTestPluginDataRoot(root);
  wxInitAllImageHandlers();
  class GuideHook : public wxModalDialogHook {
  public:
    wxString root;
    bool entered = false;
    int Enter(wxDialog* dialog) override {
      auto* viewer = dynamic_cast<InformationDialog*>(dialog);
      if (!viewer) return wxID_CANCEL;
      entered = true;
      wxTheApp->CallAfter([this, viewer] {
        auto* html = viewer->m_htmlInformation;
        EXPECT_TRUE(html->ToText().Contains("Contents"));
#ifdef __WXGTK3__
        CaptureGuide(viewer, "/tmp/celnav-howto-contents.png");
#endif
        // Exercise every page in the actual wx reader. Missing images/links
        // are also rejected by the archive/HTML asset tests.
        for (int page = 1; page <= 66; ++page) {
          viewer->SetSize(wxSize(900, 760));
          viewer->Layout();
          EXPECT_TRUE(html->LoadPage(root + wxString::Format(
              "/data/practical-guide/page-%02d.html", page))) << page;
          EXPECT_TRUE(html->ToText().Contains("Next"));
          EXPECT_EQ(html->GetVirtualSize().x, html->GetClientSize().x) << page;
#ifdef __WXGTK3__
          if (std::getenv("CELESTIAL_GUIDE_CAPTURE_ALL")) {
            viewer->SetSize(wxSize(900, std::min(4500, html->GetVirtualSize().y + 80)));
            viewer->Layout();
            wxYield();
            CaptureGuide(viewer, wxString::Format("/tmp/celnav-howto-review-%02d.png", page));
          }
#endif
        }
        for (int page : {1, 4, 6, 9, 43, 60, 62}) {
          viewer->SetSize(wxSize(900, 760));
          EXPECT_TRUE(html->LoadPage(root + wxString::Format(
              "/data/practical-guide/page-%02d.html", page)));
          viewer->Layout();
          wxYield();
#ifdef __WXGTK3__
          const wxString path = wxString::Format("/tmp/celnav-howto-page-%02d.png", page);
          CaptureGuide(viewer, path);
#endif
        }
        // Follow actual relative links, including full-size and explicit return.
        EXPECT_TRUE(html->LoadPage(root + "/data/practical-guide/page-09.html"));
        html->OnLinkClicked(wxHtmlLinkInfo("page-09-figure-1.html"));
        EXPECT_TRUE(html->GetOpenedPage().EndsWith("page-09-figure-1.html"));
        EXPECT_TRUE(html->ToText().Contains("Full-size illustration"));
        html->OnLinkClicked(wxHtmlLinkInfo("page-09.html"));
        EXPECT_TRUE(html->GetOpenedPage().EndsWith("page-09.html"));
        html->OnLinkClicked(wxHtmlLinkInfo("../Practical_Guide.html"));
        EXPECT_TRUE(html->ToText().Contains("Contents"));
        html->OnLinkClicked(wxHtmlLinkInfo("Celestial_Navigation_Definitions.html"));
        EXPECT_TRUE(html->GetOpenedPage().EndsWith("Celestial_Navigation_Definitions.html"));
        EXPECT_TRUE(html->LoadPage(root + "/data/practical-guide/page-01.html"));
        html->OnLinkClicked(wxHtmlLinkInfo("page-32.html"));
        EXPECT_TRUE(html->ToText().Contains("Lunar distance topics"));
        for (int width : {600, 900, 1200}) {
          viewer->SetSize(wxSize(width, 760));
          viewer->Layout();
          wxYield();
          EXPECT_TRUE(html->LoadPage(root + "/data/practical-guide/page-04.html"));
          EXPECT_LE(html->GetVirtualSize().x, html->GetClientSize().x) << width;
        }
        EXPECT_TRUE(html->LoadPage(root + "/data/practical-guide/page-43.html"));
        EXPECT_TRUE(html->ToText().Contains(wxString::FromUTF8("\xc2\xb1") + "12 hours"));
        viewer->EndModal(wxID_OK);
      });
      return wxID_NONE;
    }
  } hook;
  hook.root = root;
  hook.Register();
  EXPECT_TRUE(ShowBundledHtmlHelp(nullptr, "Celestial Navigation How to Guide", "Practical_Guide.html"));
  EXPECT_TRUE(hook.entered);
  hook.Unregister();
  SetTestPluginDataRoot(wxString());
  wxTheApp->OnExit();
  wxEntryCleanup();
}

TEST(HtmlHelp, PracticalGuideDeclaresEncodingForOlderReaders) {
  wxFile file(wxFileName(__FILE__).GetPath() + "/../data/practical-guide/page-43.html");
  wxString markup;
  ASSERT_TRUE(file.ReadAll(&markup, wxConvUTF8));
  EXPECT_EQ(wxHtmlParser::ExtractCharsetInformation(markup), "utf-8");
  EXPECT_TRUE(markup.Contains("http-equiv=\"Content-Type\""));
  EXPECT_TRUE(markup.Contains("&#177;12 hours"));
}

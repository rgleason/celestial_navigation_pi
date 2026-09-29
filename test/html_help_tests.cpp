#include <gtest/gtest.h>

#include "HtmlHelp.h"
#include "CelestialNavigationUI.h"
#include "mock_plugin_api.h"
#include <wx/app.h>
#include <wx/filename.h>
#include <wx/modalhook.h>
#include <cstdlib>
#ifdef __WXGTK3__
#include <gtk/gtk.h>
#endif

TEST(HtmlHelp, MissingExternalDocumentFailsSafely) {
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
        // Exercise every page in the actual wx reader. Missing images/links
        // are also rejected by the archive/HTML asset tests.
        for (int page = 1; page <= 66; ++page) {
          EXPECT_TRUE(html->LoadPage(root + wxString::Format(
              "/data/practical-guide/page-%02d.html", page))) << page;
          EXPECT_TRUE(html->ToText().Contains("Next page") || page == 66);
        }
        for (int page : {9, 43, 60, 62}) {
          viewer->SetSize(wxSize(900, 760));
          EXPECT_TRUE(html->LoadPage(root + wxString::Format(
              "/data/practical-guide/page-%02d.html", page)));
          viewer->Layout();
          wxYield();
#ifdef __WXGTK3__
          const auto size = viewer->GetSize();
          GtkAllocation allocation{0, 0, size.x, size.y};
          gtk_widget_size_allocate(GTK_WIDGET(viewer->GetHandle()), &allocation);
          auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y);
          auto* cr = cairo_create(surface);
          gtk_widget_draw(GTK_WIDGET(viewer->GetHandle()), cr);
          const wxString path = wxString::Format("/tmp/celnav-howto-page-%02d.png", page);
          EXPECT_EQ(cairo_surface_write_to_png(surface, path.utf8_str()), CAIRO_STATUS_SUCCESS);
          cairo_destroy(cr);
          cairo_surface_destroy(surface);
#endif
        }
        // Follow actual relative links, including full-size and explicit return.
        EXPECT_TRUE(html->LoadPage(root + "/data/practical-guide/page-09.html"));
        html->OnLinkClicked(wxHtmlLinkInfo("page-09-figure-1.html"));
        EXPECT_TRUE(html->GetOpenedPage().EndsWith("page-09-figure-1.html"));
        EXPECT_TRUE(html->ToText().Contains("Full-resolution lossless image"));
        html->OnLinkClicked(wxHtmlLinkInfo("page-09.html"));
        EXPECT_TRUE(html->GetOpenedPage().EndsWith("page-09.html"));
        html->OnLinkClicked(wxHtmlLinkInfo("../Practical_Guide.html"));
        EXPECT_TRUE(html->ToText().Contains("Contents"));
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

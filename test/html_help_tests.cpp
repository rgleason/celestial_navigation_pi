#include <gtest/gtest.h>

#include "HtmlHelp.h"

TEST(HtmlHelp, MissingExternalDocumentFailsSafely) {
  EXPECT_FALSE(OpenBundledDocumentExternally(
      "document-that-is-deliberately-not-bundled.pdf"));
}

#include <gtest/gtest.h>

#include "Utf8Translation.h"

TEST(Utf8Translation, PreservesNavigationAndStatusSymbols) {
  const char* source = u8"° — ± ′ Δ σ ✓ ⚠ …";
  EXPECT_EQ(CelestialTranslateUtf8(source), wxString::FromUTF8(source));
}

#include <gtest/gtest.h>

#include "DialogGeometry.h"

TEST(DialogGeometry, PreservesUsableRectangle) {
  EXPECT_EQ(wxRect(120, 80, 800, 600),
            dialog_geometry::ClampToWorkArea(
                wxRect(120, 80, 800, 600), wxSize(500, 400),
                wxRect(0, 0, 1920, 1080)));
}

TEST(DialogGeometry, BringsOffscreenRectangleBackOntoDisplay) {
  EXPECT_EQ(wxRect(1120, 480, 800, 600),
            dialog_geometry::ClampToWorkArea(
                wxRect(3000, 2000, 800, 600), wxSize(500, 400),
                wxRect(0, 0, 1920, 1080)));
}

TEST(DialogGeometry, CapsOversizeRectangleToWorkArea) {
  EXPECT_EQ(wxRect(100, 50, 1280, 720),
            dialog_geometry::ClampToWorkArea(
                wxRect(-500, -300, 2000, 1200), wxSize(760, 540),
                wxRect(100, 50, 1280, 720)));
}

TEST(DialogGeometry, RespectsMinimumWhenSpaceAllows) {
  EXPECT_EQ(wxRect(10, 10, 600, 540),
            dialog_geometry::ClampToWorkArea(
                wxRect(10, 10, 200, 100), wxSize(600, 540),
                wxRect(0, 0, 1280, 720)));
}

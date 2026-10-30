#include <gtest/gtest.h>
#include <windows.h>

#include "window_geometry_logic.h"

namespace xue_hua_navite_window_manager {
namespace test {

TEST(SnapButtonHitTest, ActivatesOnlyWhenArmedReleaseStaysInside) {
  EXPECT_FALSE(ShouldActivateSnapButton(false, true, true));
  EXPECT_FALSE(ShouldActivateSnapButton(true, false, true));
  EXPECT_FALSE(ShouldActivateSnapButton(true, true, false));
  EXPECT_TRUE(ShouldActivateSnapButton(true, true, true));
}

TEST(SnapButtonHitTest, ReportsHtMaxButtonOnlyForCustomFrames) {
  const LogicalRect rect{900.0, 0.0, 46.0, 40.0};

  EXPECT_TRUE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 920.0, 10.0));
  EXPECT_TRUE(
      ShouldReportSnapButtonHit(true, false, "immersive", rect, 945.0, 39.0));
  // Contains() is half-open on the right and bottom edges.
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 946.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 920.0, 40.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "normal", rect, 920.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, true, "hidden", rect, 920.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(false, false, "hidden", rect, 920.0, 10.0));

  const LogicalRect empty{0.0, 0.0, 0.0, 40.0};
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", empty, 0.0, 10.0));
}

TEST(SizeClamp, SetSizeHonorsStoredMinimumAndMaximum) {
  // Repro: setMinimumSize(850, 560) then setSize(500, 400) must not stick.
  double width = 500.0;
  double height = 400.0;
  ClampDipSize(850.0, 560.0, 0.0, 0.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 850.0);
  EXPECT_DOUBLE_EQ(height, 560.0);

  width = 1400.0;
  height = 900.0;
  ClampDipSize(0.0, 0.0, 1050.0, 700.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 1050.0);
  EXPECT_DOUBLE_EQ(height, 700.0);

  width = 900.0;
  height = 640.0;
  ClampDipSize(850.0, 560.0, 1050.0, 700.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 900.0);
  EXPECT_DOUBLE_EQ(height, 640.0);

  // Unset limits (<= 0) do not move the requested size.
  width = 100.0;
  height = 80.0;
  ClampDipSize(0.0, 0.0, 0.0, 0.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 100.0);
  EXPECT_DOUBLE_EQ(height, 80.0);

  // Minimum wins when the stored maximum is below the minimum.
  EXPECT_DOUBLE_EQ(ClampDipAxis(50.0, 800.0, 100.0), 800.0);
  EXPECT_DOUBLE_EQ(ClampDipAxis(50.0, 600.0, 80.0), 600.0);
}

TEST(MaximizedGeometryGuard, RestoresOnlyWhenWindowedGeometryIsRequested) {
  // Non-geometry configure (e.g. title / preventClose only) keeps maximize/FS.
  EXPECT_FALSE(ShouldRestoreWindowBeforeGeometry(false, true, false, false));
  EXPECT_FALSE(ShouldRestoreWindowBeforeGeometry(false, false, true, false));
  EXPECT_FALSE(ShouldRestoreWindowBeforeGeometry(false, false, false, true));

  // Normal windowed state does not trigger redundant restore.
  EXPECT_FALSE(ShouldRestoreWindowBeforeGeometry(true, false, false, false));

  // Explicit geometry (size / position / center) restores from maximized,
  // minimized, or fullscreen before SetWindowPos runs.
  EXPECT_TRUE(ShouldRestoreWindowBeforeGeometry(true, true, false, false));
  EXPECT_TRUE(ShouldRestoreWindowBeforeGeometry(true, false, true, false));
  EXPECT_TRUE(ShouldRestoreWindowBeforeGeometry(true, false, false, true));
}

TEST(MaximizedGeometryGuard, ClampMaximizedClientRectPreventsNegativeCrop) {
  const RECT work_area{0, 0, 1920, 1040};

  // Standard maximized Win32 window overshoots work_area by resize border (-8):
  // clamped client rect matches work_area exactly.
  const RECT zoomed_outer{-8, -8, 1928, 1048};
  const RECT zoomed_client = ClampMaximizedClientRect(zoomed_outer, work_area);
  EXPECT_EQ(zoomed_client.left, 0);
  EXPECT_EQ(zoomed_client.top, 0);
  EXPECT_EQ(zoomed_client.right, 1920);
  EXPECT_EQ(zoomed_client.bottom, 1040);

  // Regression guard for Compact Immersive (980x660 centered at 470, 210):
  // client rect must never expand to (0, 0, 1920, 1040) and crop top-left UI.
  const RECT compact_outer{470, 210, 1450, 870};
  const RECT compact_client =
      ClampMaximizedClientRect(compact_outer, work_area);
  EXPECT_EQ(compact_client.left, 470);
  EXPECT_EQ(compact_client.top, 210);
  EXPECT_EQ(compact_client.right, 1450);
  EXPECT_EQ(compact_client.bottom, 870);
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

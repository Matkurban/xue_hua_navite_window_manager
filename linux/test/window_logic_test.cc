#include <gtest/gtest.h>

#include "window_logic.h"

namespace xue_hua_navite_window_manager {
namespace test {

TEST(XueHuaWindowLogic, CenteredOriginUsesMonitorWorkArea) {
  const XueHuaCenteredOrigin origin =
      xue_hua_compute_centered_origin(0, 0, 1920, 1080, 800, 600);
  EXPECT_EQ(origin.x, 560);
  EXPECT_EQ(origin.y, 240);

  const XueHuaCenteredOrigin offset =
      xue_hua_compute_centered_origin(1920, 100, 1280, 800, 400, 200);
  EXPECT_EQ(offset.x, 1920 + (1280 - 400) / 2);
  EXPECT_EQ(offset.y, 100 + (800 - 200) / 2);

  // Integer division matches the Windows work-area path for odd sizes.
  const XueHuaCenteredOrigin odd =
      xue_hua_compute_centered_origin(0, 0, 100, 100, 41, 21);
  EXPECT_EQ(odd.x, 29);
  EXPECT_EQ(odd.y, 39);

  const XueHuaCenteredOrigin overflow =
      xue_hua_compute_centered_origin(10, 20, 100, 80, 180, 90);
  EXPECT_EQ(overflow.x, 10 + (100 - 180) / 2);
  EXPECT_EQ(overflow.y, 20 + (80 - 90) / 2);
}

TEST(XueHuaWindowLogic, CloseInterceptRequiresPreventAndNotForce) {
  EXPECT_TRUE(xue_hua_should_intercept_close(true, false));
  EXPECT_FALSE(xue_hua_should_intercept_close(true, true));
  EXPECT_FALSE(xue_hua_should_intercept_close(false, false));
  EXPECT_FALSE(xue_hua_should_intercept_close(false, true));
}

TEST(XueHuaWindowLogic, MoveDragPlanPrefersButtonSerial) {
  const XueHuaMoveDragPlan captured =
      xue_hua_plan_move_drag(true, 1234u, 0u, true);
  EXPECT_TRUE(captured.attempt);
  EXPECT_EQ(captured.timestamp, 1234u);

  const XueHuaMoveDragPlan current =
      xue_hua_plan_move_drag(true, 0u, 55u, true);
  EXPECT_TRUE(current.attempt);
  EXPECT_EQ(current.timestamp, 55u);

  // Wayland rejects GDK_CURRENT_TIME. Skip instead of calling begin_move_drag.
  const XueHuaMoveDragPlan wayland_missing =
      xue_hua_plan_move_drag(false, 0u, 0u, true);
  EXPECT_FALSE(wayland_missing.attempt);

  // X11 may accept CurrentTime when the button-press time was not captured.
  const XueHuaMoveDragPlan x11_fallback =
      xue_hua_plan_move_drag(false, 0u, 0u, false);
  EXPECT_TRUE(x11_fallback.attempt);
  EXPECT_EQ(x11_fallback.timestamp, 0u);
}

TEST(XueHuaWindowLogic, InteractiveKindParsesMoveAndResizeWires) {
  int kind = -1;
  EXPECT_TRUE(xue_hua_interactive_kind_from_wire("move", &kind));
  EXPECT_EQ(kind, XUE_HUA_INTERACTIVE_MOVE);
  EXPECT_TRUE(xue_hua_interactive_kind_from_wire("bottomRight", &kind));
  EXPECT_EQ(kind, XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST);
  EXPECT_TRUE(xue_hua_interactive_kind_from_wire("top", &kind));
  EXPECT_EQ(kind, XUE_HUA_INTERACTIVE_RESIZE_NORTH);
  EXPECT_FALSE(xue_hua_interactive_kind_from_wire("caption", &kind));
  EXPECT_FALSE(xue_hua_interactive_kind_from_wire(nullptr, &kind));
}

TEST(XueHuaWindowLogic, ViewLocalUsesWindowOriginsNotScale) {
  // Same window: Flutter logical pixels match the GDK event coordinate.
  EXPECT_DOUBLE_EQ(xue_hua_view_local(12.5, 100, 100), 12.5);
  // Child GdkWindow: shift by the origin delta. Do not divide by scale.
  EXPECT_DOUBLE_EQ(xue_hua_view_local(4.0, 80, 10), 74.0);
  EXPECT_DOUBLE_EQ(xue_hua_view_local(9.0, 10, 40), -21.0);
}

TEST(XueHuaWindowLogic, WaylandPressStartsMoveInsideDragRegion) {
  const XueHuaInteractiveRegion regions[] = {
      {1, XUE_HUA_INTERACTIVE_MOVE, 0.0, 0.0, 800.0, 40.0},
  };
  const XueHuaPressPlan hit = xue_hua_plan_interactive_press(
      true, true, false, 20.0, 10.0, regions, 1);
  EXPECT_EQ(hit.action, XUE_HUA_PRESS_MOVE);
  EXPECT_EQ(hit.kind, XUE_HUA_INTERACTIVE_MOVE);

  // Half-open edge: x == right is outside, so a neighboring widget wins.
  const XueHuaPressPlan edge = xue_hua_plan_interactive_press(
      true, true, false, 800.0, 10.0, regions, 1);
  EXPECT_EQ(edge.action, XUE_HUA_PRESS_IGNORE);

  const XueHuaPressPlan outside = xue_hua_plan_interactive_press(
      true, true, false, 20.0, 80.0, regions, 1);
  EXPECT_EQ(outside.action, XUE_HUA_PRESS_IGNORE);
}

TEST(XueHuaWindowLogic, X11PressDoesNotStealAsyncDrag) {
  const XueHuaInteractiveRegion regions[] = {
      {1, XUE_HUA_INTERACTIVE_MOVE, 0.0, 0.0, 800.0, 40.0},
      {2, XUE_HUA_INTERACTIVE_RESIZE_EAST, 790.0, 0.0, 10.0, 600.0},
  };
  const XueHuaPressPlan x11 = xue_hua_plan_interactive_press(
      false, true, false, 20.0, 10.0, regions, 2);
  EXPECT_EQ(x11.action, XUE_HUA_PRESS_IGNORE);

  const XueHuaPressPlan secondary = xue_hua_plan_interactive_press(
      true, false, false, 20.0, 10.0, regions, 1);
  EXPECT_EQ(secondary.action, XUE_HUA_PRESS_IGNORE);

  // Double-click stays with Flutter. The press hook must not start a move.
  const XueHuaPressPlan dbl = xue_hua_plan_interactive_press(
      true, true, true, 20.0, 10.0, regions, 1);
  EXPECT_EQ(dbl.action, XUE_HUA_PRESS_IGNORE);
}

TEST(XueHuaWindowLogic, ResizeRegionBeatsLargerMoveRegion) {
  const XueHuaInteractiveRegion regions[] = {
      {1, XUE_HUA_INTERACTIVE_MOVE, 0.0, 0.0, 400.0, 400.0},
      {2, XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST, 380.0, 380.0, 20.0, 20.0},
      {3, XUE_HUA_INTERACTIVE_MOVE, 0.0, 0.0, 30.0, 16.0},
  };
  const XueHuaPressPlan corner = xue_hua_plan_interactive_press(
      true, true, false, 390.0, 390.0, regions, 3);
  EXPECT_EQ(corner.action, XUE_HUA_PRESS_RESIZE);
  EXPECT_EQ(corner.kind, XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST);

  // Two move rects: the smaller one wins.
  const XueHuaPressPlan title = xue_hua_plan_interactive_press(
      true, true, false, 10.0, 8.0, regions, 3);
  EXPECT_EQ(title.action, XUE_HUA_PRESS_MOVE);

  const XueHuaInteractiveRegion empty[] = {
      {4, XUE_HUA_INTERACTIVE_MOVE, 0.0, 0.0, 0.0, 40.0},
  };
  const XueHuaPressPlan zero = xue_hua_plan_interactive_press(
      true, true, false, 0.0, 0.0, empty, 1);
  EXPECT_EQ(zero.action, XUE_HUA_PRESS_IGNORE);

  const XueHuaPressPlan none =
      xue_hua_plan_interactive_press(true, true, false, 1.0, 1.0, nullptr, 0);
  EXPECT_EQ(none.action, XUE_HUA_PRESS_IGNORE);
}

TEST(XueHuaWindowLogic, ClampSizeHonorsStoredMinimumAndMaximum) {
  // Repro: setMinimumSize(850, 560) then setSize(500, 400) must not stick.
  const XueHuaClampedSize below =
      xue_hua_clamp_window_size(500, 400, 850, 560, 0, 0);
  EXPECT_EQ(below.width, 850);
  EXPECT_EQ(below.height, 560);

  const XueHuaClampedSize above =
      xue_hua_clamp_window_size(1400, 900, 0, 0, 1050, 700);
  EXPECT_EQ(above.width, 1050);
  EXPECT_EQ(above.height, 700);

  const XueHuaClampedSize inside =
      xue_hua_clamp_window_size(900, 640, 850, 560, 1050, 700);
  EXPECT_EQ(inside.width, 900);
  EXPECT_EQ(inside.height, 640);

  const XueHuaClampedSize unbounded =
      xue_hua_clamp_window_size(100, 80, 0, 0, 0, 0);
  EXPECT_EQ(unbounded.width, 100);
  EXPECT_EQ(unbounded.height, 80);

  // Minimum wins when the stored maximum is below the minimum.
  const XueHuaClampedSize conflict =
      xue_hua_clamp_window_size(50, 40, 800, 600, 100, 80);
  EXPECT_EQ(conflict.width, 800);
  EXPECT_EQ(conflict.height, 600);
}

TEST(XueHuaWindowLogic, GeometryRestorePlanClearsMaximizedAndFullScreen) {
  const XueHuaGeometryRestorePlan no_geom =
      xue_hua_plan_geometry_restore(false, true, true, true);
  EXPECT_FALSE(no_geom.unfullscreen);
  EXPECT_FALSE(no_geom.unmaximize);
  EXPECT_FALSE(no_geom.deiconify);

  const XueHuaGeometryRestorePlan normal_win =
      xue_hua_plan_geometry_restore(true, false, false, false);
  EXPECT_FALSE(normal_win.unfullscreen);
  EXPECT_FALSE(normal_win.unmaximize);
  EXPECT_FALSE(normal_win.deiconify);

  const XueHuaGeometryRestorePlan maximized_win =
      xue_hua_plan_geometry_restore(true, false, true, false);
  EXPECT_FALSE(maximized_win.unfullscreen);
  EXPECT_TRUE(maximized_win.unmaximize);
  EXPECT_FALSE(maximized_win.deiconify);

  const XueHuaGeometryRestorePlan all_modal =
      xue_hua_plan_geometry_restore(true, true, true, true);
  EXPECT_TRUE(all_modal.unfullscreen);
  EXPECT_TRUE(all_modal.unmaximize);
  EXPECT_TRUE(all_modal.deiconify);
}

TEST(XueHuaWindowLogic, CenterPlanMatchesBackendLimits) {
  EXPECT_EQ(xue_hua_plan_center(false, false), XUE_HUA_CENTER_MAP_HINT);
  EXPECT_EQ(xue_hua_plan_center(false, true), XUE_HUA_CENTER_MAP_HINT);
  EXPECT_EQ(xue_hua_plan_center(true, false), XUE_HUA_CENTER_MOVE);
  EXPECT_EQ(xue_hua_plan_center(true, true), XUE_HUA_CENTER_WAYLAND_IGNORED);

  EXPECT_TRUE(
      xue_hua_center_position_applied(XUE_HUA_CENTER_MAP_HINT, false));
  EXPECT_TRUE(xue_hua_center_position_applied(XUE_HUA_CENTER_MOVE, false));
  // The unmapped hint is still issued on Wayland, but it is not a promise.
  EXPECT_FALSE(xue_hua_center_position_applied(XUE_HUA_CENTER_MAP_HINT, true));
  EXPECT_FALSE(
      xue_hua_center_position_applied(XUE_HUA_CENTER_WAYLAND_IGNORED, true));

  EXPECT_TRUE(xue_hua_position_is_authoritative(false));
  EXPECT_FALSE(xue_hua_position_is_authoritative(true));
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

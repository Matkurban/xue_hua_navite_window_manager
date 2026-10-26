#include <flutter_linux/flutter_linux.h>
#include <gtest/gtest.h>

#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"
#include "xue_hua_navite_window_manager_plugin_private.h"
#include "xue_hua_window_logic.h"

namespace xue_hua_navite_window_manager {
namespace test {

TEST(XueHuaNaviteWindowManagerPlugin, EnsureInitializedSucceeds) {
  g_autoptr(FlMethodResponse) response =
      xue_hua_navite_window_manager_handle_ensure_initialized();
  ASSERT_NE(response, nullptr);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(response));
}

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

// Mapping a GtkWindow, detaching Flutter's delete-event handler, and proving
// gtk_window_begin_move_drag against X11/Wayland all require a real display
// and a running FlView. Those paths call the helpers tested above.

}  // namespace test
}  // namespace xue_hua_navite_window_manager

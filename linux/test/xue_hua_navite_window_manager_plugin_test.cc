#include <flutter_linux/flutter_linux.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"
#include "xue_hua_navite_window_manager_plugin_private.h"

namespace xue_hua_navite_window_manager {
namespace test {

TEST(XueHuaNaviteWindowManagerPlugin, EnsureInitializedSucceeds) {
  g_autoptr(FlMethodResponse) response =
      xue_hua_navite_window_manager_handle_ensure_initialized();
  ASSERT_NE(response, nullptr);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(response));
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

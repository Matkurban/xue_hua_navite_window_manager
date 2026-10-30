#include <flutter_linux/flutter_linux.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>

#include <xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h>
#include "xue_hua_navite_window_manager_plugin_private.h"

namespace xue_hua_navite_window_manager {
namespace test {

TEST(XueHuaNaviteWindowManagerPlugin, EnsureInitializedSucceeds) {
  g_autoptr(FlMethodResponse) response =
      xue_hua_navite_window_manager_handle_ensure_initialized();
  ASSERT_NE(response, nullptr);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(response));
}

TEST(LaunchAtStartupLinux, EndToEndAutostartLifecycleInTempDirectory) {
  g_autoptr(GError) tmp_err = nullptr;
  g_autofree gchar* temp_dir =
      g_dir_make_tmp("xue_hua_autostart_XXXXXX", &tmp_err);
  ASSERT_NE(temp_dir, nullptr);
  ASSERT_EQ(tmp_err, nullptr);

  g_autoptr(FlValue) query_args = fl_value_new_map();
  fl_value_set_string_take(query_args, "identifier",
                           fl_value_new_string("com.xuehua.unit_test"));
  fl_value_set_string_take(query_args, "appName",
                           fl_value_new_string("XueHua Unit Test"));
  fl_value_set_string_take(query_args, "executablePath",
                           fl_value_new_string("/usr/bin/xuehua_unit"));

  // 1. Initial state is disabled.
  g_autoptr(FlMethodResponse) initial_resp =
      xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
          query_args, temp_dir);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(initial_resp));
  FlValue* initial_map = fl_method_success_response_get_result(
      FL_METHOD_SUCCESS_RESPONSE(initial_resp));
  EXPECT_FALSE(fl_value_get_bool(fl_value_lookup_string(initial_map, "isEnabled")));
  EXPECT_STREQ(fl_value_get_string(fl_value_lookup_string(initial_map, "status")),
               "disabled");
  EXPECT_STREQ(fl_value_get_string(fl_value_lookup_string(initial_map, "backend")),
               "linuxXdgAutostart");

  // 2. Enable writes .desktop file and reports enabled.
  g_autoptr(FlValue) enable_args = fl_value_new_map();
  fl_value_set_string_take(enable_args, "enabled", fl_value_new_bool(TRUE));
  fl_value_set_string_take(enable_args, "identifier",
                           fl_value_new_string("com.xuehua.unit_test"));
  fl_value_set_string_take(enable_args, "executablePath",
                           fl_value_new_string("/usr/bin/xuehua_unit"));
  g_autoptr(FlMethodResponse) enable_resp =
      xue_hua_navite_window_manager_handle_set_launch_at_startup(enable_args,
                                                                 temp_dir);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(enable_resp));
  FlValue* enable_map = fl_method_success_response_get_result(
      FL_METHOD_SUCCESS_RESPONSE(enable_resp));
  EXPECT_TRUE(fl_value_get_bool(fl_value_lookup_string(enable_map, "isEnabled")));
  EXPECT_STREQ(fl_value_get_string(fl_value_lookup_string(enable_map, "status")),
               "enabled");

  g_autofree gchar* desktop_file =
      g_build_filename(temp_dir, "com.xuehua.unit_test.desktop", nullptr);
  EXPECT_TRUE(g_file_test(desktop_file, G_FILE_TEST_IS_REGULAR));

  // 3. Simulate desktop environment disabling via Hidden=true -> disabledByUser.
  const gchar* hidden_desktop =
      "[Desktop Entry]\nType=Application\nName=Test\nExec=/usr/bin/xuehua_unit\nHidden=true\n";
  ASSERT_TRUE(g_file_set_contents(desktop_file, hidden_desktop, -1, nullptr));
  g_autoptr(FlMethodResponse) user_off_resp =
      xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
          query_args, temp_dir);
  FlValue* user_off_map = fl_method_success_response_get_result(
      FL_METHOD_SUCCESS_RESPONSE(user_off_resp));
  EXPECT_FALSE(fl_value_get_bool(fl_value_lookup_string(user_off_map, "isEnabled")));
  EXPECT_STREQ(fl_value_get_string(fl_value_lookup_string(user_off_map, "status")),
               "disabledByUser");

  // 4. Disable removes .desktop file cleanly.
  g_autoptr(FlValue) disable_args = fl_value_new_map();
  fl_value_set_string_take(disable_args, "enabled", fl_value_new_bool(FALSE));
  fl_value_set_string_take(disable_args, "identifier",
                           fl_value_new_string("com.xuehua.unit_test"));
  fl_value_set_string_take(disable_args, "executablePath",
                           fl_value_new_string("/usr/bin/xuehua_unit"));
  g_autoptr(FlMethodResponse) disable_resp =
      xue_hua_navite_window_manager_handle_set_launch_at_startup(disable_args,
                                                                 temp_dir);
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(disable_resp));
  EXPECT_FALSE(g_file_test(desktop_file, G_FILE_TEST_EXISTS));

  // 5. openLaunchAtStartupSettings returns false on Linux.
  g_autoptr(FlMethodResponse) settings_resp =
      xue_hua_navite_window_manager_handle_open_launch_at_startup_settings();
  ASSERT_TRUE(FL_IS_METHOD_SUCCESS_RESPONSE(settings_resp));
  EXPECT_FALSE(fl_value_get_bool(fl_method_success_response_get_result(
      FL_METHOD_SUCCESS_RESPONSE(settings_resp))));

  g_rmdir(temp_dir);
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

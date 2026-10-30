#include <flutter_linux/flutter_linux.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

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

namespace {

std::string unescape_percent_codes(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '%' && i + 1 < raw.size() && raw[i + 1] == '%') {
      out.push_back('%');
      ++i;
    } else {
      out.push_back(raw[i]);
    }
  }
  return out;
}

}  // namespace

TEST(LaunchAtStartupLinux, SanitizeIdAndTextPreventTraversalAndInjection) {
  EXPECT_EQ(xue_hua_sanitize_startup_id("  com.xuehua.studio.desktop "),
            "com.xuehua.studio");
  EXPECT_EQ(xue_hua_sanitize_startup_id("../evil/path"), "_evil_path");
  EXPECT_EQ(xue_hua_sanitize_startup_id("..."), "flutter_app");

  const std::string injected =
      xue_hua_sanitize_desktop_text("XueHua\nExec=/bin/sh\r\n");
  EXPECT_EQ(injected.find('\n'), std::string::npos);
  EXPECT_EQ(injected.find('\r'), std::string::npos);
  EXPECT_EQ(injected, "XueHua Exec=/bin/sh");
}

TEST(LaunchAtStartupLinux, DesktopEntryExecRoundTripsGKeyFileAndShellParse) {
  const std::string program = "/opt/XueHua Studio/xue_hua_app";
  std::vector<std::string> args;
  args.push_back("--minimized");
  args.push_back("");
  args.push_back("hello world");
  args.push_back("100% zoom");
  args.push_back("--expr=$HOME/a\\b\"quoted\"`tick`");

  const std::string content =
      xue_hua_build_autostart_desktop_entry("XueHua\\Studio", program, args);

  g_autoptr(GKeyFile) key_file = g_key_file_new();
  g_autoptr(GError) load_err = nullptr;
  ASSERT_TRUE(g_key_file_load_from_data(key_file, content.c_str(),
                                        content.size(), G_KEY_FILE_NONE,
                                        &load_err));
  ASSERT_EQ(load_err, nullptr);

  g_autofree gchar* parsed_name = g_key_file_get_string(
      key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, nullptr);
  ASSERT_NE(parsed_name, nullptr);
  EXPECT_STREQ(parsed_name, "XueHua\\Studio");

  g_autofree gchar* parsed_exec = g_key_file_get_string(
      key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC,
      &load_err);
  ASSERT_EQ(load_err, nullptr);
  ASSERT_NE(parsed_exec, nullptr);

  const std::string expanded_exec =
      unescape_percent_codes(std::string(parsed_exec));
  gint argc = 0;
  g_auto(GStrv) argv = nullptr;
  g_autoptr(GError) shell_err = nullptr;
  ASSERT_TRUE(
      g_shell_parse_argv(expanded_exec.c_str(), &argc, &argv, &shell_err));
  ASSERT_EQ(shell_err, nullptr);
  ASSERT_EQ(argc, static_cast<gint>(1 + args.size()));
  EXPECT_EQ(std::string(argv[0]), program);
  for (std::size_t i = 0; i < args.size(); ++i) {
    EXPECT_EQ(std::string(argv[i + 1]), args[i]);
  }
}

TEST(LaunchAtStartupLinux, CommandResolutionHonorsAppImageAndFlatpak) {
  std::vector<std::string> cli_args;
  cli_args.push_back("--silent");

  const XueHuaLinuxStartupCommand explicit_cmd =
      xue_hua_resolve_linux_startup_command(
          "/custom/bin/app", cli_args, "/home/u/App.AppImage", "com.xuehua.App",
          "/proc/exe");
  EXPECT_EQ(explicit_cmd.program, "/custom/bin/app");
  EXPECT_EQ(explicit_cmd.arguments.size(), 1u);

  const XueHuaLinuxStartupCommand appimage_cmd =
      xue_hua_resolve_linux_startup_command(
          "", cli_args, "/home/u/Applications/XueHua.AppImage", "",
          "/tmp/.mount_XueHua/usr/bin/app");
  EXPECT_EQ(appimage_cmd.program, "/home/u/Applications/XueHua.AppImage");
  EXPECT_EQ(appimage_cmd.reported_executable_path,
            "/home/u/Applications/XueHua.AppImage");

  const XueHuaLinuxStartupCommand flatpak_cmd =
      xue_hua_resolve_linux_startup_command("", cli_args, "",
                                            "com.xuehua.Studio", "/app/bin/s");
  EXPECT_EQ(flatpak_cmd.program, "flatpak");
  ASSERT_EQ(flatpak_cmd.arguments.size(), 3u);
  EXPECT_EQ(flatpak_cmd.arguments[0], "run");
  EXPECT_EQ(flatpak_cmd.arguments[1], "com.xuehua.Studio");
  EXPECT_EQ(flatpak_cmd.arguments[2], "--silent");
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

// Mapping a GtkWindow, detaching Flutter's delete-event handler, and proving
// gtk_window_begin_move_drag against X11/Wayland all require a real display
// and a running FlView. Those paths call the helpers tested above.

}  // namespace test
}  // namespace xue_hua_navite_window_manager

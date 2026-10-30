#include <glib.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "launch_at_startup_logic.h"

namespace xue_hua_navite_window_manager {
namespace test {

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

}  // namespace test
}  // namespace xue_hua_navite_window_manager

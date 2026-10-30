#include "xue_hua_navite_window_manager_plugin_private.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <cstring>
#include <string>
#include <vector>

#include "fl_value_utils.h"
#include "launch_at_startup_logic.h"

FlMethodResponse* xue_hua_navite_window_manager_handle_ensure_initialized() {
  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

static std::string resolve_linux_autostart_dir(const gchar* override_dir) {
  if (override_dir != nullptr && override_dir[0] != '\0') {
    return std::string(override_dir);
  }
  const gchar* xdg_env = g_getenv("XDG_CONFIG_HOME");
  if (xdg_env != nullptr && xdg_env[0] != '\0' && g_path_is_absolute(xdg_env)) {
    g_autofree gchar* built = g_build_filename(xdg_env, "autostart", nullptr);
    return built != nullptr ? std::string(built) : std::string();
  }
  const gchar* config_dir = g_get_user_config_dir();
  if (config_dir != nullptr && config_dir[0] != '\0') {
    g_autofree gchar* built =
        g_build_filename(config_dir, "autostart", nullptr);
    return built != nullptr ? std::string(built) : std::string();
  }
  return std::string();
}

static std::string read_proc_self_exe() {
  g_autoptr(GError) err = nullptr;
  g_autofree gchar* target = g_file_read_link("/proc/self/exe", &err);
  if (target == nullptr) {
    return std::string();
  }
  return std::string(target);
}

static std::string extract_linux_exe_basename(const std::string& path) {
  if (path.empty()) {
    return std::string("flutter_app");
  }
  g_autofree gchar* base = g_path_get_basename(path.c_str());
  if (base == nullptr || std::strcmp(base, ".") == 0 ||
      std::strcmp(base, "/") == 0) {
    return std::string("flutter_app");
  }
  return std::string(base);
}

struct LinuxResolvedStartupSpec {
  std::string autostart_dir;
  std::string identifier;
  std::string app_name;
  XueHuaLinuxStartupCommand command;
};

static LinuxResolvedStartupSpec resolve_linux_startup_spec(
    FlValue* args,
    const gchar* autostart_dir_override) {
  LinuxResolvedStartupSpec spec;
  spec.autostart_dir = resolve_linux_autostart_dir(autostart_dir_override);

  const std::string configured_exe =
      xue_hua_get_string_arg_or_empty(args, "executablePath");
  const std::vector<std::string> configured_args =
      xue_hua_get_string_list_arg(args, "arguments");
  const gchar* appimage_raw = g_getenv("APPIMAGE");
  const gchar* flatpak_raw = g_getenv("FLATPAK_ID");
  const std::string proc_exe = read_proc_self_exe();

  spec.command = xue_hua_resolve_linux_startup_command(
      configured_exe, configured_args,
      appimage_raw != nullptr ? std::string(appimage_raw) : std::string(),
      flatpak_raw != nullptr ? std::string(flatpak_raw) : std::string(),
      proc_exe);

  std::string raw_id = xue_hua_get_string_arg_or_empty(args, "identifier");
  const std::string raw_app_name =
      xue_hua_get_string_arg_or_empty(args, "appName");
  if (raw_id.empty()) {
    raw_id = raw_app_name;
  }
  if (raw_id.empty()) {
    const gchar* prgname = g_get_prgname();
    if (prgname != nullptr && prgname[0] != '\0') {
      raw_id = std::string(prgname);
    }
  }
  if (raw_id.empty()) {
    raw_id = extract_linux_exe_basename(
        !spec.command.reported_executable_path.empty()
            ? spec.command.reported_executable_path
            : proc_exe);
  }
  spec.identifier = xue_hua_sanitize_startup_id(raw_id);

  if (!raw_app_name.empty()) {
    spec.app_name = raw_app_name;
  } else {
    const gchar* g_app_name = g_get_application_name();
    if (g_app_name != nullptr && g_app_name[0] != '\0') {
      spec.app_name = std::string(g_app_name);
    } else {
      spec.app_name = spec.identifier;
    }
  }
  return spec;
}

static FlValue* build_linux_launch_at_startup_state_map(
    const LinuxResolvedStartupSpec& spec) {
  FlValue* map = fl_value_new_map();
  if (spec.autostart_dir.empty() || spec.command.program.empty()) {
    fl_value_set_string_take(map, "isSupported", fl_value_new_bool(FALSE));
    fl_value_set_string_take(map, "isEnabled", fl_value_new_bool(FALSE));
    fl_value_set_string_take(map, "status", fl_value_new_string("unsupported"));
    fl_value_set_string_take(map, "backend", fl_value_new_string("none"));
    fl_value_set_string_take(map, "identifier",
                             fl_value_new_string(spec.identifier.c_str()));
    fl_value_set_string_take(map, "executablePath", fl_value_new_null());
    fl_value_set_string_take(map, "canOpenSettings", fl_value_new_bool(FALSE));
    return map;
  }

  const std::string filename = spec.identifier + ".desktop";
  g_autofree gchar* desktop_path = g_build_filename(
      spec.autostart_dir.c_str(), filename.c_str(), nullptr);

  const bool file_exists =
      desktop_path != nullptr &&
      g_file_test(desktop_path, G_FILE_TEST_IS_REGULAR) != FALSE;
  bool valid_group = false;
  bool has_exec = false;
  bool hidden = false;
  bool has_gnome_enabled = false;
  bool gnome_enabled = true;

  if (file_exists) {
    g_autoptr(GKeyFile) key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, desktop_path, G_KEY_FILE_NONE,
                                  nullptr) &&
        g_key_file_has_group(key_file, G_KEY_FILE_DESKTOP_GROUP)) {
      valid_group = true;
      g_autofree gchar* exec_val = g_key_file_get_string(
          key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC,
          nullptr);
      has_exec = (exec_val != nullptr && exec_val[0] != '\0');

      g_autoptr(GError) hidden_err = nullptr;
      const gboolean hidden_val = g_key_file_get_boolean(
          key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_HIDDEN,
          &hidden_err);
      if (hidden_err == nullptr) {
        hidden = (hidden_val != FALSE);
      }

      g_autoptr(GError) gnome_err = nullptr;
      const gboolean gnome_val = g_key_file_get_boolean(
          key_file, G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-Autostart-enabled",
          &gnome_err);
      if (gnome_err == nullptr) {
        has_gnome_enabled = true;
        gnome_enabled = (gnome_val != FALSE);
      }
    }
  }

  const XueHuaLinuxStartupStatus status_enum =
      xue_hua_evaluate_desktop_entry_state(file_exists, valid_group, has_exec,
                                           hidden, has_gnome_enabled,
                                           gnome_enabled);
  const gchar* status_wire = "disabled";
  gboolean is_enabled = FALSE;
  if (status_enum == XUE_HUA_LINUX_STARTUP_ENABLED) {
    status_wire = "enabled";
    is_enabled = TRUE;
  } else if (status_enum == XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER) {
    status_wire = "disabledByUser";
    is_enabled = FALSE;
  }

  fl_value_set_string_take(map, "isSupported", fl_value_new_bool(TRUE));
  fl_value_set_string_take(map, "isEnabled", fl_value_new_bool(is_enabled));
  fl_value_set_string_take(map, "status", fl_value_new_string(status_wire));
  fl_value_set_string_take(map, "backend",
                           fl_value_new_string("linuxXdgAutostart"));
  fl_value_set_string_take(map, "identifier",
                           fl_value_new_string(spec.identifier.c_str()));
  if (spec.command.reported_executable_path.empty()) {
    fl_value_set_string_take(map, "executablePath", fl_value_new_null());
  } else {
    fl_value_set_string_take(
        map, "executablePath",
        fl_value_new_string(spec.command.reported_executable_path.c_str()));
  }
  fl_value_set_string_take(map, "canOpenSettings", fl_value_new_bool(FALSE));
  return map;
}

FlMethodResponse*
xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
    FlValue* args,
    const gchar* autostart_dir_override) {
  const LinuxResolvedStartupSpec spec =
      resolve_linux_startup_spec(args, autostart_dir_override);
  g_autoptr(FlValue) state_map = build_linux_launch_at_startup_state_map(spec);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(state_map));
}

FlMethodResponse* xue_hua_navite_window_manager_handle_set_launch_at_startup(
    FlValue* args,
    const gchar* autostart_dir_override) {
  const LinuxResolvedStartupSpec spec =
      resolve_linux_startup_spec(args, autostart_dir_override);
  if (spec.autostart_dir.empty() || spec.command.program.empty()) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        "UNSUPPORTED",
        "Unable to resolve XDG autostart directory or executable path.",
        nullptr));
  }

  const bool enabled =
      xue_hua_get_bool_arg(args, "enabled", FALSE) != FALSE;
  const std::string filename = spec.identifier + ".desktop";
  g_autofree gchar* desktop_path = g_build_filename(
      spec.autostart_dir.c_str(), filename.c_str(), nullptr);

  if (enabled) {
    if (g_mkdir_with_parents(spec.autostart_dir.c_str(), 0700) != 0) {
      return FL_METHOD_RESPONSE(fl_method_error_response_new(
          "LAUNCH_AT_STARTUP_FAILED",
          "Failed to create XDG autostart directory.", nullptr));
    }
    const std::string desktop_content = xue_hua_build_autostart_desktop_entry(
        spec.app_name, spec.command.program, spec.command.arguments);
    g_autoptr(GError) write_err = nullptr;
    if (!g_file_set_contents(desktop_path, desktop_content.c_str(),
                             static_cast<gssize>(desktop_content.size()),
                             &write_err)) {
      return FL_METHOD_RESPONSE(fl_method_error_response_new(
          "LAUNCH_AT_STARTUP_FAILED",
          write_err != nullptr ? write_err->message
                               : "Failed to write autostart desktop entry.",
          nullptr));
    }
  } else {
    if (g_file_test(desktop_path, G_FILE_TEST_EXISTS)) {
      if (g_unlink(desktop_path) != 0 &&
          g_file_test(desktop_path, G_FILE_TEST_EXISTS)) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new(
            "LAUNCH_AT_STARTUP_FAILED",
            "Failed to remove autostart desktop entry.", nullptr));
      }
    }
  }

  g_autoptr(FlValue) state_map = build_linux_launch_at_startup_state_map(spec);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(state_map));
}

FlMethodResponse*
xue_hua_navite_window_manager_handle_open_launch_at_startup_settings() {
  g_autoptr(FlValue) result = fl_value_new_bool(FALSE);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

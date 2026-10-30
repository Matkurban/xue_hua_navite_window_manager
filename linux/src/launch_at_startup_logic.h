#ifndef XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_
#define XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_

#include <string>
#include <vector>

enum XueHuaLinuxStartupStatus {
  XUE_HUA_LINUX_STARTUP_DISABLED = 0,
  XUE_HUA_LINUX_STARTUP_ENABLED = 1,
  XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER = 2,
};

struct XueHuaLinuxStartupCommand {
  std::string program;
  std::vector<std::string> arguments;
  std::string reported_executable_path;
};

// Produces a safe Freedesktop `.desktop` file stem (without `.desktop`),
// stripping leading dots, whitespace, path separators, and `.desktop` suffix.
std::string xue_hua_sanitize_startup_id(const std::string& raw);

// Sanitizes single-line Desktop Entry `string` / `localestring` values
// (`Name=`), stripping CR/LF/NUL to prevent key injection.
std::string xue_hua_sanitize_desktop_text(const std::string& raw);

// Escapes a single token for a Desktop Entry `Exec=` value (Freedesktop
// Desktop Entry Specification: `%%` field-code escaping + double-quoting and
// `\"`, `` \` ``, `\$`, `\\` escaping when reserved characters are present).
std::string xue_hua_escape_desktop_exec_token(const std::string& token);

// Builds the logical `Exec` command string (`program` + `arguments`) before
// GKeyFile `string` escaping.
std::string xue_hua_build_desktop_exec_line(
    const std::string& program,
    const std::vector<std::string>& arguments);

// Escapes a logical value for raw storage in a Desktop Entry `string` key
// (`\\`, `\n`, `\r`, `\t`, leading `\s`), matching `g_key_file_set_string`.
std::string xue_hua_escape_keyfile_string(const std::string& value);

// Resolves the Linux autostart target command in priority order:
// 1. Explicit `configured_executable_path`
// 2. `$APPIMAGE` (avoids transient `/tmp/.mount_*` FUSE mount paths)
// 3. `flatpak run <$FLATPAK_ID>` when running inside Flatpak
// 4. `/proc/self/exe` symlink target
XueHuaLinuxStartupCommand xue_hua_resolve_linux_startup_command(
    const std::string& configured_executable_path,
    const std::vector<std::string>& configured_arguments,
    const std::string& appimage_env,
    const std::string& flatpak_id_env,
    const std::string& proc_self_exe);

// Generates the complete UTF-8 contents of `~/.config/autostart/<id>.desktop`.
std::string xue_hua_build_autostart_desktop_entry(
    const std::string& app_name,
    const std::string& program,
    const std::vector<std::string>& arguments);

// Evaluates parsed `.desktop` flags into `disabled`, `enabled`, or
// `disabledByUser` (`Hidden=true` or `X-GNOME-Autostart-enabled=false`).
XueHuaLinuxStartupStatus xue_hua_evaluate_desktop_entry_state(
    bool file_exists,
    bool valid_desktop_entry,
    bool has_non_empty_exec,
    bool hidden,
    bool has_gnome_enabled,
    bool gnome_enabled);

#endif  // XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_

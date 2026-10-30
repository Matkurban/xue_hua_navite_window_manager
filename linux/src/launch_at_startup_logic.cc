#include "launch_at_startup_logic.h"

#include <cstring>

namespace {

bool is_ascii_space(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string trim_ascii(const std::string& input) {
  std::size_t start = 0;
  while (start < input.size() && is_ascii_space(input[start])) {
    ++start;
  }
  std::size_t end = input.size();
  while (end > start && is_ascii_space(input[end - 1])) {
    --end;
  }
  return input.substr(start, end - start);
}

bool is_safe_startup_id_char(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
         (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
}

bool needs_desktop_exec_quoting(char ch) {
  switch (ch) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '"':
    case '\'':
    case '\\':
    case '>':
    case '<':
    case '~':
    case '|':
    case '&':
    case ';':
    case '$':
    case '*':
    case '?':
    case '#':
    case '(':
    case ')':
    case '`':
      return true;
    default:
      return false;
  }
}

}  // namespace

std::string xue_hua_sanitize_startup_id(const std::string& raw) {
  std::string trimmed = trim_ascii(raw);
  const std::string suffix = ".desktop";
  if (trimmed.size() > suffix.size() &&
      trimmed.compare(trimmed.size() - suffix.size(), suffix.size(), suffix) ==
          0) {
    trimmed.resize(trimmed.size() - suffix.size());
  }

  std::string out;
  out.reserve(trimmed.size());
  for (std::size_t i = 0; i < trimmed.size(); ++i) {
    const char ch = trimmed[i];
    if (is_safe_startup_id_char(ch)) {
      out.push_back(ch);
    } else {
      out.push_back('_');
    }
  }

  std::size_t first_non_dot = 0;
  while (first_non_dot < out.size() && out[first_non_dot] == '.') {
    ++first_non_dot;
  }
  if (first_non_dot > 0) {
    out.erase(0, first_non_dot);
  }

  if (out.empty()) {
    return "flutter_app";
  }
  return out;
}

std::string xue_hua_sanitize_desktop_text(const std::string& raw) {
  std::string cleaned;
  cleaned.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const char ch = raw[i];
    if (ch == '\r' || ch == '\n' || ch == '\0') {
      cleaned.push_back(' ');
    } else {
      cleaned.push_back(ch);
    }
  }
  std::string trimmed = trim_ascii(cleaned);
  if (trimmed.empty()) {
    return "Flutter App";
  }
  return trimmed;
}

std::string xue_hua_escape_desktop_exec_token(const std::string& token) {
  if (token.empty()) {
    return "\"\"";
  }

  std::string percent_escaped;
  percent_escaped.reserve(token.size());
  bool requires_quotes = false;
  for (std::size_t i = 0; i < token.size(); ++i) {
    const char ch = token[i];
    if (ch == '%') {
      percent_escaped.append("%%");
    } else {
      percent_escaped.push_back(ch);
    }
    if (needs_desktop_exec_quoting(ch)) {
      requires_quotes = true;
    }
  }

  if (!requires_quotes) {
    return percent_escaped;
  }

  std::string quoted;
  quoted.reserve(percent_escaped.size() + 4);
  quoted.push_back('"');
  for (std::size_t i = 0; i < percent_escaped.size(); ++i) {
    const char ch = percent_escaped[i];
    if (ch == '"' || ch == '`' || ch == '$' || ch == '\\') {
      quoted.push_back('\\');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

std::string xue_hua_build_desktop_exec_line(
    const std::string& program,
    const std::vector<std::string>& arguments) {
  std::string line = xue_hua_escape_desktop_exec_token(program);
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    line.push_back(' ');
    line.append(xue_hua_escape_desktop_exec_token(arguments[i]));
  }
  return line;
}

std::string xue_hua_escape_keyfile_string(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 4);
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (i == 0 && ch == ' ') {
      out.append("\\s");
    } else if (ch == '\\') {
      out.append("\\\\");
    } else if (ch == '\n') {
      out.append("\\n");
    } else if (ch == '\r') {
      out.append("\\r");
    } else if (ch == '\t') {
      out.append("\\t");
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

XueHuaLinuxStartupCommand xue_hua_resolve_linux_startup_command(
    const std::string& configured_executable_path,
    const std::vector<std::string>& configured_arguments,
    const std::string& appimage_env,
    const std::string& flatpak_id_env,
    const std::string& proc_self_exe) {
  XueHuaLinuxStartupCommand result;

  const std::string explicit_exe = trim_ascii(configured_executable_path);
  if (!explicit_exe.empty()) {
    result.program = explicit_exe;
    result.arguments = configured_arguments;
    result.reported_executable_path = explicit_exe;
    return result;
  }

  const std::string appimage = trim_ascii(appimage_env);
  if (!appimage.empty()) {
    result.program = appimage;
    result.arguments = configured_arguments;
    result.reported_executable_path = appimage;
    return result;
  }

  const std::string flatpak_id = trim_ascii(flatpak_id_env);
  if (!flatpak_id.empty()) {
    result.program = "flatpak";
    result.arguments.reserve(2 + configured_arguments.size());
    result.arguments.push_back("run");
    result.arguments.push_back(flatpak_id);
    for (std::size_t i = 0; i < configured_arguments.size(); ++i) {
      result.arguments.push_back(configured_arguments[i]);
    }
    result.reported_executable_path = "flatpak";
    return result;
  }

  const std::string self_exe = trim_ascii(proc_self_exe);
  result.program = self_exe;
  result.arguments = configured_arguments;
  result.reported_executable_path = self_exe;
  return result;
}

std::string xue_hua_build_autostart_desktop_entry(
    const std::string& app_name,
    const std::string& program,
    const std::vector<std::string>& arguments) {
  const std::string clean_name =
      xue_hua_escape_keyfile_string(xue_hua_sanitize_desktop_text(app_name));
  const std::string exec_line = xue_hua_escape_keyfile_string(
      xue_hua_build_desktop_exec_line(program, arguments));

  std::string entry;
  entry.reserve(256 + clean_name.size() + exec_line.size());
  entry.append("[Desktop Entry]\n");
  entry.append("Type=Application\n");
  entry.append("Version=1.0\n");
  entry.append("Name=");
  entry.append(clean_name);
  entry.append("\nExec=");
  entry.append(exec_line);
  entry.append("\nTerminal=false\n");
  entry.append("StartupNotify=false\n");
  entry.append("Hidden=false\n");
  entry.append("X-GNOME-Autostart-enabled=true\n");
  return entry;
}

XueHuaLinuxStartupStatus xue_hua_evaluate_desktop_entry_state(
    bool file_exists,
    bool valid_desktop_entry,
    bool has_non_empty_exec,
    bool hidden,
    bool has_gnome_enabled,
    bool gnome_enabled) {
  if (!file_exists || !valid_desktop_entry || !has_non_empty_exec) {
    return XUE_HUA_LINUX_STARTUP_DISABLED;
  }
  if (hidden) {
    return XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER;
  }
  if (has_gnome_enabled && !gnome_enabled) {
    return XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER;
  }
  return XUE_HUA_LINUX_STARTUP_ENABLED;
}

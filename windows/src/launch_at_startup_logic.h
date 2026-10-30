#ifndef FLUTTER_PLUGIN_XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_
#define FLUTTER_PLUGIN_XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_

#include <windows.h>

#include <string>
#include <vector>

namespace xue_hua_navite_window_manager {

/// Approval state recorded by Windows Task Manager / Settings under
/// `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run`.
enum class WindowsStartupApproval {
  kApproved = 0,
  kDisabledByUser = 1,
  kDisabledByPolicy = 2,
};

/// Wire-ready launch-at-startup state tuple (`isEnabled` + `status`).
struct WindowsStartupWireState {
  bool is_enabled = false;
  const char* status = "disabled";
};

/// Trims ASCII/Unicode whitespace and replaces characters unsafe in Win32
/// registry value names (`\`, `/`, control chars) with `_`.
inline std::wstring SanitizeWindowsStartupId(const std::wstring& raw) {
  auto is_space = [](wchar_t ch) {
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
  };
  size_t start = 0;
  while (start < raw.size() && is_space(raw[start])) {
    ++start;
  }
  size_t end = raw.size();
  while (end > start && is_space(raw[end - 1])) {
    --end;
  }

  std::wstring out;
  out.reserve(end - start);
  for (size_t i = start; i < end; ++i) {
    const wchar_t ch = raw[i];
    if (static_cast<unsigned int>(ch) < 32u || ch == L'\\' || ch == L'/' ||
        ch == L':' || ch == L'*' || ch == L'?' || ch == L'"' || ch == L'<' ||
        ch == L'>' || ch == L'|') {
      out.push_back(L'_');
    } else {
      out.push_back(ch);
    }
  }
  if (out.empty()) {
    return L"FlutterApp";
  }
  return out;
}

/// Escapes a single command-line argument so Win32 `CommandLineToArgvW`
/// reconstructs the exact original string.
inline std::wstring EscapeWindowsCommandLineArg(const std::wstring& arg) {
  if (arg.empty()) {
    return L"\"\"";
  }
  bool needs_quotes = false;
  for (wchar_t ch : arg) {
    if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\v' ||
        ch == L'"') {
      needs_quotes = true;
      break;
    }
  }
  if (!needs_quotes) {
    return arg;
  }

  std::wstring out;
  out.push_back(L'"');
  size_t i = 0;
  while (i < arg.size()) {
    size_t num_backslashes = 0;
    while (i < arg.size() && arg[i] == L'\\') {
      ++num_backslashes;
      ++i;
    }
    if (i == arg.size()) {
      out.append(num_backslashes * 2, L'\\');
      break;
    }
    if (arg[i] == L'"') {
      out.append(num_backslashes * 2 + 1, L'\\');
      out.push_back(L'"');
    } else {
      out.append(num_backslashes, L'\\');
      out.push_back(arg[i]);
    }
    ++i;
  }
  out.push_back(L'"');
  return out;
}

/// Builds a quoted `CurrentVersion\Run` command string with escaped arguments.
inline std::wstring BuildWindowsStartupCommand(
    const std::wstring& executable_path,
    const std::vector<std::wstring>& arguments) {
  std::wstring exe = executable_path;
  if (exe.size() >= 2 && exe.front() == L'"' && exe.back() == L'"') {
    exe = exe.substr(1, exe.size() - 2);
  }
  std::wstring cmd;
  cmd.push_back(L'"');
  cmd.append(exe);
  cmd.push_back(L'"');
  for (const auto& arg : arguments) {
    cmd.push_back(L' ');
    cmd.append(EscapeWindowsCommandLineArg(arg));
  }
  return cmd;
}

/// Interprets the 12-byte `REG_BINARY` record written by Windows Explorer /
/// Task Manager under `StartupApproved\Run`.
inline WindowsStartupApproval EvaluateWindowsStartupApprovedBytes(
    const BYTE* data,
    size_t size) {
  if (data == nullptr || size == 0) {
    return WindowsStartupApproval::kApproved;
  }
  const BYTE status = data[0];
  if (status == 0x09) {
    return WindowsStartupApproval::kDisabledByPolicy;
  }
  if ((status & 0x01) != 0) {
    return WindowsStartupApproval::kDisabledByUser;
  }
  return WindowsStartupApproval::kApproved;
}

/// Combines `CurrentVersion\Run` presence with `StartupApproved\Run` approval.
inline WindowsStartupWireState EvaluateWindowsRegistryStartupState(
    bool run_value_exists,
    WindowsStartupApproval approval) {
  if (approval == WindowsStartupApproval::kDisabledByPolicy) {
    return WindowsStartupWireState{false, "disabledByPolicy"};
  }
  if (!run_value_exists) {
    return WindowsStartupWireState{false, "disabled"};
  }
  if (approval == WindowsStartupApproval::kDisabledByUser) {
    return WindowsStartupWireState{false, "disabledByUser"};
  }
  return WindowsStartupWireState{true, "enabled"};
}

/// Maps MSIX `StartupTaskState` (`SystemAppData\<PFN>\<TaskId>\State`) DWORD.
inline WindowsStartupWireState EvaluateWindowsMsixStartupState(
    bool state_value_exists,
    DWORD state_dword) {
  if (!state_value_exists) {
    return WindowsStartupWireState{false, "disabled"};
  }
  switch (state_dword) {
    case 1:
      return WindowsStartupWireState{false, "disabledByUser"};
    case 2:
    case 4:
      return WindowsStartupWireState{true, "enabled"};
    case 3:
      return WindowsStartupWireState{false, "disabledByPolicy"};
    case 0:
    default:
      return WindowsStartupWireState{false, "disabled"};
  }
}

}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_LAUNCH_AT_STARTUP_LOGIC_H_

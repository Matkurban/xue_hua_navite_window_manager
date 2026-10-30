#include "xue_hua_navite_window_manager_plugin.h"

#include <shellapi.h>

#include <string>
#include <vector>

#include "method_call_utils.h"

namespace xue_hua_navite_window_manager {

namespace {

constexpr wchar_t kRunRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupApprovedRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kMsixStartupRootPrefix[] =
    L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData\\";
constexpr LONG kAppModelErrorNoPackage = 15700L;

std::wstring QueryCurrentExecutablePath() {
  std::wstring buffer(MAX_PATH, L'\0');
  while (buffer.size() <= 32768) {
    const DWORD len = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0) {
      return std::wstring();
    }
    if (len < buffer.size()) {
      buffer.resize(static_cast<size_t>(len));
      return buffer;
    }
    buffer.resize(buffer.size() * 2, L'\0');
  }
  return std::wstring();
}

std::wstring ExtractExecutableStem(const std::wstring& path) {
  size_t slash = path.find_last_of(L"\\/");
  std::wstring filename =
      (slash == std::wstring::npos) ? path : path.substr(slash + 1);
  if (filename.size() > 4) {
    std::wstring ext = filename.substr(filename.size() - 4);
    if (ext == L".exe" || ext == L".EXE" || ext == L".Exe") {
      filename.resize(filename.size() - 4);
    }
  }
  return filename.empty() ? std::wstring(L"FlutterApp") : filename;
}

std::wstring QueryCurrentPackageFamilyName() {
  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel32 == nullptr) {
    return std::wstring();
  }
  using GetCurrentPackageFamilyNameFn = LONG(WINAPI*)(UINT32*, PWSTR);
  auto fn = reinterpret_cast<GetCurrentPackageFamilyNameFn>(
      GetProcAddress(kernel32, "GetCurrentPackageFamilyName"));
  if (fn == nullptr) {
    return std::wstring();
  }
  UINT32 length = 0;
  LONG rc = fn(&length, nullptr);
  if (rc == kAppModelErrorNoPackage || rc != ERROR_INSUFFICIENT_BUFFER ||
      length == 0) {
    return std::wstring();
  }
  std::wstring buffer(static_cast<size_t>(length), L'\0');
  rc = fn(&length, buffer.data());
  if (rc != ERROR_SUCCESS) {
    return std::wstring();
  }
  while (!buffer.empty() && buffer.back() == L'\0') {
    buffer.pop_back();
  }
  return buffer;
}

bool HasNonEmptyRegistryRunValue(const std::wstring& value_name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunRegistryKey, 0, KEY_QUERY_VALUE,
                    &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD type = 0;
  DWORD bytes = 0;
  LONG rc = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr,
                             &bytes);
  if (rc != ERROR_SUCCESS ||
      (type != REG_SZ && type != REG_EXPAND_SZ) ||
      bytes <= sizeof(wchar_t)) {
    RegCloseKey(key);
    return false;
  }
  std::vector<BYTE> data(static_cast<size_t>(bytes), 0);
  rc = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(),
                        &bytes);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
    return false;
  }
  const auto* chars = reinterpret_cast<const wchar_t*>(data.data());
  return chars[0] != L'\0';
}

WindowsStartupApproval QueryStartupApprovedRun(const std::wstring& value_name,
                                               bool* exists_out) {
  if (exists_out != nullptr) {
    *exists_out = false;
  }
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedRunKey, 0,
                    KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return WindowsStartupApproval::kApproved;
  }
  DWORD type = 0;
  DWORD bytes = 0;
  LONG rc = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr,
                             &bytes);
  if (rc != ERROR_SUCCESS || type != REG_BINARY || bytes == 0) {
    RegCloseKey(key);
    return WindowsStartupApproval::kApproved;
  }
  std::vector<BYTE> data(static_cast<size_t>(bytes), 0);
  rc = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(),
                        &bytes);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS) {
    return WindowsStartupApproval::kApproved;
  }
  if (exists_out != nullptr) {
    *exists_out = true;
  }
  return EvaluateWindowsStartupApprovedBytes(data.data(), data.size());
}

std::wstring DiscoverMsixStartupTaskId(
    const std::wstring& package_family_name) {
  const std::wstring root_path =
      std::wstring(kMsixStartupRootPrefix) + package_family_name;
  HKEY root_key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, root_path.c_str(), 0,
                    KEY_ENUMERATE_SUB_KEYS | KEY_READ,
                    &root_key) != ERROR_SUCCESS) {
    return std::wstring();
  }
  std::wstring found;
  for (DWORD index = 0;; ++index) {
    wchar_t subkey_name[256]{};
    DWORD subkey_len =
        static_cast<DWORD>(sizeof(subkey_name) / sizeof(subkey_name[0]));
    const LONG enum_rc = RegEnumKeyExW(root_key, index, subkey_name,
                                       &subkey_len, nullptr, nullptr, nullptr,
                                       nullptr);
    if (enum_rc != ERROR_SUCCESS) {
      break;
    }
    HKEY task_key = nullptr;
    if (RegOpenKeyExW(root_key, subkey_name, 0, KEY_QUERY_VALUE, &task_key) ==
        ERROR_SUCCESS) {
      DWORD type = 0;
      DWORD size = sizeof(DWORD);
      DWORD state_val = 0;
      const LONG qrc = RegQueryValueExW(
          task_key, L"State", nullptr, &type,
          reinterpret_cast<LPBYTE>(&state_val), &size);
      RegCloseKey(task_key);
      if (qrc == ERROR_SUCCESS && type == REG_DWORD) {
        found.assign(subkey_name, static_cast<size_t>(subkey_len));
        break;
      }
    }
  }
  RegCloseKey(root_key);
  return found;
}

bool QueryMsixStartupTaskDword(const std::wstring& package_family_name,
                               const std::wstring& task_id,
                               DWORD* out_state) {
  if (package_family_name.empty() || task_id.empty() || out_state == nullptr) {
    return false;
  }
  const std::wstring key_path = std::wstring(kMsixStartupRootPrefix) +
                                package_family_name + L"\\" + task_id;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, key_path.c_str(), 0, KEY_QUERY_VALUE,
                    &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD type = 0;
  DWORD size = sizeof(DWORD);
  DWORD value = 0;
  const LONG rc = RegQueryValueExW(
      key, L"State", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS || type != REG_DWORD) {
    return false;
  }
  *out_state = value;
  return true;
}

flutter::EncodableMap BuildLaunchAtStartupStateMap(
    bool is_supported,
    bool is_enabled,
    const std::string& status,
    const std::string& backend,
    const std::wstring& identifier,
    const std::wstring& executable_path,
    bool can_open_settings) {
  flutter::EncodableMap map;
  map[flutter::EncodableValue("isSupported")] =
      flutter::EncodableValue(is_supported);
  map[flutter::EncodableValue("isEnabled")] =
      flutter::EncodableValue(is_enabled);
  map[flutter::EncodableValue("status")] = flutter::EncodableValue(status);
  map[flutter::EncodableValue("backend")] = flutter::EncodableValue(backend);
  map[flutter::EncodableValue("identifier")] =
      flutter::EncodableValue(internal::WideToUtf8(identifier));
  if (executable_path.empty()) {
    map[flutter::EncodableValue("executablePath")] = flutter::EncodableValue();
  } else {
    map[flutter::EncodableValue("executablePath")] =
        flutter::EncodableValue(internal::WideToUtf8(executable_path));
  }
  map[flutter::EncodableValue("canOpenSettings")] =
      flutter::EncodableValue(can_open_settings);
  return map;
}

void ResolveWindowsStartupTarget(const flutter::EncodableMap& args,
                                 std::wstring* out_executable_path,
                                 std::wstring* out_identifier) {
  std::wstring exe_path;
  if (auto custom_exe = internal::GetStringArg(args, "executablePath")) {
    exe_path = internal::TrimWide(internal::Utf8ToWide(*custom_exe));
  }
  if (exe_path.empty()) {
    exe_path = QueryCurrentExecutablePath();
  }

  std::wstring raw_id;
  if (auto custom_id = internal::GetStringArg(args, "identifier")) {
    raw_id = internal::TrimWide(internal::Utf8ToWide(*custom_id));
  }
  if (raw_id.empty()) {
    if (auto app_name = internal::GetStringArg(args, "appName")) {
      raw_id = internal::TrimWide(internal::Utf8ToWide(*app_name));
    }
  }
  if (raw_id.empty()) {
    raw_id = ExtractExecutableStem(exe_path);
  }

  if (out_executable_path != nullptr) {
    *out_executable_path = exe_path;
  }
  if (out_identifier != nullptr) {
    *out_identifier = SanitizeWindowsStartupId(raw_id);
  }
}

}  // namespace

flutter::EncodableMap
XueHuaNaviteWindowManagerPlugin::QueryLaunchAtStartupState(
    const flutter::EncodableMap& args) const {
  std::wstring executable_path;
  std::wstring identifier;
  ResolveWindowsStartupTarget(args, &executable_path, &identifier);

  const std::wstring package_family_name = QueryCurrentPackageFamilyName();
  if (!package_family_name.empty()) {
    std::wstring task_id;
    if (auto explicit_task = internal::GetStringArg(args, "msixTaskId")) {
      task_id = internal::TrimWide(internal::Utf8ToWide(*explicit_task));
    }
    if (task_id.empty()) {
      task_id = DiscoverMsixStartupTaskId(package_family_name);
    }
    if (task_id.empty()) {
      task_id = identifier;
    }

    DWORD state_dword = 0;
    const bool exists =
        QueryMsixStartupTaskDword(package_family_name, task_id, &state_dword);
    const WindowsStartupWireState wire =
        EvaluateWindowsMsixStartupState(exists, state_dword);
    return BuildLaunchAtStartupStateMap(
        true, wire.is_enabled, wire.status, "windowsStartupTask", task_id,
        executable_path, true);
  }

  if (executable_path.empty()) {
    return BuildLaunchAtStartupStateMap(false, false, "unsupported", "none",
                                        identifier, executable_path, false);
  }

  const bool run_exists = HasNonEmptyRegistryRunValue(identifier);
  bool approval_exists = false;
  const WindowsStartupApproval approval =
      QueryStartupApprovedRun(identifier, &approval_exists);
  const WindowsStartupWireState wire =
      EvaluateWindowsRegistryStartupState(run_exists, approval);

  return BuildLaunchAtStartupStateMap(true, wire.is_enabled, wire.status,
                                      "windowsRegistry", identifier,
                                      executable_path, true);
}

bool XueHuaNaviteWindowManagerPlugin::SetLaunchAtStartup(
    const flutter::EncodableMap& args,
    flutter::EncodableMap* out_state,
    std::string* out_error_code,
    std::string* out_error_message) const {
  const bool enabled = internal::GetBoolArg(args, "enabled").value_or(false);

  std::wstring executable_path;
  std::wstring identifier;
  ResolveWindowsStartupTarget(args, &executable_path, &identifier);

  const std::wstring package_family_name = QueryCurrentPackageFamilyName();
  if (!package_family_name.empty()) {
    std::wstring task_id;
    if (auto explicit_task = internal::GetStringArg(args, "msixTaskId")) {
      task_id = internal::TrimWide(internal::Utf8ToWide(*explicit_task));
    }
    if (task_id.empty()) {
      task_id = DiscoverMsixStartupTaskId(package_family_name);
    }
    if (task_id.empty()) {
      task_id = identifier;
    }

    DWORD current_state = 0;
    const bool state_exists = QueryMsixStartupTaskDword(
        package_family_name, task_id, &current_state);
    if (state_exists && current_state == 3 && enabled) {
      // Disabled by Group Policy: preserve policy and report current state.
      if (out_state != nullptr) {
        *out_state = QueryLaunchAtStartupState(args);
      }
      return true;
    }

    const std::wstring key_path = std::wstring(kMsixStartupRootPrefix) +
                                  package_family_name + L"\\" + task_id;
    HKEY task_key = nullptr;
    const LONG open_rc = RegOpenKeyExW(
        HKEY_CURRENT_USER, key_path.c_str(), 0,
        KEY_QUERY_VALUE | KEY_SET_VALUE, &task_key);
    if (open_rc != ERROR_SUCCESS) {
      if (!enabled && open_rc == ERROR_FILE_NOT_FOUND) {
        if (out_state != nullptr) {
          *out_state = QueryLaunchAtStartupState(args);
        }
        return true;
      }
      if (out_error_code != nullptr) {
        *out_error_code = "STARTUP_TASK_NOT_DECLARED";
      }
      if (out_error_message != nullptr) {
        *out_error_message =
            "MSIX StartupTask subkey not found. Declare windows.startupTask "
            "in Package.appxmanifest or provide a valid msixTaskId.";
      }
      return false;
    }

    const DWORD target_state = enabled ? 2u : 0u;
    const LONG set_rc = RegSetValueExW(
        task_key, L"State", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&target_state), sizeof(target_state));
    RegCloseKey(task_key);
    if (set_rc != ERROR_SUCCESS && set_rc != ERROR_ACCESS_DENIED) {
      if (out_error_code != nullptr) {
        *out_error_code = "LAUNCH_AT_STARTUP_FAILED";
      }
      if (out_error_message != nullptr) {
        *out_error_message =
            "Failed to update MSIX StartupTask State in registry (Win32 error " +
            std::to_string(set_rc) + ").";
      }
      return false;
    }

    if (out_state != nullptr) {
      *out_state = QueryLaunchAtStartupState(args);
    }
    return true;
  }

  if (executable_path.empty()) {
    if (out_error_code != nullptr) {
      *out_error_code = "UNSUPPORTED";
    }
    if (out_error_message != nullptr) {
      *out_error_message =
          "Unable to resolve current Windows executable path.";
    }
    return false;
  }

  bool approval_exists = false;
  const WindowsStartupApproval approval =
      QueryStartupApprovedRun(identifier, &approval_exists);
  if (approval == WindowsStartupApproval::kDisabledByPolicy && enabled) {
    if (out_state != nullptr) {
      *out_state = QueryLaunchAtStartupState(args);
    }
    return true;
  }

  if (enabled) {
    const std::vector<std::wstring> cli_args =
        internal::GetWideStringListArg(args, "arguments");
    const std::wstring command =
        BuildWindowsStartupCommand(executable_path, cli_args);

    HKEY run_key = nullptr;
    const LONG create_rc = RegCreateKeyExW(
        HKEY_CURRENT_USER, kRunRegistryKey, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &run_key, nullptr);
    if (create_rc != ERROR_SUCCESS) {
      if (out_error_code != nullptr) {
        *out_error_code = "LAUNCH_AT_STARTUP_FAILED";
      }
      if (out_error_message != nullptr) {
        *out_error_message =
            "Failed to open HKCU CurrentVersion\\Run (Win32 error " +
            std::to_string(create_rc) + ").";
      }
      return false;
    }

    const DWORD byte_len =
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LONG set_rc = RegSetValueExW(
        run_key, identifier.c_str(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), byte_len);
    RegCloseKey(run_key);
    if (set_rc != ERROR_SUCCESS) {
      if (out_error_code != nullptr) {
        *out_error_code = "LAUNCH_AT_STARTUP_FAILED";
      }
      if (out_error_message != nullptr) {
        *out_error_message =
            "Failed to write value to HKCU CurrentVersion\\Run (Win32 error " +
            std::to_string(set_rc) + ").";
      }
      return false;
    }

    // If the entry was previously disabled in Task Manager / Settings, mark it
    // approved again when explicitly re-enabled in-app.
    if (approval_exists &&
        approval == WindowsStartupApproval::kDisabledByUser) {
      HKEY approved_key = nullptr;
      if (RegCreateKeyExW(HKEY_CURRENT_USER, kStartupApprovedRunKey, 0,
                          nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                          nullptr, &approved_key, nullptr) == ERROR_SUCCESS) {
        const BYTE approved_bytes[12] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        RegSetValueExW(approved_key, identifier.c_str(), 0, REG_BINARY,
                       approved_bytes, sizeof(approved_bytes));
        RegCloseKey(approved_key);
      }
    }
  } else {
    HKEY run_key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunRegistryKey, 0, KEY_SET_VALUE,
                      &run_key) == ERROR_SUCCESS) {
      const LONG del_rc = RegDeleteValueW(run_key, identifier.c_str());
      RegCloseKey(run_key);
      if (del_rc != ERROR_SUCCESS && del_rc != ERROR_FILE_NOT_FOUND) {
        if (out_error_code != nullptr) {
          *out_error_code = "LAUNCH_AT_STARTUP_FAILED";
        }
        if (out_error_message != nullptr) {
          *out_error_message =
              "Failed to remove value from HKCU CurrentVersion\\Run (Win32 error " +
              std::to_string(del_rc) + ").";
        }
        return false;
      }
    }

    if (approval_exists &&
        approval != WindowsStartupApproval::kDisabledByPolicy) {
      HKEY approved_key = nullptr;
      if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedRunKey, 0,
                        KEY_SET_VALUE, &approved_key) == ERROR_SUCCESS) {
        RegDeleteValueW(approved_key, identifier.c_str());
        RegCloseKey(approved_key);
      }
    }
  }

  if (out_state != nullptr) {
    *out_state = QueryLaunchAtStartupState(args);
  }
  return true;
}

bool XueHuaNaviteWindowManagerPlugin::OpenLaunchAtStartupSettings() const {
  HINSTANCE instance = ShellExecuteW(
      nullptr, L"open", L"ms-settings:startupapps", nullptr, nullptr,
      SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(instance) > 32;
}

}  // namespace xue_hua_navite_window_manager

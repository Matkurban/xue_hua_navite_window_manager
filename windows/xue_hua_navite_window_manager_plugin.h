#ifndef FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_
#define FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>
#include <vector>

namespace xue_hua_navite_window_manager {

/// Logical rectangle in device-independent pixels (DIP).
struct LogicalRect {
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;

  bool Contains(double px, double py) const {
    return px >= x && px < (x + width) && py >= y && py < (y + height);
  }
};

/// True when a left-button release should activate the Snap Layout control.
///
/// The press must have been armed (`WM_NCLBUTTONDOWN` / `HTMAXBUTTON`) and the
/// cursor must still be inside the registered DIP rect. A release over the
/// Windows 11 Snap Layouts flyout is outside that rect and must not toggle.
inline bool ShouldActivateSnapButton(bool enabled,
                                     bool armed,
                                     bool cursor_inside) {
  return enabled && armed && cursor_inside;
}

/// True when `WM_NCHITTEST` should return `HTMAXBUTTON` for this cursor.
///
/// Only hidden and immersive frames participate. `normal` keeps the system
/// caption buttons, and fullscreen has no custom caption.
inline bool ShouldReportSnapButtonHit(bool enabled,
                                       bool fullscreen,
                                       const std::string& title_bar_style,
                                       const LogicalRect& rect,
                                       double x,
                                       double y) {
  if (!enabled || fullscreen) {
    return false;
  }
  if (title_bar_style != "hidden" && title_bar_style != "immersive") {
    return false;
  }
  return rect.width > 0.0 && rect.height > 0.0 && rect.Contains(x, y);
}

/// Clamps one logical axis to stored limits.
///
/// A limit `<= 0` is unconstrained. When both are set and `max_value` is
/// below `min_value`, the minimum wins, matching macOS `configure`.
/// `SetWindowPos` does not consult `WM_GETMINMAXINFO`, so `setSize` and
/// `setBounds` must clamp before converting DIP to physical pixels.
inline double ClampDipAxis(double value, double min_value, double max_value) {
  if (max_value > 0.0 && value > max_value) {
    value = max_value;
  }
  if (min_value > 0.0 && value < min_value) {
    value = min_value;
  }
  return value;
}

/// Clamps a DIP width and height. Null axes are left alone.
inline void ClampDipSize(double min_width,
                         double min_height,
                         double max_width,
                         double max_height,
                         double* width,
                         double* height) {
  if (width != nullptr) {
    *width = ClampDipAxis(*width, min_width, max_width);
  }
  if (height != nullptr) {
    *height = ClampDipAxis(*height, min_height, max_height);
  }
}

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

/// Production-grade Windows desktop window manager plugin (C++17 / Win32 / DWM).
///
/// Key architecture:
/// - **Win32 Subclassing (`SetWindowSubclass`)**: Intercepts `WM_NCCALCSIZE`,
///   `WM_NCHITTEST`, `WM_GETMINMAXINFO`, `WM_ERASEBKGND`, `WM_CLOSE`, `WM_SIZE`,
///   `WM_MOVE`, `WM_ACTIVATE`, `WM_NCMOUSEMOVE`, `WM_NCLBUTTONDOWN`,
///   `WM_NCLBUTTONUP`, and the captured `WM_LBUTTONUP` directly on the
///   top-level host `HWND`.
/// - **Zero-Flash First Frame**: Fills `WM_ERASEBKGND` with a custom `HBRUSH` matching
///   `backgroundColor`, applies all geometry & DWM attributes prior to revealing, and
///   defers `ShowWindow` until `markReadyToShow` is called after Flutter's first frame.
/// - **Deep Custom Frame & Native Feel**: Removes the standard non-client title bar via
///   `WM_NCCALCSIZE` while preserving `WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX`,
///   enabling Windows 11 native rounded corners (`DWMWCP_ROUND`), DWM drop shadow
///   (`DwmExtendFrameIntoClientArea`), and Windows 11 Snap Layouts (`HTMAXBUTTON`).
class XueHuaNaviteWindowManagerPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  explicit XueHuaNaviteWindowManagerPlugin(
      flutter::PluginRegistrarWindows* registrar = nullptr);

  virtual ~XueHuaNaviteWindowManagerPlugin();

  // Disallow copy and assign.
  XueHuaNaviteWindowManagerPlugin(const XueHuaNaviteWindowManagerPlugin&) =
      delete;
  XueHuaNaviteWindowManagerPlugin& operator=(
      const XueHuaNaviteWindowManagerPlugin&) = delete;

  // Handles incoming MethodChannel calls from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

 private:
  // Resolves the top-level host HWND and installs the Win32 window subclass.
  HWND ResolveMainWindow();
  void AttachSubclassIfNeeded(HWND hwnd);
  void DetachSubclass();

  // Static Win32 Subclass Procedure (`SUBCLASSPROC`).
  static LRESULT CALLBACK SubclassProc(HWND hwnd,
                                       UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData);

  // Instance message handler invoked from SubclassProc.
  LRESULT HandleWindowMessage(HWND hwnd,
                              UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam);

  // DPI conversion helpers (96.0 base DPI <-> physical pixels).
  UINT GetWindowDpi(HWND hwnd) const;
  double GetScaleFactor(HWND hwnd) const;
  int DipToPhysical(double dip, double scale) const;
  double PhysicalToDip(int physical, double scale) const;

  // Atomic configuration & DWM styling helpers.
  flutter::EncodableMap ConfigureAtomically(
      HWND hwnd,
      const flutter::EncodableMap& args);
  void MarkReadyToShow(HWND hwnd);
  void ApplyTitleBarStyle(HWND hwnd, const std::string& style);
  void ApplyDwmFrameAndCorners(HWND hwnd);
  void ApplyBackgroundColor(HWND hwnd, uint32_t argb);
  void ApplyResizable(HWND hwnd, bool resizable);
  void ApplyAlwaysOnTop(HWND hwnd, bool always_on_top);
  void ApplyOpacity(HWND hwnd, double opacity);

  // Geometry & state snapshot helpers.
  RECT ComputeCenteredPhysicalRect(HWND hwnd, int width_px, int height_px) const;
  flutter::EncodableMap GetBoundsMap(HWND hwnd) const;
  flutter::EncodableMap BuildWindowStateSnapshot(HWND hwnd) const;

  // Native drag & edge resize helpers.
  void StartNativeDragging(HWND hwnd);
  void StartNativeResizing(HWND hwnd, const std::string& edge);

  // Snap Layout maximize button. Coordinates are Flutter view client DIPs,
  // matching `RenderBox.localToGlobal`.
  bool ScreenToViewDip(HWND hwnd, POINT screen_pt, double* x, double* y) const;
  bool CursorInSnapButton(HWND hwnd, POINT screen_pt) const;
  void ArmSnapButtonPress(HWND hwnd);
  void FinishSnapButtonPress(HWND hwnd);

  // Event emission over EventChannel.
  void EmitWindowEvent(const std::string& type, HWND hwnd);

  // Launch-at-startup (login auto-start) helpers.
  flutter::EncodableMap QueryLaunchAtStartupState(
      const flutter::EncodableMap& args) const;
  bool SetLaunchAtStartup(const flutter::EncodableMap& args,
                          flutter::EncodableMap* out_state,
                          std::string* out_error_code,
                          std::string* out_error_message) const;
  bool OpenLaunchAtStartupSettings() const;

  flutter::PluginRegistrarWindows* registrar_ = nullptr;
  HWND subclassed_hwnd_ = nullptr;
  bool is_subclassed_ = false;

  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  // Window state & constraints (stored in logical DIP).
  std::string title_bar_style_ = "normal";
  bool prevent_close_ = false;
  bool is_force_closing_ = false;
  bool is_resizable_ = true;
  bool has_shadow_ = true;
  bool is_fullscreen_ = false;
  double resize_border_dip_ = 6.0;

  double min_width_dip_ = 0.0;
  double min_height_dip_ = 0.0;
  double max_width_dip_ = 0.0;
  double max_height_dip_ = 0.0;

  // Saved window style and placement prior to entering fullscreen.
  LONG_PTR pre_fullscreen_style_ = 0;
  LONG_PTR pre_fullscreen_ex_style_ = 0;
  WINDOWPLACEMENT pre_fullscreen_placement_ = {sizeof(WINDOWPLACEMENT)};

  // Custom background brush for zero-flash WM_ERASEBKGND & resize fill.
  COLORREF bg_color_ref_ = RGB(24, 24, 32);
  HBRUSH bg_brush_ = nullptr;

  // Windows 11 Snap Layouts (`HTMAXBUTTON`) & custom drag regions (in logical DIP).
  bool snap_button_enabled_ = false;
  LogicalRect snap_button_rect_;
  bool is_snap_button_hovered_ = false;
  // Armed by WM_NCLBUTTONDOWN/HTMAXBUTTON. The matching release often arrives
  // as WM_LBUTTONUP after SetCapture, not as WM_NCLBUTTONUP.
  bool snap_button_pressed_ = false;
  bool snap_click_posted_ = false;

  std::vector<LogicalRect> drag_rects_;
  std::vector<LogicalRect> exclude_rects_;

  // Track previous maximize/minimize states for accurate event emission.
  bool was_maximized_ = false;
  bool was_minimized_ = false;
};

}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_

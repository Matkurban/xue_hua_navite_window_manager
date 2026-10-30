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

#include "launch_at_startup_logic.h"
#include "window_geometry_logic.h"

namespace xue_hua_navite_window_manager {

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
  void RestoreWindowForGeometry(HWND hwnd);
  void SyncChildViewSize(HWND hwnd);
  void FocusFlutterView(HWND hwnd);
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

  // Asynchronous EventChannel queueing (flushed at top-level on the Win32
  // message loop so Dart listeners never run nested inside WndProc / UIA).
  void QueueWindowEvent(const std::string& type, HWND hwnd);
  void FlushQueuedWindowEvents();

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

  // Queued EventChannel payloads awaiting top-level message-loop delivery.
  std::vector<flutter::EncodableMap> pending_events_;
  bool event_flush_posted_ = false;
};

}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_

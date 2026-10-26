#include "xue_hua_navite_window_manager_plugin.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// Fallback DWM attribute constants for Windows SDKs prior to 10.0.22000.0
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace xue_hua_navite_window_manager {

namespace {

constexpr UINT_PTR kWindowSubclassId = 0x5848574D;  // 'XHWM'

// Posted after the snap-button release so the Dart listener does not run
// nested inside the mouse-up / SetCapture handler.
constexpr UINT kSnapButtonInvokeMessage = WM_APP + 0x58;

// Windows 11 DWM_WINDOW_CORNER_PREFERENCE values
constexpr DWORD kDwmwcpDefault = 0;
constexpr DWORD kDwmwcpDoNotRound = 1;
constexpr DWORD kDwmwcpRound = 2;

// Converts a UTF-8 std::string to UTF-16 std::wstring for Win32 Unicode APIs.
std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return std::wstring();
  }
  int target_len = MultiByteToWideChar(
      CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (target_len <= 0) {
    return std::wstring();
  }
  std::wstring wide(static_cast<size_t>(target_len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      wide.data(), target_len);
  return wide;
}

// Helper functions to read values from flutter::EncodableMap safely.
std::optional<double> GetDoubleArg(const flutter::EncodableMap& args,
                                   const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* d = std::get_if<double>(&it->second)) {
    return *d;
  }
  if (const auto* i32 = std::get_if<int32_t>(&it->second)) {
    return static_cast<double>(*i32);
  }
  if (const auto* i64 = std::get_if<int64_t>(&it->second)) {
    return static_cast<double>(*i64);
  }
  return std::nullopt;
}

std::optional<bool> GetBoolArg(const flutter::EncodableMap& args,
                               const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* b = std::get_if<bool>(&it->second)) {
    return *b;
  }
  return std::nullopt;
}

std::optional<std::string> GetStringArg(const flutter::EncodableMap& args,
                                        const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* s = std::get_if<std::string>(&it->second)) {
    return *s;
  }
  return std::nullopt;
}

std::optional<uint32_t> GetUInt32Arg(const flutter::EncodableMap& args,
                                     const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* i64 = std::get_if<int64_t>(&it->second)) {
    return static_cast<uint32_t>(*i64);
  }
  if (const auto* i32 = std::get_if<int32_t>(&it->second)) {
    return static_cast<uint32_t>(*i32);
  }
  return std::nullopt;
}

int64_t CurrentEpochMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

// static
void XueHuaNaviteWindowManagerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto method_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "xue_hua_navite_window_manager/methods",
          &flutter::StandardMethodCodec::GetInstance());

  auto event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "xue_hua_navite_window_manager/events",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<XueHuaNaviteWindowManagerPlugin>(registrar);
  auto* plugin_ptr = plugin.get();

  method_channel->SetMethodCallHandler(
      [plugin_ptr](const auto& call, auto result) {
        plugin_ptr->HandleMethodCall(call, std::move(result));
      });

  auto stream_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [plugin_ptr](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        plugin_ptr->event_sink_ = std::move(events);
        return nullptr;
      },
      [plugin_ptr](const flutter::EncodableValue* arguments)
          -> std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>> {
        plugin_ptr->event_sink_.reset();
        return nullptr;
      });

  event_channel->SetStreamHandler(std::move(stream_handler));

  registrar->AddPlugin(std::move(plugin));
}

XueHuaNaviteWindowManagerPlugin::XueHuaNaviteWindowManagerPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar) {
  bg_brush_ = CreateSolidBrush(bg_color_ref_);
}

XueHuaNaviteWindowManagerPlugin::~XueHuaNaviteWindowManagerPlugin() {
  DetachSubclass();
  if (bg_brush_ != nullptr) {
    DeleteObject(bg_brush_);
    bg_brush_ = nullptr;
  }
}

HWND XueHuaNaviteWindowManagerPlugin::ResolveMainWindow() {
  if (subclassed_hwnd_ != nullptr && IsWindow(subclassed_hwnd_)) {
    return subclassed_hwnd_;
  }
  if (registrar_ != nullptr && registrar_->GetView() != nullptr) {
    HWND view_hwnd = registrar_->GetView()->GetNativeWindow();
    if (view_hwnd != nullptr) {
      HWND root_hwnd = GetAncestor(view_hwnd, GA_ROOT);
      if (root_hwnd != nullptr) {
        AttachSubclassIfNeeded(root_hwnd);
        return root_hwnd;
      }
    }
  }
  return nullptr;
}

void XueHuaNaviteWindowManagerPlugin::AttachSubclassIfNeeded(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return;
  }
  if (is_subclassed_ && subclassed_hwnd_ == hwnd) {
    return;
  }
  DetachSubclass();
  if (SetWindowSubclass(hwnd, &XueHuaNaviteWindowManagerPlugin::SubclassProc,
                        kWindowSubclassId,
                        reinterpret_cast<DWORD_PTR>(this))) {
    subclassed_hwnd_ = hwnd;
    is_subclassed_ = true;
    was_maximized_ = IsZoomed(hwnd) != FALSE;
    was_minimized_ = IsIconic(hwnd) != FALSE;
  }
}

void XueHuaNaviteWindowManagerPlugin::DetachSubclass() {
  if (is_subclassed_ && subclassed_hwnd_ != nullptr &&
      IsWindow(subclassed_hwnd_)) {
    RemoveWindowSubclass(subclassed_hwnd_,
                         &XueHuaNaviteWindowManagerPlugin::SubclassProc,
                         kWindowSubclassId);
  }
  is_subclassed_ = false;
  subclassed_hwnd_ = nullptr;
}

UINT XueHuaNaviteWindowManagerPlugin::GetWindowDpi(HWND hwnd) const {
  if (hwnd != nullptr && IsWindow(hwnd)) {
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi > 0) {
      return dpi;
    }
  }
  return 96;
}

double XueHuaNaviteWindowManagerPlugin::GetScaleFactor(HWND hwnd) const {
  return static_cast<double>(GetWindowDpi(hwnd)) / 96.0;
}

int XueHuaNaviteWindowManagerPlugin::DipToPhysical(double dip,
                                                   double scale) const {
  return static_cast<int>(std::lround(dip * scale));
}

double XueHuaNaviteWindowManagerPlugin::PhysicalToDip(int physical,
                                                      double scale) const {
  if (scale <= 0.0) {
    return static_cast<double>(physical);
  }
  return static_cast<double>(physical) / scale;
}

// static
LRESULT CALLBACK XueHuaNaviteWindowManagerPlugin::SubclassProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData) {
  auto* self =
      reinterpret_cast<XueHuaNaviteWindowManagerPlugin*>(dwRefData);
  if (self != nullptr) {
    return self->HandleWindowMessage(hwnd, uMsg, wParam, lParam);
  }
  return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT XueHuaNaviteWindowManagerPlugin::HandleWindowMessage(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) {
  switch (uMsg) {
    // 1. Zero-Flash & Anti-Black-Edge Erase Background
    case WM_ERASEBKGND: {
      if (bg_brush_ != nullptr) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, bg_brush_);
        return 1;
      }
      break;
    }

    // 2. Remove Standard Non-Client Title Bar While Keeping Native Frame/Shadow
    case WM_NCCALCSIZE: {
      if (wParam == TRUE &&
          (title_bar_style_ == "hidden" || title_bar_style_ == "immersive") &&
          !is_fullscreen_) {
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        if (IsZoomed(hwnd)) {
          // When maximized, constrain the client rect to the monitor's work area
          // so the window does not spill off-screen or cover an auto-hide taskbar.
          HMONITOR monitor =
              MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
          MONITORINFO monitor_info{};
          monitor_info.cbSize = sizeof(MONITORINFO);
          if (GetMonitorInfoW(monitor, &monitor_info)) {
            params->rgrc[0] = monitor_info.rcWork;
          }
        } else {
          // Returning 0 without calling DefSubclassProc removes the top system
          // title bar and non-client borders while preserving DWM rounded corners
          // and drop shadows.
        }
        return 0;
      }
      break;
    }

    // 3. Custom Hit-Testing: 8-Direction Resize Borders, Snap Layouts (HTMAXBUTTON), & Dragging
    case WM_NCHITTEST: {
      if ((title_bar_style_ == "hidden" || title_bar_style_ == "immersive") &&
          !is_fullscreen_) {
        POINT cursor_screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT window_rect{};
        GetWindowRect(hwnd, &window_rect);

        const double scale = GetScaleFactor(hwnd);
        const int border_px =
            is_resizable_ && !IsZoomed(hwnd)
                ? std::max(4, DipToPhysical(resize_border_dip_, scale))
                : 0;

        // 3a. Check 8-direction resize borders first when not maximized.
        if (border_px > 0) {
          const bool on_left =
              cursor_screen.x >= window_rect.left &&
              cursor_screen.x < window_rect.left + border_px;
          const bool on_right =
              cursor_screen.x < window_rect.right &&
              cursor_screen.x >= window_rect.right - border_px;
          const bool on_top =
              cursor_screen.y >= window_rect.top &&
              cursor_screen.y < window_rect.top + border_px;
          const bool on_bottom =
              cursor_screen.y < window_rect.bottom &&
              cursor_screen.y >= window_rect.bottom - border_px;

          if (on_top && on_left) return HTTOPLEFT;
          if (on_top && on_right) return HTTOPRIGHT;
          if (on_bottom && on_left) return HTBOTTOMLEFT;
          if (on_bottom && on_right) return HTBOTTOMRIGHT;
          if (on_left) return HTLEFT;
          if (on_right) return HTRIGHT;
          if (on_top) return HTTOP;
          if (on_bottom) return HTBOTTOM;
        }

        // Flutter's `localToGlobal` origin is the view HWND, which is not
        // always the root client origin (child view offset / extended frame).
        double cursor_dip_x = 0.0;
        double cursor_dip_y = 0.0;
        if (!ScreenToViewDip(hwnd, cursor_screen, &cursor_dip_x,
                             &cursor_dip_y)) {
          break;
        }

        // 3b. Windows 11 Snap Layouts maximize button (`HTMAXBUTTON`).
        // Hover keeps returning HTMAXBUTTON so DWM can show the snap flyout.
        // The click itself is completed in ArmSnapButtonPress / FinishSnapButtonPress
        // because this non-client hit never reaches Flutter's GestureDetector.
        if (ShouldReportSnapButtonHit(snap_button_enabled_, is_fullscreen_,
                                       title_bar_style_, snap_button_rect_,
                                       cursor_dip_x, cursor_dip_y)) {
          if (!is_snap_button_hovered_) {
            is_snap_button_hovered_ = true;
            EmitWindowEvent("snapButtonHovered", hwnd);
          }
          return HTMAXBUTTON;
        } else if (is_snap_button_hovered_) {
          is_snap_button_hovered_ = false;
          EmitWindowEvent("snapButtonLeft", hwnd);
        }

        // 3c. Check explicit non-draggable exclusion rects.
        bool in_excluded = false;
        for (const auto& rect : exclude_rects_) {
          if (rect.Contains(cursor_dip_x, cursor_dip_y)) {
            in_excluded = true;
            break;
          }
        }

        if (!in_excluded) {
          for (const auto& rect : drag_rects_) {
            if (rect.Contains(cursor_dip_x, cursor_dip_y)) {
              return HTCAPTION;
            }
          }
        }
      }
      break;
    }

    // 4. Maximize clicks. Returning HTMAXBUTTON makes the press non-client, so
    // Flutter's onTap never runs. DefWindowProc(WM_NCLBUTTONDOWN, HTMAXBUTTON)
    // paints the system caption glyph and does not reliably deliver
    // WM_NCLBUTTONUP on a caption-less frame (the release arrives as
    // WM_LBUTTONUP, or not at all). Arm the press, capture the mouse, and
    // post snapButtonClicked only if the release is still inside the button.
    // Snap Layouts hover is unchanged: WM_NCHITTEST still returns HTMAXBUTTON.
    case WM_NCLBUTTONDOWN: {
      if (wParam == HTMAXBUTTON && snap_button_enabled_) {
        ArmSnapButtonPress(hwnd);
        return 0;
      }
      break;
    }

    case WM_NCLBUTTONUP: {
      if (snap_button_pressed_ ||
          (wParam == HTMAXBUTTON && snap_button_enabled_)) {
        if (!snap_button_pressed_) {
          snap_button_pressed_ = true;
        }
        FinishSnapButtonPress(hwnd);
        return 0;
      }
      break;
    }

    case WM_LBUTTONUP: {
      if (snap_button_pressed_) {
        FinishSnapButtonPress(hwnd);
        return 0;
      }
      break;
    }

    case WM_CAPTURECHANGED: {
      // ReleaseCapture from FinishSnapButtonPress clears the flag first.
      // A capture stolen by another window cancels the click.
      snap_button_pressed_ = false;
      break;
    }

    case kSnapButtonInvokeMessage: {
      snap_click_posted_ = false;
      if (snap_button_enabled_) {
        EmitWindowEvent("snapButtonClicked", hwnd);
      }
      return 0;
    }

    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE: {
      if (is_snap_button_hovered_) {
        is_snap_button_hovered_ = false;
        EmitWindowEvent("snapButtonLeft", hwnd);
      }
      break;
    }

    // 5. Native Message-Loop Level Min/Max Size Enforcement (`WM_GETMINMAXINFO`)
    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
      const double scale = GetScaleFactor(hwnd);

      if (min_width_dip_ > 0.0) {
        mmi->ptMinTrackSize.x = DipToPhysical(min_width_dip_, scale);
      }
      if (min_height_dip_ > 0.0) {
        mmi->ptMinTrackSize.y = DipToPhysical(min_height_dip_, scale);
      }
      if (max_width_dip_ > 0.0) {
        mmi->ptMaxTrackSize.x = DipToPhysical(max_width_dip_, scale);
      }
      if (max_height_dip_ > 0.0) {
        mmi->ptMaxTrackSize.y = DipToPhysical(max_height_dip_, scale);
      }
      return 0;
    }

    // 6. Close Interception (`WM_CLOSE`)
    case WM_CLOSE: {
      if (!is_force_closing_ && prevent_close_) {
        EmitWindowEvent("closeRequested", hwnd);
        return 0;
      }
      break;
    }

    // 7. Window Lifecycle & Geometry Event Notifications
    case WM_SIZE: {
      const bool currently_minimized = (wParam == SIZE_MINIMIZED);
      const bool currently_maximized = (wParam == SIZE_MAXIMIZED);

      if (currently_minimized && !was_minimized_) {
        was_minimized_ = true;
        EmitWindowEvent("minimized", hwnd);
      } else if (!currently_minimized && was_minimized_) {
        was_minimized_ = false;
        EmitWindowEvent("restored", hwnd);
      }

      if (currently_maximized && !was_maximized_) {
        was_maximized_ = true;
        EmitWindowEvent("maximized", hwnd);
      } else if (!currently_maximized && was_maximized_ && !currently_minimized) {
        was_maximized_ = false;
        EmitWindowEvent("unmaximized", hwnd);
      }

      if (!currently_minimized) {
        EmitWindowEvent("resized", hwnd);
      }
      break;
    }

    case WM_MOVE: {
      if (!IsIconic(hwnd)) {
        EmitWindowEvent("moved", hwnd);
      }
      break;
    }

    case WM_ACTIVATE: {
      const WORD state = LOWORD(wParam);
      if (state == WA_ACTIVE || state == WA_CLICKACTIVE) {
        EmitWindowEvent("focused", hwnd);
      } else if (state == WA_INACTIVE) {
        EmitWindowEvent("blurred", hwnd);
      }
      break;
    }

    case WM_NCDESTROY: {
      DetachSubclass();
      break;
    }
  }

  return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void XueHuaNaviteWindowManagerPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const std::string& method = method_call.method_name();
  const auto* args_ptr =
      std::get_if<flutter::EncodableMap>(method_call.arguments());
  const flutter::EncodableMap empty_args{};
  const flutter::EncodableMap& args =
      args_ptr != nullptr ? *args_ptr : empty_args;

  HWND hwnd = ResolveMainWindow();

  if (method == "ensureInitialized") {
    result->Success();
    return;
  }

  if (hwnd == nullptr) {
    result->Error("NO_WINDOW", "Unable to resolve main Win32 HWND.");
    return;
  }

  if (method == "configure") {
    flutter::EncodableMap snapshot = ConfigureAtomically(hwnd, args);
    result->Success(flutter::EncodableValue(snapshot));
    return;
  }

  if (method == "markReadyToShow") {
    MarkReadyToShow(hwnd);
    result->Success();
    return;
  }

  if (method == "getState") {
    result->Success(flutter::EncodableValue(BuildWindowStateSnapshot(hwnd)));
    return;
  }

  if (method == "show") {
    const bool focus = GetBoolArg(args, "focus").value_or(true);
    ShowWindow(hwnd, focus ? SW_SHOW : SW_SHOWNA);
    if (focus) {
      SetForegroundWindow(hwnd);
      SetFocus(hwnd);
    }
    result->Success();
    return;
  }

  if (method == "hide") {
    ShowWindow(hwnd, SW_HIDE);
    result->Success();
    return;
  }

  if (method == "focus") {
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    result->Success();
    return;
  }

  if (method == "blur") {
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "close") {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    result->Success();
    return;
  }

  if (method == "destroy") {
    is_force_closing_ = true;
    DestroyWindow(hwnd);
    result->Success();
    return;
  }

  if (method == "maximize") {
    ShowWindow(hwnd, SW_MAXIMIZE);
    result->Success();
    return;
  }

  if (method == "unmaximize") {
    ShowWindow(hwnd, SW_RESTORE);
    result->Success();
    return;
  }

  if (method == "minimize") {
    ShowWindow(hwnd, SW_MINIMIZE);
    result->Success();
    return;
  }

  if (method == "restore") {
    ShowWindow(hwnd, SW_RESTORE);
    result->Success();
    return;
  }

  if (method == "setFullScreen") {
    const bool target_fs = GetBoolArg(args, "isFullScreen").value_or(false);
    if (target_fs != is_fullscreen_) {
      if (target_fs) {
        pre_fullscreen_style_ = GetWindowLongPtrW(hwnd, GWL_STYLE);
        pre_fullscreen_ex_style_ = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        GetWindowPlacement(hwnd, &pre_fullscreen_placement_);

        SetWindowLongPtrW(
            hwnd, GWL_STYLE,
            pre_fullscreen_style_ & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLongPtrW(
            hwnd, GWL_EXSTYLE,
            pre_fullscreen_ex_style_ &
                ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE |
                  WS_EX_STATICEDGE));

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(monitor, &mi);

        is_fullscreen_ = true;
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        EmitWindowEvent("enteredFullScreen", hwnd);
      } else {
        is_fullscreen_ = false;
        SetWindowLongPtrW(hwnd, GWL_STYLE, pre_fullscreen_style_);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, pre_fullscreen_ex_style_);
        SetWindowPlacement(hwnd, &pre_fullscreen_placement_);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        ApplyDwmFrameAndCorners(hwnd);
        EmitWindowEvent("exitedFullScreen", hwnd);
      }
    }
    result->Success();
    return;
  }

  if (method == "setBounds") {
    const double scale = GetScaleFactor(hwnd);
    RECT current{};
    GetWindowRect(hwnd, &current);
    const int x = args.count(flutter::EncodableValue("x"))
                      ? DipToPhysical(GetDoubleArg(args, "x").value(), scale)
                      : current.left;
    const int y = args.count(flutter::EncodableValue("y"))
                      ? DipToPhysical(GetDoubleArg(args, "y").value(), scale)
                      : current.top;
    int w = current.right - current.left;
    int h = current.bottom - current.top;
    if (args.count(flutter::EncodableValue("width"))) {
      const double width = ClampDipAxis(GetDoubleArg(args, "width").value(),
                                        min_width_dip_, max_width_dip_);
      w = DipToPhysical(width, scale);
    }
    if (args.count(flutter::EncodableValue("height"))) {
      const double height = ClampDipAxis(GetDoubleArg(args, "height").value(),
                                         min_height_dip_, max_height_dip_);
      h = DipToPhysical(height, scale);
    }

    SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "getBounds") {
    result->Success(flutter::EncodableValue(GetBoundsMap(hwnd)));
    return;
  }

  if (method == "setSize") {
    const double scale = GetScaleFactor(hwnd);
    double width = GetDoubleArg(args, "width").value_or(800.0);
    double height = GetDoubleArg(args, "height").value_or(600.0);
    ClampDipSize(min_width_dip_, min_height_dip_, max_width_dip_,
                 max_height_dip_, &width, &height);
    const int w = DipToPhysical(width, scale);
    const int h = DipToPhysical(height, scale);
    SetWindowPos(hwnd, nullptr, 0, 0, w, h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "setPosition") {
    const double scale = GetScaleFactor(hwnd);
    const int x = DipToPhysical(GetDoubleArg(args, "x").value_or(0.0), scale);
    const int y = DipToPhysical(GetDoubleArg(args, "y").value_or(0.0), scale);
    SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "center") {
    const double scale = GetScaleFactor(hwnd);
    RECT current{};
    GetWindowRect(hwnd, &current);
    int w = current.right - current.left;
    int h = current.bottom - current.top;
    if (auto width_opt = GetDoubleArg(args, "width")) {
      w = DipToPhysical(
          ClampDipAxis(*width_opt, min_width_dip_, max_width_dip_), scale);
    }
    if (auto height_opt = GetDoubleArg(args, "height")) {
      h = DipToPhysical(
          ClampDipAxis(*height_opt, min_height_dip_, max_height_dip_), scale);
    }
    RECT centered = ComputeCenteredPhysicalRect(hwnd, w, h);
    SetWindowPos(hwnd, nullptr, centered.left, centered.top,
                 centered.right - centered.left,
                 centered.bottom - centered.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "setMinimumSize") {
    min_width_dip_ = GetDoubleArg(args, "width").value_or(0.0);
    min_height_dip_ = GetDoubleArg(args, "height").value_or(0.0);
    result->Success();
    return;
  }

  if (method == "setMaximumSize") {
    max_width_dip_ = GetDoubleArg(args, "width").value_or(0.0);
    max_height_dip_ = GetDoubleArg(args, "height").value_or(0.0);
    result->Success();
    return;
  }

  if (method == "setResizable") {
    ApplyResizable(hwnd, GetBoolArg(args, "resizable").value_or(true));
    result->Success();
    return;
  }

  if (method == "setAlwaysOnTop") {
    ApplyAlwaysOnTop(hwnd, GetBoolArg(args, "alwaysOnTop").value_or(false));
    result->Success();
    return;
  }

  if (method == "setPreventClose") {
    prevent_close_ = GetBoolArg(args, "preventClose").value_or(false);
    result->Success();
    return;
  }

  if (method == "setTitle") {
    if (auto title = GetStringArg(args, "title")) {
      std::wstring wide_title = Utf8ToWide(*title);
      SetWindowTextW(hwnd, wide_title.c_str());
    }
    result->Success();
    return;
  }

  if (method == "setTitleBarStyle") {
    ApplyTitleBarStyle(
        hwnd, GetStringArg(args, "titleBarStyle").value_or("normal"));
    result->Success();
    return;
  }

  if (method == "setTrafficLightsOffset" ||
      method == "setTrafficLightsVisible") {
    // macOS-specific APIs; no-op on Windows.
    result->Success();
    return;
  }

  if (method == "setBackgroundColor") {
    if (auto argb = GetUInt32Arg(args, "backgroundColor")) {
      ApplyBackgroundColor(hwnd, *argb);
    }
    result->Success();
    return;
  }

  if (method == "setOpacity") {
    if (auto opacity = GetDoubleArg(args, "opacity")) {
      ApplyOpacity(hwnd, *opacity);
    }
    result->Success();
    return;
  }

  if (method == "setHasShadow") {
    has_shadow_ = GetBoolArg(args, "hasShadow").value_or(true);
    ApplyDwmFrameAndCorners(hwnd);
    result->Success();
    return;
  }

  if (method == "startDragging") {
    StartNativeDragging(hwnd);
    result->Success();
    return;
  }

  if (method == "startResizing") {
    if (auto edge = GetStringArg(args, "edge")) {
      StartNativeResizing(hwnd, *edge);
    }
    result->Success();
    return;
  }

  if (method == "setSnapLayoutButtonRect") {
    const bool enabled = GetBoolArg(args, "enabled").value_or(false);
    snap_button_enabled_ = enabled;
    if (enabled) {
      snap_button_rect_.x = GetDoubleArg(args, "x").value_or(0.0);
      snap_button_rect_.y = GetDoubleArg(args, "y").value_or(0.0);
      snap_button_rect_.width = GetDoubleArg(args, "width").value_or(0.0);
      snap_button_rect_.height = GetDoubleArg(args, "height").value_or(0.0);
    } else {
      is_snap_button_hovered_ = false;
    }
    result->Success();
    return;
  }

  if (method == "setTitleBarDragRects") {
    drag_rects_.clear();
    exclude_rects_.clear();

    auto parse_rects = [](const flutter::EncodableMap& map, const char* key,
                          std::vector<LogicalRect>* out) {
      auto it = map.find(flutter::EncodableValue(key));
      if (it == map.end()) return;
      const auto* list = std::get_if<flutter::EncodableList>(&it->second);
      if (!list) return;
      for (const auto& item : *list) {
        if (const auto* rmap = std::get_if<flutter::EncodableMap>(&item)) {
          LogicalRect r{};
          r.x = GetDoubleArg(*rmap, "x").value_or(0.0);
          r.y = GetDoubleArg(*rmap, "y").value_or(0.0);
          r.width = GetDoubleArg(*rmap, "width").value_or(0.0);
          r.height = GetDoubleArg(*rmap, "height").value_or(0.0);
          out->push_back(r);
        }
      }
    };

    parse_rects(args, "dragRects", &drag_rects_);
    parse_rects(args, "excludeRects", &exclude_rects_);
    result->Success();
    return;
  }

  result->NotImplemented();
}

flutter::EncodableMap XueHuaNaviteWindowManagerPlugin::ConfigureAtomically(
    HWND hwnd,
    const flutter::EncodableMap& args) {
  // 1. Background Color & Brush (prevents white/black flash on startup and resize).
  if (auto argb = GetUInt32Arg(args, "backgroundColor")) {
    ApplyBackgroundColor(hwnd, *argb);
  }

  // 2. Title, PreventClose, Shadow, Border Hit-Test Width.
  if (auto title = GetStringArg(args, "title")) {
    std::wstring wide_title = Utf8ToWide(*title);
    SetWindowTextW(hwnd, wide_title.c_str());
  }
  if (auto prevent_close = GetBoolArg(args, "preventClose")) {
    prevent_close_ = *prevent_close;
  }
  if (auto has_shadow = GetBoolArg(args, "hasShadow")) {
    has_shadow_ = *has_shadow;
  }
  if (auto border_width = GetDoubleArg(args, "resizeBorderWidth")) {
    resize_border_dip_ = *border_width;
  }

  // 3. Min / Max Size Constraints (in DIP, converted dynamically per monitor DPI in WM_GETMINMAXINFO).
  if (auto min_w = GetDoubleArg(args, "minWidth")) {
    min_width_dip_ = *min_w;
  }
  if (auto min_h = GetDoubleArg(args, "minHeight")) {
    min_height_dip_ = *min_h;
  }
  if (auto max_w = GetDoubleArg(args, "maxWidth")) {
    max_width_dip_ = *max_w;
  }
  if (auto max_h = GetDoubleArg(args, "maxHeight")) {
    max_height_dip_ = *max_h;
  }

  // 4. Resizable & TitleBarStyle & DWM Rounded Corners / Shadow.
  if (auto resizable = GetBoolArg(args, "resizable")) {
    is_resizable_ = *resizable;
  }
  if (auto style = GetStringArg(args, "titleBarStyle")) {
    title_bar_style_ = *style;
  }
  ApplyResizable(hwnd, is_resizable_);
  ApplyDwmFrameAndCorners(hwnd);

  // 5. Compute Target Physical Bounds in Memory (Size -> Clamp -> Center or Position).
  const double scale = GetScaleFactor(hwnd);
  RECT current_rect{};
  GetWindowRect(hwnd, &current_rect);

  int target_w = current_rect.right - current_rect.left;
  int target_h = current_rect.bottom - current_rect.top;

  if (auto w_dip = GetDoubleArg(args, "width")) {
    const double clamped_w =
        ClampDipAxis(*w_dip, min_width_dip_, max_width_dip_);
    target_w = DipToPhysical(clamped_w, scale);
  }
  if (auto h_dip = GetDoubleArg(args, "height")) {
    const double clamped_h =
        ClampDipAxis(*h_dip, min_height_dip_, max_height_dip_);
    target_h = DipToPhysical(clamped_h, scale);
  }

  int target_x = current_rect.left;
  int target_y = current_rect.top;

  const bool center = GetBoolArg(args, "center").value_or(false);
  if (center) {
    RECT centered = ComputeCenteredPhysicalRect(hwnd, target_w, target_h);
    target_x = centered.left;
    target_y = centered.top;
  } else {
    if (auto x_dip = GetDoubleArg(args, "x")) {
      target_x = DipToPhysical(*x_dip, scale);
    }
    if (auto y_dip = GetDoubleArg(args, "y")) {
      target_y = DipToPhysical(*y_dip, scale);
    }
  }

  // 6. Single Atomic SetWindowPos for Z-Order, Geometry, and Frame Change.
  const bool always_on_top = GetBoolArg(args, "alwaysOnTop").value_or(
      (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
  HWND insert_after = always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;

  SetWindowPos(hwnd, insert_after, target_x, target_y, target_w, target_h,
               SWP_NOACTIVATE | SWP_FRAMECHANGED);

  // 7. Show immediately if requested.
  const bool show_when_ready =
      GetBoolArg(args, "showWhenReady").value_or(false);
  if (show_when_ready) {
    MarkReadyToShow(hwnd);
  }

  return BuildWindowStateSnapshot(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::MarkReadyToShow(HWND hwnd) {
  if (!IsWindowVisible(hwnd)) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
  }
  RedrawWindow(hwnd, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  SetForegroundWindow(hwnd);
  SetFocus(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::ApplyTitleBarStyle(
    HWND hwnd,
    const std::string& style) {
  title_bar_style_ = style;
  ApplyDwmFrameAndCorners(hwnd);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
}

void XueHuaNaviteWindowManagerPlugin::ApplyDwmFrameAndCorners(HWND hwnd) {
  // Maintain WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU
  // so Windows 11 Snap Layouts, Win+Arrow aero snap, and DWM animations work natively.
  LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  if (is_resizable_) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
  } else {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  SetWindowLongPtrW(hwnd, GWL_STYLE, style);

  // Enable Windows 11 native rounded corners (`DWMWCP_ROUND`).
  DWORD corner_pref = has_shadow_ ? kDwmwcpRound : kDwmwcpDoNotRound;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_pref,
                        sizeof(corner_pref));

  // Extend the DWM frame by 1px at the top (or 0 when shadow is disabled) so DWM
  // renders the native system drop shadow around a custom frameless window.
  MARGINS margins = has_shadow_ ? MARGINS{0, 0, 1, 0} : MARGINS{0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void XueHuaNaviteWindowManagerPlugin::ApplyBackgroundColor(HWND hwnd,
                                                           uint32_t argb) {
  const BYTE r = static_cast<BYTE>((argb >> 16) & 0xFF);
  const BYTE g = static_cast<BYTE>((argb >> 8) & 0xFF);
  const BYTE b = static_cast<BYTE>(argb & 0xFF);
  bg_color_ref_ = RGB(r, g, b);

  if (bg_brush_ != nullptr) {
    DeleteObject(bg_brush_);
  }
  bg_brush_ = CreateSolidBrush(bg_color_ref_);
  InvalidateRect(hwnd, nullptr, TRUE);
}

void XueHuaNaviteWindowManagerPlugin::ApplyResizable(HWND hwnd,
                                                     bool resizable) {
  is_resizable_ = resizable;
  LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  if (resizable) {
    style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
  } else {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  SetWindowLongPtrW(hwnd, GWL_STYLE, style);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
}

void XueHuaNaviteWindowManagerPlugin::ApplyAlwaysOnTop(HWND hwnd,
                                                       bool always_on_top) {
  SetWindowPos(hwnd, always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void XueHuaNaviteWindowManagerPlugin::ApplyOpacity(HWND hwnd, double opacity) {
  double clamped = std::clamp(opacity, 0.0, 1.0);
  LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (clamped < 1.0) {
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(
        hwnd, 0, static_cast<BYTE>( std::lround(clamped * 255.0)), LWA_ALPHA);
  } else {
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
  }
}

RECT XueHuaNaviteWindowManagerPlugin::ComputeCenteredPhysicalRect(
    HWND hwnd,
    int width_px,
    int height_px) const {
  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{};
  mi.cbSize = sizeof(MONITORINFO);
  if (GetMonitorInfoW(monitor, &mi)) {
    const int work_w = mi.rcWork.right - mi.rcWork.left;
    const int work_h = mi.rcWork.bottom - mi.rcWork.top;
    const int left = mi.rcWork.left + (work_w - width_px) / 2;
    const int top = mi.rcWork.top + (work_h - height_px) / 2;
    return RECT{left, top, left + width_px, top + height_px};
  }
  return RECT{100, 100, 100 + width_px, 100 + height_px};
}

flutter::EncodableMap XueHuaNaviteWindowManagerPlugin::GetBoundsMap(
    HWND hwnd) const {
  RECT rc{};
  GetWindowRect(hwnd, &rc);
  const double scale = GetScaleFactor(hwnd);

  flutter::EncodableMap map;
  map[flutter::EncodableValue("x")] =
      flutter::EncodableValue(PhysicalToDip(rc.left, scale));
  map[flutter::EncodableValue("y")] =
      flutter::EncodableValue(PhysicalToDip(rc.top, scale));
  map[flutter::EncodableValue("width")] =
      flutter::EncodableValue(PhysicalToDip(rc.right - rc.left, scale));
  map[flutter::EncodableValue("height")] =
      flutter::EncodableValue(PhysicalToDip(rc.bottom - rc.top, scale));
  return map;
}

flutter::EncodableMap
XueHuaNaviteWindowManagerPlugin::BuildWindowStateSnapshot(HWND hwnd) const {
  flutter::EncodableMap map = GetBoundsMap(hwnd);
  const LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  map[flutter::EncodableValue("isVisible")] =
      flutter::EncodableValue(IsWindowVisible(hwnd) != FALSE);
  map[flutter::EncodableValue("isMaximized")] =
      flutter::EncodableValue(IsZoomed(hwnd) != FALSE);
  map[flutter::EncodableValue("isMinimized")] =
      flutter::EncodableValue(IsIconic(hwnd) != FALSE);
  map[flutter::EncodableValue("isFullScreen")] =
      flutter::EncodableValue(is_fullscreen_);
  map[flutter::EncodableValue("isFocused")] =
      flutter::EncodableValue(GetForegroundWindow() == hwnd);
  map[flutter::EncodableValue("isAlwaysOnTop")] =
      flutter::EncodableValue((ex_style & WS_EX_TOPMOST) != 0);
  map[flutter::EncodableValue("isResizable")] =
      flutter::EncodableValue(is_resizable_);
  map[flutter::EncodableValue("isPreventClose")] =
      flutter::EncodableValue(prevent_close_);
  return map;
}

void XueHuaNaviteWindowManagerPlugin::StartNativeDragging(HWND hwnd) {
  ReleaseCapture();
  SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void XueHuaNaviteWindowManagerPlugin::StartNativeResizing(
    HWND hwnd,
    const std::string& edge) {
  if (!is_resizable_ || IsZoomed(hwnd)) {
    return;
  }
  WPARAM hit_target = 0;
  if (edge == "top") {
    hit_target = HTTOP;
  } else if (edge == "bottom") {
    hit_target = HTBOTTOM;
  } else if (edge == "left") {
    hit_target = HTLEFT;
  } else if (edge == "right") {
    hit_target = HTRIGHT;
  } else if (edge == "topLeft") {
    hit_target = HTTOPLEFT;
  } else if (edge == "topRight") {
    hit_target = HTTOPRIGHT;
  } else if (edge == "bottomLeft") {
    hit_target = HTBOTTOMLEFT;
  } else if (edge == "bottomRight") {
    hit_target = HTBOTTOMRIGHT;
  }

  if (hit_target != 0) {
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, hit_target, 0);
  }
}

bool XueHuaNaviteWindowManagerPlugin::ScreenToViewDip(HWND hwnd,
                                                         POINT screen_pt,
                                                         double* x,
                                                         double* y) const {
  if (x == nullptr || y == nullptr) {
    return false;
  }
  HWND target = hwnd;
  if (registrar_ != nullptr && registrar_->GetView() != nullptr) {
    HWND view = registrar_->GetView()->GetNativeWindow();
    if (view != nullptr && IsWindow(view)) {
      target = view;
    }
  }
  POINT client = screen_pt;
  if (!ScreenToClient(target, &client)) {
    return false;
  }
  const double scale = GetScaleFactor(hwnd);
  *x = PhysicalToDip(client.x, scale);
  *y = PhysicalToDip(client.y, scale);
  return true;
}

bool XueHuaNaviteWindowManagerPlugin::CursorInSnapButton(
    HWND hwnd,
    POINT screen_pt) const {
  double x = 0.0;
  double y = 0.0;
  if (!ScreenToViewDip(hwnd, screen_pt, &x, &y)) {
    return false;
  }
  return snap_button_rect_.Contains(x, y);
}

void XueHuaNaviteWindowManagerPlugin::ArmSnapButtonPress(HWND hwnd) {
  snap_button_pressed_ = true;
  // Capture so the release cannot be dropped when DefWindowProc is not called
  // for HTMAXBUTTON. The Snap Layouts flyout is a separate process and still
  // receives clicks that start on the flyout itself (hover, then click a zone).
  SetCapture(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::FinishSnapButtonPress(HWND hwnd) {
  if (!snap_button_pressed_) {
    return;
  }
  POINT screen_pt{};
  GetCursorPos(&screen_pt);
  const bool inside = CursorInSnapButton(hwnd, screen_pt);
  // Clear before ReleaseCapture. That call synchronously delivers
  // WM_CAPTURECHANGED, which must not cancel a click we already accepted.
  snap_button_pressed_ = false;
  if (GetCapture() == hwnd) {
    ReleaseCapture();
  }
  if (!ShouldActivateSnapButton(snap_button_enabled_, true, inside)) {
    return;
  }
  if (snap_click_posted_) {
    return;
  }
  snap_click_posted_ = true;
  PostMessageW(hwnd, kSnapButtonInvokeMessage, 0, 0);
}

void XueHuaNaviteWindowManagerPlugin::EmitWindowEvent(const std::string& type,
                                                      HWND hwnd) {
  if (!event_sink_) {
    return;
  }
  flutter::EncodableMap payload = GetBoundsMap(hwnd);
  payload[flutter::EncodableValue("type")] = flutter::EncodableValue(type);
  payload[flutter::EncodableValue("isMaximized")] =
      flutter::EncodableValue(IsZoomed(hwnd) != FALSE);
  payload[flutter::EncodableValue("isFullScreen")] =
      flutter::EncodableValue(is_fullscreen_);
  payload[flutter::EncodableValue("timestamp")] =
      flutter::EncodableValue(CurrentEpochMillis());
  event_sink_->Success(flutter::EncodableValue(payload));
}

}  // namespace xue_hua_navite_window_manager

#include "xue_hua_navite_window_manager_plugin.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>

#include "method_call_utils.h"

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

// Windows 11 DWM_WINDOW_CORNER_PREFERENCE values
constexpr DWORD kDwmwcpDefault = 0;
constexpr DWORD kDwmwcpDoNotRound = 1;
constexpr DWORD kDwmwcpRound = 2;

}  // namespace

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

flutter::EncodableMap XueHuaNaviteWindowManagerPlugin::ConfigureAtomically(
    HWND hwnd,
    const flutter::EncodableMap& args) {
  // 1. Background Color & Brush (prevents white/black flash on startup and resize).
  if (auto argb = internal::GetUInt32Arg(args, "backgroundColor")) {
    ApplyBackgroundColor(hwnd, *argb);
  }

  // 2. Title, PreventClose, Shadow, Border Hit-Test Width.
  if (auto title = internal::GetStringArg(args, "title")) {
    std::wstring wide_title = internal::Utf8ToWide(*title);
    SetWindowTextW(hwnd, wide_title.c_str());
  }
  if (auto prevent_close = internal::GetBoolArg(args, "preventClose")) {
    prevent_close_ = *prevent_close;
  }
  if (auto has_shadow = internal::GetBoolArg(args, "hasShadow")) {
    has_shadow_ = *has_shadow;
  }
  if (auto border_width = internal::GetDoubleArg(args, "resizeBorderWidth")) {
    resize_border_dip_ = *border_width;
  }

  // 3. Min / Max Size Constraints (in DIP, converted dynamically per monitor DPI in WM_GETMINMAXINFO).
  if (auto min_w = internal::GetDoubleArg(args, "minWidth")) {
    min_width_dip_ = *min_w;
  }
  if (auto min_h = internal::GetDoubleArg(args, "minHeight")) {
    min_height_dip_ = *min_h;
  }
  if (auto max_w = internal::GetDoubleArg(args, "maxWidth")) {
    max_width_dip_ = *max_w;
  }
  if (auto max_h = internal::GetDoubleArg(args, "maxHeight")) {
    max_height_dip_ = *max_h;
  }

  // 4. Resizable & TitleBarStyle & DWM Rounded Corners / Shadow.
  if (auto resizable = internal::GetBoolArg(args, "resizable")) {
    is_resizable_ = *resizable;
  }
  if (auto style = internal::GetStringArg(args, "titleBarStyle")) {
    title_bar_style_ = *style;
  }

  const auto w_dip = internal::GetDoubleArg(args, "width");
  const auto h_dip = internal::GetDoubleArg(args, "height");
  const auto x_dip = internal::GetDoubleArg(args, "x");
  const auto y_dip = internal::GetDoubleArg(args, "y");
  const bool center = internal::GetBoolArg(args, "center").value_or(false);
  const bool has_geometry_request = w_dip.has_value() || h_dip.has_value() ||
                                    x_dip.has_value() || y_dip.has_value() ||
                                    center;

  // Restore to normal windowed state BEFORE reading GetWindowRect or calling
  // SetWindowPos. Win32 SetWindowPos never clears WS_MAXIMIZE/WS_MINIMIZE, so
  // applying windowed bounds onto a maximized HWND would leave IsZoomed(hwnd)
  // true and cause WM_NCCALCSIZE / DWM to crop or stretch the client surface.
  if (ShouldRestoreWindowBeforeGeometry(
          has_geometry_request, IsZoomed(hwnd) != FALSE,
          IsIconic(hwnd) != FALSE, is_fullscreen_)) {
    RestoreWindowForGeometry(hwnd);
  }

  ApplyDwmFrameAndCorners(hwnd);

  // 5. Compute Target Physical Bounds in Memory (Size -> Clamp -> Center or Position).
  const double scale = GetScaleFactor(hwnd);
  RECT current_rect{};
  GetWindowRect(hwnd, &current_rect);

  int target_w = current_rect.right - current_rect.left;
  int target_h = current_rect.bottom - current_rect.top;

  if (w_dip.has_value()) {
    const double clamped_w =
        ClampDipAxis(*w_dip, min_width_dip_, max_width_dip_);
    target_w = DipToPhysical(clamped_w, scale);
  }
  if (h_dip.has_value()) {
    const double clamped_h =
        ClampDipAxis(*h_dip, min_height_dip_, max_height_dip_);
    target_h = DipToPhysical(clamped_h, scale);
  }

  int target_x = current_rect.left;
  int target_y = current_rect.top;

  if (center) {
    RECT centered = ComputeCenteredPhysicalRect(hwnd, target_w, target_h);
    target_x = centered.left;
    target_y = centered.top;
  } else {
    if (x_dip.has_value()) {
      target_x = DipToPhysical(*x_dip, scale);
    }
    if (y_dip.has_value()) {
      target_y = DipToPhysical(*y_dip, scale);
    }
  }

  // 6. Single Atomic SetWindowPos for Z-Order, Geometry, and Frame Change.
  const bool always_on_top = internal::GetBoolArg(args, "alwaysOnTop").value_or(
      (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
  HWND insert_after = always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST;

  UINT swp_flags = SWP_NOACTIVATE | SWP_FRAMECHANGED;
  if (!w_dip.has_value() && !h_dip.has_value()) {
    swp_flags |= SWP_NOSIZE;
  }
  if (!center && !x_dip.has_value() && !y_dip.has_value()) {
    swp_flags |= SWP_NOMOVE;
  }

  SetWindowPos(hwnd, insert_after, target_x, target_y, target_w, target_h,
               swp_flags);
  SyncChildViewSize(hwnd);

  // 7. Show immediately if requested.
  const bool show_when_ready =
      internal::GetBoolArg(args, "showWhenReady").value_or(false);
  if (show_when_ready) {
    MarkReadyToShow(hwnd);
  }

  return BuildWindowStateSnapshot(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::RestoreWindowForGeometry(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return;
  }

  if (is_fullscreen_) {
    is_fullscreen_ = false;
    const LONG_PTR restored_style =
        pre_fullscreen_style_ & ~(WS_MAXIMIZE | WS_MINIMIZE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, restored_style);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, pre_fullscreen_ex_style_);

    WINDOWPLACEMENT wp = pre_fullscreen_placement_;
    wp.length = sizeof(WINDOWPLACEMENT);
    wp.flags &= ~WPF_RESTORETOMAXIMIZED;
    wp.showCmd = IsWindowVisible(hwnd) ? SW_SHOWNORMAL : SW_HIDE;
    SetWindowPlacement(hwnd, &wp);

    ApplyDwmFrameAndCorners(hwnd);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (was_maximized_) {
      was_maximized_ = false;
      QueueWindowEvent("unmaximized", hwnd);
    }
    if (was_minimized_) {
      was_minimized_ = false;
      QueueWindowEvent("restored", hwnd);
    }
    QueueWindowEvent("exitedFullScreen", hwnd);
  }

  if (IsZoomed(hwnd) != FALSE || IsIconic(hwnd) != FALSE) {
    if (IsWindowVisible(hwnd) != FALSE) {
      // SW_SHOWNORMAL restores both minimized and maximized windows directly to
      // normal windowed state (even when WPF_RESTORETOMAXIMIZED was set).
      ShowWindow(hwnd, SW_SHOWNORMAL);
    } else {
      WINDOWPLACEMENT wp{};
      wp.length = sizeof(WINDOWPLACEMENT);
      if (GetWindowPlacement(hwnd, &wp)) {
        wp.flags &= ~WPF_RESTORETOMAXIMIZED;
        wp.showCmd = SW_HIDE;
        SetWindowPlacement(hwnd, &wp);
      }
    }

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & (WS_MAXIMIZE | WS_MINIMIZE)) != 0) {
      SetWindowLongPtrW(hwnd, GWL_STYLE,
                        style & ~(WS_MAXIMIZE | WS_MINIMIZE));
      SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    if (was_minimized_ && IsIconic(hwnd) == FALSE) {
      was_minimized_ = false;
      QueueWindowEvent("restored", hwnd);
    }
    if (was_maximized_ && IsZoomed(hwnd) == FALSE) {
      was_maximized_ = false;
      QueueWindowEvent("unmaximized", hwnd);
    }
  }
}

void XueHuaNaviteWindowManagerPlugin::SyncChildViewSize(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd) || IsIconic(hwnd) != FALSE) {
    return;
  }
  if (registrar_ == nullptr || registrar_->GetView() == nullptr) {
    return;
  }
  HWND view_hwnd = registrar_->GetView()->GetNativeWindow();
  if (view_hwnd == nullptr || view_hwnd == hwnd || !IsWindow(view_hwnd) ||
      GetParent(view_hwnd) != hwnd) {
    return;
  }
  RECT client{};
  if (!GetClientRect(hwnd, &client)) {
    return;
  }
  const int target_w = client.right - client.left;
  const int target_h = client.bottom - client.top;
  if (target_w <= 0 || target_h <= 0) {
    return;
  }
  RECT view_rect{};
  POINT top_left{0, 0};
  if (GetClientRect(view_hwnd, &view_rect) &&
      ClientToScreen(view_hwnd, &top_left) &&
      ScreenToClient(hwnd, &top_left)) {
    const int cur_w = view_rect.right - view_rect.left;
    const int cur_h = view_rect.bottom - view_rect.top;
    if (top_left.x == 0 && top_left.y == 0 && cur_w == target_w &&
        cur_h == target_h) {
      return;
    }
  }
  MoveWindow(view_hwnd, 0, 0, target_w, target_h, TRUE);
}

void XueHuaNaviteWindowManagerPlugin::FocusFlutterView(HWND hwnd) {
  if (registrar_ != nullptr && registrar_->GetView() != nullptr) {
    HWND view_hwnd = registrar_->GetView()->GetNativeWindow();
    if (view_hwnd != nullptr && IsWindow(view_hwnd)) {
      SetFocus(view_hwnd);
      return;
    }
  }
  if (hwnd != nullptr && IsWindow(hwnd)) {
    SetFocus(hwnd);
  }
}

void XueHuaNaviteWindowManagerPlugin::MarkReadyToShow(HWND hwnd) {
  SyncChildViewSize(hwnd);
  if (!IsWindowVisible(hwnd)) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
  } else {
    InvalidateRect(hwnd, nullptr, FALSE);
  }
  SetForegroundWindow(hwnd);
  FocusFlutterView(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::ApplyTitleBarStyle(
    HWND hwnd,
    const std::string& style) {
  title_bar_style_ = style;
  ApplyDwmFrameAndCorners(hwnd);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                   SWP_FRAMECHANGED);
  SyncChildViewSize(hwnd);
}

void XueHuaNaviteWindowManagerPlugin::ApplyDwmFrameAndCorners(HWND hwnd) {
  if (!is_fullscreen_) {
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
  }

  const bool custom_frame =
      !is_fullscreen_ &&
      (title_bar_style_ == "hidden" || title_bar_style_ == "immersive");

  DWORD corner_pref = kDwmwcpDefault;
  if (is_fullscreen_ || !has_shadow_) {
    corner_pref = kDwmwcpDoNotRound;
  } else if (custom_frame) {
    corner_pref = kDwmwcpRound;
  }
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_pref,
                        sizeof(corner_pref));

  // Extend the DWM frame by 1px at the top only for custom frameless modes
  // (`hidden` / `immersive`) when shadow is enabled; `normal` uses the standard
  // non-client caption/frame without bleeding DWM margins into the client area.
  MARGINS margins =
      (custom_frame && has_shadow_) ? MARGINS{0, 0, 1, 0} : MARGINS{0, 0, 0, 0};
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
  if (is_fullscreen_) {
    if (resizable) {
      pre_fullscreen_style_ |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
    } else {
      pre_fullscreen_style_ &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    return;
  }
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
        hwnd, 0, static_cast<BYTE>(std::lround(clamped * 255.0)), LWA_ALPHA);
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

}  // namespace xue_hua_navite_window_manager

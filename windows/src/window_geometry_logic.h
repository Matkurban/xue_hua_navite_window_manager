#ifndef FLUTTER_PLUGIN_XUE_HUA_WINDOW_GEOMETRY_LOGIC_H_
#define FLUTTER_PLUGIN_XUE_HUA_WINDOW_GEOMETRY_LOGIC_H_

#include <windows.h>

#include <algorithm>
#include <string>

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

/// True when a window must be restored to normal windowed state before applying
/// explicit windowed geometry (`configure`, `setBounds`, `setSize`,
/// `setPosition`, `center`).
///
/// On Win32, `SetWindowPos` does not clear `WS_MAXIMIZE` or `WS_MINIMIZE`, so
/// resizing a maximized window leaves `IsZoomed(hwnd)` true and causes
/// `WM_NCCALCSIZE` / DWM to treat the resized window as maximized.
inline bool ShouldRestoreWindowBeforeGeometry(bool has_geometry_request,
                                              bool is_maximized,
                                              bool is_minimized,
                                              bool is_fullscreen) {
  return has_geometry_request && (is_maximized || is_minimized || is_fullscreen);
}

/// Constrains a maximized custom-frame window's proposed `WM_NCCALCSIZE` screen
/// rectangle (`rgrc[0]`) to `monitor_work_rect` by intersection rather than raw
/// replacement.
///
/// For a normal maximized Win32 window whose outer rect overshoots the monitor
/// by the resize border (e.g. `(-8, -8, 1928, 1048)`), the intersection equals
/// `monitor_work_rect` (`(0, 0, 1920, 1040)`). If `window_rect` is ever smaller
/// than `monitor_work_rect`, clamping prevents negative client origins and
/// prevents the client area from exceeding the outer window bounds.
inline RECT ClampMaximizedClientRect(RECT window_rect, RECT monitor_work_rect) {
  RECT clamped{};
  clamped.left = (std::max)(window_rect.left, monitor_work_rect.left);
  clamped.top = (std::max)(window_rect.top, monitor_work_rect.top);
  clamped.right = (std::min)(window_rect.right, monitor_work_rect.right);
  clamped.bottom = (std::min)(window_rect.bottom, monitor_work_rect.bottom);
  if (clamped.right < clamped.left) {
    clamped.right = clamped.left;
  }
  if (clamped.bottom < clamped.top) {
    clamped.bottom = clamped.top;
  }
  return clamped;
}

}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_WINDOW_GEOMETRY_LOGIC_H_

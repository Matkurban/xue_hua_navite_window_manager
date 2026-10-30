#include "xue_hua_navite_window_manager_plugin.h"

#include <windowsx.h>

#include <algorithm>

#include "method_call_utils.h"

namespace xue_hua_navite_window_manager {

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
          // When maximized, intersect the proposed window rect with the
          // monitor's work area so the window does not spill off-screen or
          // cover an auto-hide taskbar, while guaranteeing the client rect
          // never exceeds the outer window bounds.
          HMONITOR monitor =
              MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
          MONITORINFO monitor_info{};
          monitor_info.cbSize = sizeof(MONITORINFO);
          if (GetMonitorInfoW(monitor, &monitor_info)) {
            params->rgrc[0] =
                ClampMaximizedClientRect(params->rgrc[0], monitor_info.rcWork);
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
                ? (std::max)(4, DipToPhysical(resize_border_dip_, scale))
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
            QueueWindowEvent("snapButtonHovered", hwnd);
          }
          return HTMAXBUTTON;
        } else if (is_snap_button_hovered_) {
          is_snap_button_hovered_ = false;
          QueueWindowEvent("snapButtonLeft", hwnd);
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

    case internal::kSnapButtonInvokeMessage: {
      snap_click_posted_ = false;
      if (snap_button_enabled_) {
        QueueWindowEvent("snapButtonClicked", hwnd);
        FlushQueuedWindowEvents();
      }
      return 0;
    }

    case internal::kFlushQueuedEventsMessage: {
      FlushQueuedWindowEvents();
      return 0;
    }

    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE: {
      if (is_snap_button_hovered_) {
        is_snap_button_hovered_ = false;
        QueueWindowEvent("snapButtonLeft", hwnd);
      }
      break;
    }

    // 5. Native Message-Loop Level Min/Max Size Enforcement (`WM_GETMINMAXINFO`)
    case WM_GETMINMAXINFO: {
      DefSubclassProc(hwnd, uMsg, wParam, lParam);
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
        QueueWindowEvent("closeRequested", hwnd);
        return 0;
      }
      break;
    }

    // 7. Window Lifecycle & Geometry Event Notifications
    // Let DefSubclassProc (Win32Window::MessageHandler) resize/focus the child
    // FlutterWindowsView first before snapshotting and queueing Dart events.
    case WM_SIZE: {
      const LRESULT def_result = DefSubclassProc(hwnd, uMsg, wParam, lParam);
      const bool currently_minimized = (wParam == SIZE_MINIMIZED);
      const bool currently_maximized = (wParam == SIZE_MAXIMIZED);

      if (!currently_minimized) {
        SyncChildViewSize(hwnd);
      }

      if (currently_minimized && !was_minimized_) {
        was_minimized_ = true;
        QueueWindowEvent("minimized", hwnd);
      } else if (!currently_minimized && was_minimized_) {
        was_minimized_ = false;
        QueueWindowEvent("restored", hwnd);
      }

      if (currently_maximized && !was_maximized_) {
        was_maximized_ = true;
        QueueWindowEvent("maximized", hwnd);
      } else if (!currently_maximized && was_maximized_ &&
                 !currently_minimized) {
        was_maximized_ = false;
        QueueWindowEvent("unmaximized", hwnd);
      }

      if (!currently_minimized) {
        QueueWindowEvent("resized", hwnd);
      }
      return def_result;
    }

    case WM_MOVE: {
      const LRESULT def_result = DefSubclassProc(hwnd, uMsg, wParam, lParam);
      if (!IsIconic(hwnd)) {
        QueueWindowEvent("moved", hwnd);
      }
      return def_result;
    }

    case WM_ACTIVATE: {
      const LRESULT def_result = DefSubclassProc(hwnd, uMsg, wParam, lParam);
      const WORD state = LOWORD(wParam);
      if (state == WA_ACTIVE || state == WA_CLICKACTIVE) {
        QueueWindowEvent("focused", hwnd);
      } else if (state == WA_INACTIVE) {
        QueueWindowEvent("blurred", hwnd);
      }
      return def_result;
    }

    case WM_NCDESTROY: {
      DetachSubclass();
      break;
    }
  }

  return DefSubclassProc(hwnd, uMsg, wParam, lParam);
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
  PostMessageW(hwnd, internal::kSnapButtonInvokeMessage, 0, 0);
}

}  // namespace xue_hua_navite_window_manager

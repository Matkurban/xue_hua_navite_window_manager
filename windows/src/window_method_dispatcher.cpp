#include "xue_hua_navite_window_manager_plugin.h"

#include "method_call_utils.h"

namespace xue_hua_navite_window_manager {

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

  if (method == "getLaunchAtStartupState") {
    result->Success(flutter::EncodableValue(QueryLaunchAtStartupState(args)));
    return;
  }

  if (method == "setLaunchAtStartup") {
    flutter::EncodableMap state_map;
    std::string error_code;
    std::string error_message;
    if (SetLaunchAtStartup(args, &state_map, &error_code, &error_message)) {
      result->Success(flutter::EncodableValue(state_map));
    } else {
      result->Error(error_code, error_message);
    }
    return;
  }

  if (method == "openLaunchAtStartupSettings") {
    result->Success(flutter::EncodableValue(OpenLaunchAtStartupSettings()));
    return;
  }

  if (method == "setLinuxInteractiveRegion") {
    // Linux-specific Wayland gesture region hit-test registration; no-op on Windows.
    result->Success();
    return;
  }

  if (method == "setSnapLayoutButtonRect") {
    const bool enabled = internal::GetBoolArg(args, "enabled").value_or(false);
    snap_button_enabled_ = enabled;
    if (enabled) {
      snap_button_rect_.x = internal::GetDoubleArg(args, "x").value_or(0.0);
      snap_button_rect_.y = internal::GetDoubleArg(args, "y").value_or(0.0);
      snap_button_rect_.width =
          internal::GetDoubleArg(args, "width").value_or(0.0);
      snap_button_rect_.height =
          internal::GetDoubleArg(args, "height").value_or(0.0);
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
          r.x = internal::GetDoubleArg(*rmap, "x").value_or(0.0);
          r.y = internal::GetDoubleArg(*rmap, "y").value_or(0.0);
          r.width = internal::GetDoubleArg(*rmap, "width").value_or(0.0);
          r.height = internal::GetDoubleArg(*rmap, "height").value_or(0.0);
          out->push_back(r);
        }
      }
    };

    parse_rects(args, "dragRects", &drag_rects_);
    parse_rects(args, "excludeRects", &exclude_rects_);
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
    const bool focus = internal::GetBoolArg(args, "focus").value_or(true);
    ShowWindow(hwnd, focus ? SW_SHOW : SW_SHOWNA);
    SyncChildViewSize(hwnd);
    if (focus) {
      SetForegroundWindow(hwnd);
      FocusFlutterView(hwnd);
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
    FocusFlutterView(hwnd);
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
    if (is_fullscreen_) {
      RestoreWindowForGeometry(hwnd);
    }
    ShowWindow(hwnd, SW_MAXIMIZE);
    SyncChildViewSize(hwnd);
    result->Success();
    return;
  }

  if (method == "unmaximize") {
    if (is_fullscreen_ || IsZoomed(hwnd) != FALSE) {
      RestoreWindowForGeometry(hwnd);
    }
    result->Success();
    return;
  }

  if (method == "minimize") {
    ShowWindow(hwnd, SW_MINIMIZE);
    result->Success();
    return;
  }

  if (method == "restore") {
    RestoreWindowForGeometry(hwnd);
    result->Success();
    return;
  }

  if (method == "setFullScreen") {
    const bool target_fs =
        internal::GetBoolArg(args, "isFullScreen").value_or(false);
    if (target_fs != is_fullscreen_) {
      if (target_fs) {
        pre_fullscreen_style_ = GetWindowLongPtrW(hwnd, GWL_STYLE);
        pre_fullscreen_ex_style_ = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        pre_fullscreen_placement_.length = sizeof(WINDOWPLACEMENT);
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
        SyncChildViewSize(hwnd);
        QueueWindowEvent("enteredFullScreen", hwnd);
      } else {
        is_fullscreen_ = false;
        SetWindowLongPtrW(hwnd, GWL_STYLE, pre_fullscreen_style_);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, pre_fullscreen_ex_style_);
        SetWindowPlacement(hwnd, &pre_fullscreen_placement_);
        ApplyDwmFrameAndCorners(hwnd);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        SyncChildViewSize(hwnd);
        QueueWindowEvent("exitedFullScreen", hwnd);
      }
    }
    result->Success();
    return;
  }

  if (method == "setBounds") {
    if (ShouldRestoreWindowBeforeGeometry(
            true, IsZoomed(hwnd) != FALSE, IsIconic(hwnd) != FALSE,
            is_fullscreen_)) {
      RestoreWindowForGeometry(hwnd);
    }
    const double scale = GetScaleFactor(hwnd);
    RECT current{};
    GetWindowRect(hwnd, &current);
    const int x =
        args.count(flutter::EncodableValue("x"))
            ? DipToPhysical(internal::GetDoubleArg(args, "x").value(), scale)
            : current.left;
    const int y =
        args.count(flutter::EncodableValue("y"))
            ? DipToPhysical(internal::GetDoubleArg(args, "y").value(), scale)
            : current.top;
    int w = current.right - current.left;
    int h = current.bottom - current.top;
    if (args.count(flutter::EncodableValue("width"))) {
      const double width =
          ClampDipAxis(internal::GetDoubleArg(args, "width").value(),
                       min_width_dip_, max_width_dip_);
      w = DipToPhysical(width, scale);
    }
    if (args.count(flutter::EncodableValue("height"))) {
      const double height =
          ClampDipAxis(internal::GetDoubleArg(args, "height").value(),
                       min_height_dip_, max_height_dip_);
      h = DipToPhysical(height, scale);
    }

    SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    SyncChildViewSize(hwnd);
    result->Success();
    return;
  }

  if (method == "getBounds") {
    result->Success(flutter::EncodableValue(GetBoundsMap(hwnd)));
    return;
  }

  if (method == "setSize") {
    if (ShouldRestoreWindowBeforeGeometry(
            true, IsZoomed(hwnd) != FALSE, IsIconic(hwnd) != FALSE,
            is_fullscreen_)) {
      RestoreWindowForGeometry(hwnd);
    }
    const double scale = GetScaleFactor(hwnd);
    double width = internal::GetDoubleArg(args, "width").value_or(800.0);
    double height = internal::GetDoubleArg(args, "height").value_or(600.0);
    ClampDipSize(min_width_dip_, min_height_dip_, max_width_dip_,
                 max_height_dip_, &width, &height);
    const int w = DipToPhysical(width, scale);
    const int h = DipToPhysical(height, scale);
    SetWindowPos(hwnd, nullptr, 0, 0, w, h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SyncChildViewSize(hwnd);
    result->Success();
    return;
  }

  if (method == "setPosition") {
    if (ShouldRestoreWindowBeforeGeometry(
            true, IsZoomed(hwnd) != FALSE, IsIconic(hwnd) != FALSE,
            is_fullscreen_)) {
      RestoreWindowForGeometry(hwnd);
    }
    const double scale = GetScaleFactor(hwnd);
    const int x =
        DipToPhysical(internal::GetDoubleArg(args, "x").value_or(0.0), scale);
    const int y =
        DipToPhysical(internal::GetDoubleArg(args, "y").value_or(0.0), scale);
    SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    result->Success();
    return;
  }

  if (method == "center") {
    if (ShouldRestoreWindowBeforeGeometry(
            true, IsZoomed(hwnd) != FALSE, IsIconic(hwnd) != FALSE,
            is_fullscreen_)) {
      RestoreWindowForGeometry(hwnd);
    }
    const double scale = GetScaleFactor(hwnd);
    RECT current{};
    GetWindowRect(hwnd, &current);
    int w = current.right - current.left;
    int h = current.bottom - current.top;
    if (auto width_opt = internal::GetDoubleArg(args, "width")) {
      w = DipToPhysical(
          ClampDipAxis(*width_opt, min_width_dip_, max_width_dip_), scale);
    }
    if (auto height_opt = internal::GetDoubleArg(args, "height")) {
      h = DipToPhysical(
          ClampDipAxis(*height_opt, min_height_dip_, max_height_dip_), scale);
    }
    RECT centered = ComputeCenteredPhysicalRect(hwnd, w, h);
    SetWindowPos(hwnd, nullptr, centered.left, centered.top,
                 centered.right - centered.left,
                 centered.bottom - centered.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SyncChildViewSize(hwnd);
    result->Success();
    return;
  }

  if (method == "setMinimumSize") {
    min_width_dip_ = internal::GetDoubleArg(args, "width").value_or(0.0);
    min_height_dip_ = internal::GetDoubleArg(args, "height").value_or(0.0);
    result->Success();
    return;
  }

  if (method == "setMaximumSize") {
    max_width_dip_ = internal::GetDoubleArg(args, "width").value_or(0.0);
    max_height_dip_ = internal::GetDoubleArg(args, "height").value_or(0.0);
    result->Success();
    return;
  }

  if (method == "setResizable") {
    ApplyResizable(hwnd, internal::GetBoolArg(args, "resizable").value_or(true));
    result->Success();
    return;
  }

  if (method == "setAlwaysOnTop") {
    ApplyAlwaysOnTop(
        hwnd, internal::GetBoolArg(args, "alwaysOnTop").value_or(false));
    result->Success();
    return;
  }

  if (method == "setPreventClose") {
    prevent_close_ =
        internal::GetBoolArg(args, "preventClose").value_or(false);
    result->Success();
    return;
  }

  if (method == "setTitle") {
    if (auto title = internal::GetStringArg(args, "title")) {
      std::wstring wide_title = internal::Utf8ToWide(*title);
      SetWindowTextW(hwnd, wide_title.c_str());
    }
    result->Success();
    return;
  }

  if (method == "setTitleBarStyle") {
    ApplyTitleBarStyle(
        hwnd, internal::GetStringArg(args, "titleBarStyle").value_or("normal"));
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
    if (auto argb = internal::GetUInt32Arg(args, "backgroundColor")) {
      ApplyBackgroundColor(hwnd, *argb);
    }
    result->Success();
    return;
  }

  if (method == "setOpacity") {
    if (auto opacity = internal::GetDoubleArg(args, "opacity")) {
      ApplyOpacity(hwnd, *opacity);
    }
    result->Success();
    return;
  }

  if (method == "setHasShadow") {
    has_shadow_ = internal::GetBoolArg(args, "hasShadow").value_or(true);
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
    if (auto edge = internal::GetStringArg(args, "edge")) {
      StartNativeResizing(hwnd, *edge);
    }
    result->Success();
    return;
  }

  result->NotImplemented();
}

}  // namespace xue_hua_navite_window_manager

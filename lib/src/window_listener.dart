import 'dart:ui';

import 'window_event.dart';

/// Mixin and interface for listening to native desktop window lifecycle and geometry events.
///
/// Register a [WindowListener] via `XueHuaNaviteWindowManager.instance.addListener(this)`
/// and remove it in `dispose()` via `XueHuaNaviteWindowManager.instance.removeListener(this)`.
mixin class WindowListener {
  /// Called on every raw [WindowEvent] emitted by the native platform.
  void onWindowEvent(WindowEvent event) {}

  /// Called when the window is resized.
  ///
  /// [size] is the new logical size in DIP.
  void onWindowResize(Size size) {}

  /// Called when the window is moved on screen.
  ///
  /// [position] is the new logical top-left coordinate in DIP.
  void onWindowMove(Offset position) {}

  /// Called when the window is maximized.
  void onWindowMaximize() {}

  /// Called when the window is restored from the maximized state.
  void onWindowUnmaximize() {}

  /// Called when the window is minimized to the taskbar or Dock.
  void onWindowMinimize() {}

  /// Called when the window is restored from the minimized state.
  void onWindowRestore() {}

  /// Called when the window enters native full-screen mode.
  void onWindowEnterFullScreen() {}

  /// Called when the window exits native full-screen mode.
  void onWindowLeaveFullScreen() {}

  /// Called when the window gains input focus (becomes the active/key window).
  void onWindowFocus() {}

  /// Called when the window loses input focus.
  void onWindowBlur() {}

  /// Called when the user attempts to close the window while `preventClose` is `true`.
  ///
  /// Use this callback to display an unsaved-changes confirmation dialog and then
  /// invoke `XueHuaNaviteWindowManager.instance.destroy()` to force-close, or
  /// `XueHuaNaviteWindowManager.instance.hide()` to minimize to the system tray.
  void onWindowCloseRequested() {}

  /// Called on Windows 11 when the cursor hovers over the registered `HTMAXBUTTON`
  /// Snap Layout region.
  void onSnapButtonHovered(bool isHovered) {}

  /// Called on Windows 11 when the user clicks the registered `HTMAXBUTTON` region.
  void onSnapButtonClicked() {}
}

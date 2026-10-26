import 'dart:ui';

import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'src/window_config.dart';
import 'src/window_enums.dart';
import 'src/window_event.dart';
import 'src/window_state_snapshot.dart';
import 'xue_hua_navite_window_manager_method_channel.dart';

/// The interface that platform-specific implementations of `xue_hua_navite_window_manager` must extend.
abstract class XueHuaNaviteWindowManagerPlatform extends PlatformInterface {
  /// Constructs a [XueHuaNaviteWindowManagerPlatform].
  XueHuaNaviteWindowManagerPlatform() : super(token: _token);

  static final Object _token = Object();

  static XueHuaNaviteWindowManagerPlatform _instance =
      MethodChannelXueHuaNaviteWindowManager();

  /// The default instance of [XueHuaNaviteWindowManagerPlatform] to use.
  static XueHuaNaviteWindowManagerPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [XueHuaNaviteWindowManagerPlatform] when
  /// they register themselves.
  static set instance(XueHuaNaviteWindowManagerPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  /// Broadcast stream of native [WindowEvent] notifications.
  Stream<WindowEvent> get windowEvents {
    throw UnimplementedError('windowEvents has not been implemented.');
  }

  /// Initializes the native window manager hook if not already initialized.
  Future<void> ensureInitialized() {
    throw UnimplementedError('ensureInitialized() has not been implemented.');
  }

  /// Atomically applies all non-null properties in [config] in a single native pass.
  Future<WindowStateSnapshot> configure(WindowConfig config) {
    throw UnimplementedError('configure() has not been implemented.');
  }

  /// Notifies the native host that the first Flutter frame is rendered and ready
  /// to be revealed without any black or white flash.
  Future<void> markReadyToShow() {
    throw UnimplementedError('markReadyToShow() has not been implemented.');
  }

  /// Retrieves a complete snapshot of the window's current geometry and state.
  Future<WindowStateSnapshot> getState() {
    throw UnimplementedError('getState() has not been implemented.');
  }

  /// Shows the window without necessarily stealing focus if [focus] is `false`.
  Future<void> show({bool focus = true}) {
    throw UnimplementedError('show() has not been implemented.');
  }

  /// Hides the window.
  Future<void> hide() {
    throw UnimplementedError('hide() has not been implemented.');
  }

  /// Brings the window to the front and gives it input focus.
  Future<void> focus() {
    throw UnimplementedError('focus() has not been implemented.');
  }

  /// Removes input focus from the window.
  Future<void> blur() {
    throw UnimplementedError('blur() has not been implemented.');
  }

  /// Requests the window to close (respects `preventClose`).
  Future<void> close() {
    throw UnimplementedError('close() has not been implemented.');
  }

  /// Force-closes and destroys the window immediately, bypassing `preventClose`.
  Future<void> destroy() {
    throw UnimplementedError('destroy() has not been implemented.');
  }

  /// Maximizes the window.
  Future<void> maximize() {
    throw UnimplementedError('maximize() has not been implemented.');
  }

  /// Restores the window from maximized state.
  Future<void> unmaximize() {
    throw UnimplementedError('unmaximize() has not been implemented.');
  }

  /// Minimizes the window to the taskbar or Dock.
  Future<void> minimize() {
    throw UnimplementedError('minimize() has not been implemented.');
  }

  /// Restores the window from minimized or maximized state.
  Future<void> restore() {
    throw UnimplementedError('restore() has not been implemented.');
  }

  /// Sets whether the window is in native full-screen mode.
  Future<void> setFullScreen(bool isFullScreen) {
    throw UnimplementedError('setFullScreen() has not been implemented.');
  }

  /// Sets the window bounds `(x, y, width, height)` in logical pixels (DIP).
  Future<void> setBounds(Rect bounds, {bool animate = false}) {
    throw UnimplementedError('setBounds() has not been implemented.');
  }

  /// Gets the current window bounds `(x, y, width, height)` in logical pixels (DIP).
  Future<Rect> getBounds() {
    throw UnimplementedError('getBounds() has not been implemented.');
  }

  /// Sets the window size `(width, height)` in logical pixels (DIP).
  Future<void> setSize(Size size, {bool animate = false}) {
    throw UnimplementedError('setSize() has not been implemented.');
  }

  /// Gets the current window size in logical pixels (DIP).
  Future<Size> getSize() {
    throw UnimplementedError('getSize() has not been implemented.');
  }

  /// Sets the window top-left position `(x, y)` in logical pixels (DIP).
  Future<void> setPosition(Offset position, {bool animate = false}) {
    throw UnimplementedError('setPosition() has not been implemented.');
  }

  /// Gets the current window top-left position in logical pixels (DIP).
  Future<Offset> getPosition() {
    throw UnimplementedError('getPosition() has not been implemented.');
  }

  /// Centers the window on the current screen.
  Future<void> center({Size? size, bool animate = false}) {
    throw UnimplementedError('center() has not been implemented.');
  }

  /// Sets the minimum window size in logical pixels (DIP).
  Future<void> setMinimumSize(Size size) {
    throw UnimplementedError('setMinimumSize() has not been implemented.');
  }

  /// Sets the maximum window size in logical pixels (DIP).
  Future<void> setMaximumSize(Size size) {
    throw UnimplementedError('setMaximumSize() has not been implemented.');
  }

  /// Sets whether the user can resize the window.
  Future<void> setResizable(bool resizable) {
    throw UnimplementedError('setResizable() has not been implemented.');
  }

  /// Sets whether the window stays on top of other windows.
  Future<void> setAlwaysOnTop(bool alwaysOnTop) {
    throw UnimplementedError('setAlwaysOnTop() has not been implemented.');
  }

  /// Sets whether closing the window is intercepted.
  Future<void> setPreventClose(bool preventClose) {
    throw UnimplementedError('setPreventClose() has not been implemented.');
  }

  /// Sets the window title text.
  Future<void> setTitle(String title) {
    throw UnimplementedError('setTitle() has not been implemented.');
  }

  /// Sets the window title bar style (`normal`, `hidden`, or `immersive`).
  Future<void> setTitleBarStyle(
    TitleBarStyle style, {
    Offset? trafficLightsOffset,
    bool? trafficLightsVisible,
  }) {
    throw UnimplementedError('setTitleBarStyle() has not been implemented.');
  }

  /// Sets the macOS traffic light buttons `(x, y)` offset from the top-left corner.
  Future<void> setTrafficLightsOffset(Offset offset) {
    throw UnimplementedError(
      'setTrafficLightsOffset() has not been implemented.',
    );
  }

  /// Sets whether the macOS traffic light buttons are visible.
  Future<void> setTrafficLightsVisible(bool visible) {
    throw UnimplementedError(
      'setTrafficLightsVisible() has not been implemented.',
    );
  }

  /// Sets the native window background color (used for zero-flash startup and resize background fill).
  Future<void> setBackgroundColor(Color color) {
    throw UnimplementedError('setBackgroundColor() has not been implemented.');
  }

  /// Sets the window opacity (`0.0` transparent to `1.0` opaque).
  Future<void> setOpacity(double opacity) {
    throw UnimplementedError('setOpacity() has not been implemented.');
  }

  /// Sets whether the native window shadow is enabled.
  Future<void> setHasShadow(bool hasShadow) {
    throw UnimplementedError('setHasShadow() has not been implemented.');
  }

  /// Initiates an OS-level window drag operation from the current pointer event.
  Future<void> startDragging() {
    throw UnimplementedError('startDragging() has not been implemented.');
  }

  /// Initiates an OS-level window edge/corner resize operation for [edge].
  Future<void> startResizing(ResizeEdge edge) {
    throw UnimplementedError('startResizing() has not been implemented.');
  }

  /// Updates the logical bounding box of the custom maximize button on Windows 11
  /// so native `WM_NCHITTEST` returns `HTMAXBUTTON` to trigger Windows 11 Snap Layouts.
  ///
  /// Pass `null` to clear the registered rect.
  Future<void> setSnapLayoutButtonRect(Rect? rect) {
    throw UnimplementedError(
      'setSnapLayoutButtonRect() has not been implemented.',
    );
  }

  /// Updates the list of interactive non-draggable exclusion rectangles inside
  /// a custom title bar (for Windows `WM_NCHITTEST` if needed).
  Future<void> setTitleBarDragRects({
    required List<Rect> dragRects,
    List<Rect> excludeRects = const <Rect>[],
  }) {
    throw UnimplementedError(
      'setTitleBarDragRects() has not been implemented.',
    );
  }
}

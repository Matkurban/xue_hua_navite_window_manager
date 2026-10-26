import 'dart:async';
import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/widgets.dart';

import '../xue_hua_navite_window_manager_platform_interface.dart';
import 'window_config.dart';
import 'window_enums.dart';
import 'window_event.dart';
import 'window_listener.dart';
import 'window_state_snapshot.dart';

/// High-performance singleton controller for desktop window management across
/// macOS, Windows, and Linux.
///
/// Named to match the plugin package `xue_hua_navite_window_manager`.
/// Use [XueHuaNaviteWindowManager.configure] or `XueHuaNaviteWindowManager.instance.configureWindow`
/// during `main()` initialization to perform an atomic, single-channel window setup
/// with zero startup black/white flash.
class XueHuaNaviteWindowManager {
  XueHuaNaviteWindowManager._();

  /// Singleton instance of [XueHuaNaviteWindowManager].
  static final XueHuaNaviteWindowManager instance =
      XueHuaNaviteWindowManager._();

  XueHuaNaviteWindowManagerPlatform get _platform =>
      XueHuaNaviteWindowManagerPlatform.instance;

  final ObserverList<WindowListener> _listeners =
      ObserverList<WindowListener>();
  StreamSubscription<WindowEvent>? _eventSubscription;
  bool _isInitialized = false;
  WindowStateSnapshot? _lastSnapshot;

  /// Latest cached [WindowStateSnapshot] updated automatically on configure and events.
  WindowStateSnapshot? get lastSnapshot => _lastSnapshot;

  /// Broadcast stream of native [WindowEvent] notifications.
  Stream<WindowEvent> get events => _platform.windowEvents;

  /// Initializes the native window hook and subscribes to native lifecycle events.
  Future<void> ensureInitialized() async {
    WidgetsFlutterBinding.ensureInitialized();
    if (_isInitialized) return;
    _isInitialized = true;
    _startListeningIfNeeded();
    await _platform.ensureInitialized();
  }

  void _startListeningIfNeeded() {
    _eventSubscription ??= _platform.windowEvents.listen(_handleNativeEvent);
  }

  void _handleNativeEvent(WindowEvent event) {
    if (_lastSnapshot != null) {
      _lastSnapshot = WindowStateSnapshot(
        bounds: event.bounds ?? _lastSnapshot!.bounds,
        isVisible: _lastSnapshot!.isVisible,
        isMaximized:
            event.isMaximized ??
            (event.type == WindowEventType.maximized
                ? true
                : event.type == WindowEventType.unmaximized
                ? false
                : _lastSnapshot!.isMaximized),
        isMinimized: event.type == WindowEventType.minimized
            ? true
            : event.type == WindowEventType.restored
            ? false
            : _lastSnapshot!.isMinimized,
        isFullScreen:
            event.isFullScreen ??
            (event.type == WindowEventType.enteredFullScreen
                ? true
                : event.type == WindowEventType.exitedFullScreen
                ? false
                : _lastSnapshot!.isFullScreen),
        isFocused: event.type == WindowEventType.focused
            ? true
            : event.type == WindowEventType.blurred
            ? false
            : _lastSnapshot!.isFocused,
        isAlwaysOnTop: _lastSnapshot!.isAlwaysOnTop,
        isResizable: _lastSnapshot!.isResizable,
        isPreventClose: _lastSnapshot!.isPreventClose,
      );
    }

    final List<WindowListener> localListeners = List<WindowListener>.of(
      _listeners,
    );
    for (final WindowListener listener in localListeners) {
      if (!_listeners.contains(listener)) continue;
      listener.onWindowEvent(event);
      switch (event.type) {
        case WindowEventType.resized:
          if (event.bounds != null) {
            listener.onWindowResize(event.bounds!.size);
          }
        case WindowEventType.moved:
          if (event.bounds != null) {
            listener.onWindowMove(event.bounds!.topLeft);
          }
        case WindowEventType.maximized:
          listener.onWindowMaximize();
        case WindowEventType.unmaximized:
          listener.onWindowUnmaximize();
        case WindowEventType.minimized:
          listener.onWindowMinimize();
        case WindowEventType.restored:
          listener.onWindowRestore();
        case WindowEventType.enteredFullScreen:
          listener.onWindowEnterFullScreen();
        case WindowEventType.exitedFullScreen:
          listener.onWindowLeaveFullScreen();
        case WindowEventType.focused:
          listener.onWindowFocus();
        case WindowEventType.blurred:
          listener.onWindowBlur();
        case WindowEventType.closeRequested:
          listener.onWindowCloseRequested();
        case WindowEventType.snapButtonHovered:
          listener.onSnapButtonHovered(true);
        case WindowEventType.snapButtonLeft:
          listener.onSnapButtonHovered(false);
        case WindowEventType.snapButtonClicked:
          listener.onSnapButtonClicked();
      }
    }
  }

  /// Registers a [WindowListener] to receive window lifecycle and geometry callbacks.
  void addListener(WindowListener listener) {
    _startListeningIfNeeded();
    _listeners.add(listener);
  }

  /// Removes a previously registered [WindowListener].
  void removeListener(WindowListener listener) {
    _listeners.remove(listener);
  }

  /// Atomically configures the desktop window in a single platform call and
  /// coordinates zero-flash presentation after the first Flutter frame renders.
  ///
  /// When `config.showWhenReady` is `true` (default), the native window applies all
  /// size, constraint, centering, background color, and frameless styling properties
  /// off-screen or while hidden, and reveals the window only after Flutter's
  /// `SchedulerBinding.instance.addPostFrameCallback` fires for the first frame.
  static Future<WindowStateSnapshot> configure(WindowConfig config) {
    return instance.configureWindow(config);
  }

  /// Instance method equivalent of [XueHuaNaviteWindowManager.configure].
  Future<WindowStateSnapshot> configureWindow(WindowConfig config) async {
    await ensureInitialized();

    // Send the atomic configuration payload without revealing immediately until
    // the first Flutter frame is guaranteed to be rasterized.
    final WindowConfig preFrameConfig = config.copyWith(showWhenReady: false);
    final WindowStateSnapshot snapshot = await _platform.configure(
      preFrameConfig,
    );
    _lastSnapshot = snapshot;

    if (config.showWhenReady) {
      final Completer<void> frameCompleter = Completer<void>();
      SchedulerBinding.instance.addPostFrameCallback((_) {
        if (!frameCompleter.isCompleted) {
          frameCompleter.complete();
        }
      });
      SchedulerBinding.instance.ensureVisualUpdate();
      // Do not block forever if called before runApp; reveal as soon as first frame completes.
      unawaited(
        frameCompleter.future.then((_) async {
          await _platform.markReadyToShow();
        }),
      );
    }

    return snapshot;
  }

  /// Queries a fresh [WindowStateSnapshot] from the native platform.
  Future<WindowStateSnapshot> getState() async {
    final WindowStateSnapshot state = await _platform.getState();
    _lastSnapshot = state;
    return state;
  }

  /// Shows the window, optionally bringing it to the foreground with input focus.
  Future<void> show({bool focus = true}) => _platform.show(focus: focus);

  /// Hides the window from the screen.
  Future<void> hide() => _platform.hide();

  /// Focuses the window.
  Future<void> focus() => _platform.focus();

  /// Removes focus from the window.
  Future<void> blur() => _platform.blur();

  /// Requests the window to close (triggers `onWindowCloseRequested` if `preventClose` is enabled).
  Future<void> close() => _platform.close();

  /// Immediately destroys the window, bypassing `preventClose`.
  Future<void> destroy() => _platform.destroy();

  /// Maximizes the window.
  Future<void> maximize() => _platform.maximize();

  /// Restores the window from maximized state.
  Future<void> unmaximize() => _platform.unmaximize();

  /// Toggles between maximized and unmaximized states.
  Future<void> toggleMaximize() async {
    final WindowStateSnapshot state = await getState();
    if (state.isMaximized) {
      await unmaximize();
    } else {
      await maximize();
    }
  }

  /// Minimizes the window to the taskbar or Dock.
  Future<void> minimize() => _platform.minimize();

  /// Restores the window from minimized state.
  Future<void> restore() => _platform.restore();

  /// Enters or exits native full-screen mode.
  Future<void> setFullScreen(bool isFullScreen) =>
      _platform.setFullScreen(isFullScreen);

  /// Sets the window bounds `(x, y, width, height)` in logical pixels (DIP).
  Future<void> setBounds(Rect bounds, {bool animate = false}) =>
      _platform.setBounds(bounds, animate: animate);

  /// Returns the current window bounds in logical pixels (DIP).
  Future<Rect> getBounds() => _platform.getBounds();

  /// Sets the window size `(width, height)` in logical pixels (DIP).
  Future<void> setSize(Size size, {bool animate = false}) =>
      _platform.setSize(size, animate: animate);

  /// Returns the current window size in logical pixels (DIP).
  Future<Size> getSize() => _platform.getSize();

  /// Sets the window position `(x, y)` in logical pixels (DIP).
  Future<void> setPosition(Offset position, {bool animate = false}) =>
      _platform.setPosition(position, animate: animate);

  /// Returns the current window position in logical pixels (DIP).
  Future<Offset> getPosition() => _platform.getPosition();

  /// Centers the window on the active monitor, optionally resizing it atomically first.
  Future<void> center({Size? size, bool animate = false}) =>
      _platform.center(size: size, animate: animate);

  /// Sets the minimum window size in logical pixels (DIP).
  Future<void> setMinimumSize(Size size) => _platform.setMinimumSize(size);

  /// Sets the maximum window size in logical pixels (DIP).
  Future<void> setMaximumSize(Size size) => _platform.setMaximumSize(size);

  /// Enables or disables user window resizing.
  Future<void> setResizable(bool resizable) =>
      _platform.setResizable(resizable);

  /// Sets whether the window floats on top of normal windows.
  Future<void> setAlwaysOnTop(bool alwaysOnTop) =>
      _platform.setAlwaysOnTop(alwaysOnTop);

  /// Enables or disables close interception.
  Future<void> setPreventClose(bool preventClose) =>
      _platform.setPreventClose(preventClose);

  /// Updates the window title.
  Future<void> setTitle(String title) => _platform.setTitle(title);

  /// Updates the window title bar style and optional macOS traffic light parameters.
  Future<void> setTitleBarStyle(
    TitleBarStyle style, {
    Offset? trafficLightsOffset,
    bool? trafficLightsVisible,
  }) => _platform.setTitleBarStyle(
    style,
    trafficLightsOffset: trafficLightsOffset,
    trafficLightsVisible: trafficLightsVisible,
  );

  /// Dynamically repositions the macOS traffic light buttons (`close`, `miniaturize`, `zoom`).
  Future<void> setTrafficLightsOffset(Offset offset) =>
      _platform.setTrafficLightsOffset(offset);

  /// Shows or hides the macOS traffic light buttons.
  Future<void> setTrafficLightsVisible(bool visible) =>
      _platform.setTrafficLightsVisible(visible);

  /// Synchronizes the native window background color to prevent black edges during resize.
  Future<void> setBackgroundColor(Color color) =>
      _platform.setBackgroundColor(color);

  /// Sets the overall window opacity (`0.0` to `1.0`).
  Future<void> setOpacity(double opacity) => _platform.setOpacity(opacity);

  /// Enables or disables the native window drop shadow.
  Future<void> setHasShadow(bool hasShadow) =>
      _platform.setHasShadow(hasShadow);

  /// Starts an OS-level window drag operation immediately.
  Future<void> startDragging() => _platform.startDragging();

  /// Starts an OS-level window resize operation along [edge].
  Future<void> startResizing(ResizeEdge edge) => _platform.startResizing(edge);

  /// Registers the bounding box of a custom maximize button (in logical window coordinates)
  /// so Windows 11 `WM_NCHITTEST` returns `HTMAXBUTTON` and displays the native Snap Layouts flyout.
  Future<void> setSnapLayoutButtonRect(Rect? rect) =>
      _platform.setSnapLayoutButtonRect(rect);

  /// Updates the custom draggable title bar rectangles and interactive exclusion rectangles.
  Future<void> setTitleBarDragRects({
    required List<Rect> dragRects,
    List<Rect> excludeRects = const <Rect>[],
  }) => _platform.setTitleBarDragRects(
    dragRects: dragRects,
    excludeRects: excludeRects,
  );
}

/// Type alias for backward compatibility with architectural specifications referencing `NativeWindowController`.
typedef NativeWindowController = XueHuaNaviteWindowManager;

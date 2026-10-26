import 'dart:ui';

import 'window_enums.dart';

/// Atomic configuration model for initializing or updating the native desktop window
/// in a single platform channel invocation.
///
/// Using [WindowConfig] with `NativeWindowController.instance.configure(config)`
/// computes target geometry (size, constraints, and centered coordinates) and window
/// styles in memory on the native thread before applying a single atomic window frame
/// update, eliminating startup flashes and window jumping.
class WindowConfig {
  /// Initial or target logical size (width, height) in device-independent pixels (DIP).
  final Size? size;

  /// Minimum allowed logical window size (enforced in `WM_GETMINMAXINFO` on Windows,
  /// `NSWindow.minSize` on macOS, and `gdk_window_set_geometry_hints` on Linux).
  final Size? minSize;

  /// Maximum allowed logical window size (enforced in `WM_GETMINMAXINFO` on Windows,
  /// `NSWindow.maxSize` on macOS, and `gdk_window_set_geometry_hints` on Linux).
  final Size? maxSize;

  /// Explicit logical screen position `(x, y)` of the window's top-left corner.
  /// Ignored if [center] is `true`.
  final Offset? position;

  /// Whether to center the window on the active monitor after applying [size].
  final bool center;

  /// Window title text displayed in the title bar and taskbar/Dock.
  final String? title;

  /// Visual style of the window title bar (`normal`, `hidden`, or `immersive`).
  final TitleBarStyle? titleBarStyle;

  /// Custom `(x, y)` offset from the top-left corner of the window for the macOS
  /// traffic light buttons (`closeButton`, `miniaturizeButton`, `zoomButton`).
  ///
  /// - `dx`: Horizontal padding from the left edge of the window (in logical points).
  /// - `dy`: Vertical padding from the top edge of the window (in logical points).
  final Offset? trafficLightsOffset;

  /// Whether the macOS traffic light buttons are visible when using `hidden` or
  /// `immersive` title bar styles. Defaults to `true` (or `false` when `immersive`
  /// unless explicitly set).
  final bool? trafficLightsVisible;

  /// Native window background color painted during startup (`WM_ERASEBKGND` on Windows,
  /// `NSWindow.backgroundColor` on macOS, `gdk_window` background on Linux) and during
  /// rapid window edge resizing to prevent black/white tearing artifacts.
  final Color? backgroundColor;

  /// Whether the window should float above all other non-topmost windows.
  final bool? alwaysOnTop;

  /// Whether the user can resize the window by dragging its edges or corners.
  final bool? resizable;

  /// Whether closing the window is intercepted so Dart can show a confirmation dialog
  /// or hide the window to the system tray. When `true`, attempting to close emits
  /// [WindowEventType.closeRequested].
  final bool? preventClose;

  /// Whether to automatically reveal and focus the window once the first Flutter frame
  /// has rendered and the atomic geometry/style configuration has been applied.
  final bool showWhenReady;

  /// Whether to preserve the native DWM drop shadow on Windows when [titleBarStyle]
  /// is `hidden` or `immersive`. Defaults to `true`.
  final bool? hasShadow;

  /// Logical width in DIP of the custom resize border hit-test zone on frameless
  /// Windows/Linux windows. Defaults to `6.0`.
  final double? resizeBorderWidth;

  /// Creates an atomic [WindowConfig].
  const WindowConfig({
    this.size,
    this.minSize,
    this.maxSize,
    this.position,
    this.center = false,
    this.title,
    this.titleBarStyle,
    this.trafficLightsOffset,
    this.trafficLightsVisible,
    this.backgroundColor,
    this.alwaysOnTop,
    this.resizable,
    this.preventClose,
    this.showWhenReady = true,
    this.hasShadow,
    this.resizeBorderWidth,
  });

  /// Creates a copy of this [WindowConfig] with selective overrides.
  WindowConfig copyWith({
    Size? size,
    Size? minSize,
    Size? maxSize,
    Offset? position,
    bool? center,
    String? title,
    TitleBarStyle? titleBarStyle,
    Offset? trafficLightsOffset,
    bool? trafficLightsVisible,
    Color? backgroundColor,
    bool? alwaysOnTop,
    bool? resizable,
    bool? preventClose,
    bool? showWhenReady,
    bool? hasShadow,
    double? resizeBorderWidth,
  }) {
    return WindowConfig(
      size: size ?? this.size,
      minSize: minSize ?? this.minSize,
      maxSize: maxSize ?? this.maxSize,
      position: position ?? this.position,
      center: center ?? this.center,
      title: title ?? this.title,
      titleBarStyle: titleBarStyle ?? this.titleBarStyle,
      trafficLightsOffset: trafficLightsOffset ?? this.trafficLightsOffset,
      trafficLightsVisible: trafficLightsVisible ?? this.trafficLightsVisible,
      backgroundColor: backgroundColor ?? this.backgroundColor,
      alwaysOnTop: alwaysOnTop ?? this.alwaysOnTop,
      resizable: resizable ?? this.resizable,
      preventClose: preventClose ?? this.preventClose,
      showWhenReady: showWhenReady ?? this.showWhenReady,
      hasShadow: hasShadow ?? this.hasShadow,
      resizeBorderWidth: resizeBorderWidth ?? this.resizeBorderWidth,
    );
  }

  /// Converts this configuration into a serializable map for the platform channel.
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      if (size != null) 'width': size!.width,
      if (size != null) 'height': size!.height,
      if (minSize != null) 'minWidth': minSize!.width,
      if (minSize != null) 'minHeight': minSize!.height,
      if (maxSize != null) 'maxWidth': maxSize!.width,
      if (maxSize != null) 'maxHeight': maxSize!.height,
      if (position != null) 'x': position!.dx,
      if (position != null) 'y': position!.dy,
      'center': center,
      if (title != null) 'title': title,
      if (titleBarStyle != null) 'titleBarStyle': titleBarStyle!.wireValue,
      if (trafficLightsOffset != null) ...<String, dynamic>{
        'trafficLightsOffsetX': trafficLightsOffset!.dx,
        'trafficLightsOffsetY': trafficLightsOffset!.dy,
      },
      if (trafficLightsVisible != null)
        'trafficLightsVisible': trafficLightsVisible,
      if (backgroundColor != null)
        'backgroundColor': backgroundColor!.toARGB32(),
      if (alwaysOnTop != null) 'alwaysOnTop': alwaysOnTop,
      if (resizable != null) 'resizable': resizable,
      if (preventClose != null) 'preventClose': preventClose,
      'showWhenReady': showWhenReady,
      if (hasShadow != null) 'hasShadow': hasShadow,
      if (resizeBorderWidth != null) 'resizeBorderWidth': resizeBorderWidth,
    };
  }

  @override
  String toString() {
    return 'WindowConfig(size: $size, minSize: $minSize, maxSize: $maxSize, '
        'position: $position, center: $center, title: $title, '
        'titleBarStyle: $titleBarStyle, trafficLightsOffset: $trafficLightsOffset, '
        'backgroundColor: $backgroundColor, alwaysOnTop: $alwaysOnTop, '
        'resizable: $resizable, preventClose: $preventClose, '
        'showWhenReady: $showWhenReady)';
  }
}

/// Snapshot of the current native window geometry and state flags.
class WindowStateSnapshot {
  /// Current window bounding rectangle in logical screen coordinates (DIP).
  final Rect bounds;

  /// Whether the window is currently visible on screen.
  final bool isVisible;

  /// Whether the window is currently maximized (zoomed).
  final bool isMaximized;

  /// Whether the window is currently minimized (iconified).
  final bool isMinimized;

  /// Whether the window is currently in native full-screen mode.
  final bool isFullScreen;

  /// Whether the window currently has input focus.
  final bool isFocused;

  /// Whether the window is floating above normal windows.
  final bool isAlwaysOnTop;

  /// Whether the user can resize the window.
  final bool isResizable;

  /// Whether window close interception is currently active.
  final bool isPreventClose;

  /// Creates a [WindowStateSnapshot].
  const WindowStateSnapshot({
    required this.bounds,
    required this.isVisible,
    required this.isMaximized,
    required this.isMinimized,
    required this.isFullScreen,
    required this.isFocused,
    required this.isAlwaysOnTop,
    required this.isResizable,
    required this.isPreventClose,
  });

  /// Constructs a [WindowStateSnapshot] from a platform channel dictionary.
  factory WindowStateSnapshot.fromMap(Map<Object?, Object?> map) {
    final double x = (map['x'] as num?)?.toDouble() ?? 0.0;
    final double y = (map['y'] as num?)?.toDouble() ?? 0.0;
    final double width = (map['width'] as num?)?.toDouble() ?? 800.0;
    final double height = (map['height'] as num?)?.toDouble() ?? 600.0;

    return WindowStateSnapshot(
      bounds: Rect.fromLTWH(x, y, width, height),
      isVisible: (map['isVisible'] as bool?) ?? true,
      isMaximized: (map['isMaximized'] as bool?) ?? false,
      isMinimized: (map['isMinimized'] as bool?) ?? false,
      isFullScreen: (map['isFullScreen'] as bool?) ?? false,
      isFocused: (map['isFocused'] as bool?) ?? true,
      isAlwaysOnTop: (map['isAlwaysOnTop'] as bool?) ?? false,
      isResizable: (map['isResizable'] as bool?) ?? true,
      isPreventClose: (map['isPreventClose'] as bool?) ?? false,
    );
  }

  /// Converts this snapshot into a serializable map.
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      'x': bounds.left,
      'y': bounds.top,
      'width': bounds.width,
      'height': bounds.height,
      'isVisible': isVisible,
      'isMaximized': isMaximized,
      'isMinimized': isMinimized,
      'isFullScreen': isFullScreen,
      'isFocused': isFocused,
      'isAlwaysOnTop': isAlwaysOnTop,
      'isResizable': isResizable,
      'isPreventClose': isPreventClose,
    };
  }
}

/// Strongly typed event payload delivered by the native window event stream.
class WindowEvent {
  /// The specific window event type.
  final WindowEventType type;

  /// Current window bounds `(x, y, width, height)` in logical pixels at the time of the event, if available.
  final Rect? bounds;

  /// Whether the window is currently maximized, if included in the event payload.
  final bool? isMaximized;

  /// Whether the window is currently in full-screen mode, if included in the event payload.
  final bool? isFullScreen;

  /// Timestamp (milliseconds since epoch) when the event was generated.
  final int timestamp;

  /// Creates a [WindowEvent].
  const WindowEvent({
    required this.type,
    this.bounds,
    this.isMaximized,
    this.isFullScreen,
    required this.timestamp,
  });

  /// Parses a [WindowEvent] from a raw map received over the native `EventChannel`.
  static WindowEvent? fromMap(Map<Object?, Object?> map) {
    final String? typeRaw = map['type'] as String?;
    final WindowEventType? eventType = WindowEventType.fromWire(typeRaw);
    if (eventType == null) return null;

    Rect? bounds;
    if (map['width'] != null && map['height'] != null) {
      final double x = (map['x'] as num?)?.toDouble() ?? 0.0;
      final double y = (map['y'] as num?)?.toDouble() ?? 0.0;
      final double width = (map['width'] as num).toDouble();
      final double height = (map['height'] as num).toDouble();
      bounds = Rect.fromLTWH(x, y, width, height);
    }

    return WindowEvent(
      type: eventType,
      bounds: bounds,
      isMaximized: map['isMaximized'] as bool?,
      isFullScreen: map['isFullScreen'] as bool?,
      timestamp:
          (map['timestamp'] as int?) ?? DateTime.now().millisecondsSinceEpoch,
    );
  }

  @override
  String toString() =>
      'WindowEvent(type: ${type.wireValue}, bounds: $bounds, isMaximized: $isMaximized, isFullScreen: $isFullScreen)';
}

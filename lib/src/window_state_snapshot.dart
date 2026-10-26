import 'dart:ui';

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

  @override
  String toString() {
    return 'WindowStateSnapshot(bounds: $bounds, isVisible: $isVisible, '
        'isMaximized: $isMaximized, isMinimized: $isMinimized, '
        'isFullScreen: $isFullScreen, isFocused: $isFocused, '
        'isAlwaysOnTop: $isAlwaysOnTop, isResizable: $isResizable, '
        'isPreventClose: $isPreventClose)';
  }
}

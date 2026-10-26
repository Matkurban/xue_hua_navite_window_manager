/// Window title bar style options across macOS, Windows, and Linux.
enum TitleBarStyle {
  /// Standard operating system title bar with native caption and controls.
  normal('normal'),

  /// Hidden system title bar while preserving native border, shadow,
  /// rounded corners (Windows 11 DWM), and window control buttons (macOS traffic lights).
  hidden('hidden'),

  /// Immersive full-size content view where the client area extends into the title bar
  /// and system controls can be hidden or customized.
  immersive('immersive');

  const TitleBarStyle(this.wireValue);

  /// String value transmitted over the platform channel.
  final String wireValue;

  /// Parses a [TitleBarStyle] from its platform channel string representation.
  static TitleBarStyle fromWire(String? value) {
    return TitleBarStyle.values.firstWhere(
      (e) => e.wireValue == value,
      orElse: () => TitleBarStyle.normal,
    );
  }
}

/// Window resize edge or corner directions for programmatic edge resizing.
enum ResizeEdge {
  /// Top edge (`HTTOP` on Windows, `GDK_WINDOW_EDGE_NORTH` on Linux).
  top('top'),

  /// Bottom edge (`HTBOTTOM` on Windows, `GDK_WINDOW_EDGE_SOUTH` on Linux).
  bottom('bottom'),

  /// Left edge (`HTLEFT` on Windows, `GDK_WINDOW_EDGE_WEST` on Linux).
  left('left'),

  /// Right edge (`HTRIGHT` on Windows, `GDK_WINDOW_EDGE_EAST` on Linux).
  right('right'),

  /// Top-left corner (`HTTOPLEFT` on Windows, `GDK_WINDOW_EDGE_NORTH_WEST` on Linux).
  topLeft('topLeft'),

  /// Top-right corner (`HTTOPRIGHT` on Windows, `GDK_WINDOW_EDGE_NORTH_EAST` on Linux).
  topRight('topRight'),

  /// Bottom-left corner (`HTBOTTOMLEFT` on Windows, `GDK_WINDOW_EDGE_SOUTH_WEST` on Linux).
  bottomLeft('bottomLeft'),

  /// Bottom-right corner (`HTBOTTOMRIGHT` on Windows, `GDK_WINDOW_EDGE_SOUTH_EAST` on Linux).
  bottomRight('bottomRight');

  const ResizeEdge(this.wireValue);

  /// String value transmitted over the platform channel.
  final String wireValue;

  /// Parses a [ResizeEdge] from its platform channel string representation.
  static ResizeEdge? fromWire(String? value) {
    if (value == null) return null;
    for (final edge in ResizeEdge.values) {
      if (edge.wireValue == value) return edge;
    }
    return null;
  }
}

/// Window lifecycle and geometry event types emitted by the native platform.
enum WindowEventType {
  /// Emitted when the window has been resized.
  resized('resized'),

  /// Emitted when the window has been moved on screen.
  moved('moved'),

  /// Emitted when the window is maximized.
  maximized('maximized'),

  /// Emitted when the window is restored from maximized state.
  unmaximized('unmaximized'),

  /// Emitted when the window is minimized to the taskbar/Dock.
  minimized('minimized'),

  /// Emitted when the window is restored from minimized state.
  restored('restored'),

  /// Emitted when the window enters native full-screen mode.
  enteredFullScreen('enteredFullScreen'),

  /// Emitted when the window exits native full-screen mode.
  exitedFullScreen('exitedFullScreen'),

  /// Emitted when the window gains input focus (becomes key window).
  focused('focused'),

  /// Emitted when the window loses input focus.
  blurred('blurred'),

  /// Emitted when the user attempts to close the window while `preventClose` is enabled.
  closeRequested('closeRequested'),

  /// Emitted when the mouse enters the registered Windows 11 Snap Layout maximize button rect.
  snapButtonHovered('snapButtonHovered'),

  /// Emitted when the mouse leaves the registered Windows 11 Snap Layout maximize button rect.
  snapButtonLeft('snapButtonLeft'),

  /// Emitted when the registered Windows 11 Snap Layout maximize button is clicked via `WM_NCLBUTTONUP`.
  snapButtonClicked('snapButtonClicked');

  const WindowEventType(this.wireValue);

  /// String value transmitted over the platform EventChannel.
  final String wireValue;

  /// Parses a [WindowEventType] from its platform channel string representation.
  static WindowEventType? fromWire(String? value) {
    if (value == null) return null;
    for (final type in WindowEventType.values) {
      if (type.wireValue == value) return type;
    }
    return null;
  }
}

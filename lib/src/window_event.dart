import 'dart:ui';

import 'window_enums.dart';

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

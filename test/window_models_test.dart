import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  group('WindowConfig', () {
    test('has the documented defaults', () {
      const WindowConfig config = WindowConfig();

      expect(config.center, isFalse);
      expect(config.showWhenReady, isTrue);
      expect(config.size, isNull);
      expect(config.titleBarStyle, isNull);
      expect(config.preventClose, isNull);
    });

    test('toMap sends only what was set, plus center and showWhenReady', () {
      expect(const WindowConfig().toMap(), <String, dynamic>{
        'center': false,
        'showWhenReady': true,
      });
    });

    test('toMap sends every field under its wire key', () {
      const WindowConfig config = WindowConfig(
        size: Size(1280, 800),
        minSize: Size(640, 480),
        maxSize: Size(2560, 1440),
        position: Offset(10, 20),
        center: true,
        title: 'Title',
        titleBarStyle: TitleBarStyle.immersive,
        trafficLightsOffset: Offset(16, 14),
        trafficLightsVisible: false,
        backgroundColor: Color(0xFF123456),
        alwaysOnTop: true,
        resizable: false,
        preventClose: true,
        showWhenReady: false,
        hasShadow: false,
        resizeBorderWidth: 8,
      );

      expect(config.toMap(), <String, dynamic>{
        'width': 1280.0,
        'height': 800.0,
        'minWidth': 640.0,
        'minHeight': 480.0,
        'maxWidth': 2560.0,
        'maxHeight': 1440.0,
        'x': 10.0,
        'y': 20.0,
        'center': true,
        'title': 'Title',
        'titleBarStyle': 'immersive',
        'trafficLightsOffsetX': 16.0,
        'trafficLightsOffsetY': 14.0,
        'trafficLightsVisible': false,
        'backgroundColor': 0xFF123456,
        'alwaysOnTop': true,
        'resizable': false,
        'preventClose': true,
        'showWhenReady': false,
        'hasShadow': false,
        'resizeBorderWidth': 8.0,
      });
    });

    test('toMap keeps false booleans, which are not the same as unset', () {
      final Map<String, dynamic> map = const WindowConfig(
        alwaysOnTop: false,
        resizable: false,
        preventClose: false,
        hasShadow: false,
        trafficLightsVisible: false,
      ).toMap();

      expect(map['alwaysOnTop'], isFalse);
      expect(map['resizable'], isFalse);
      expect(map['preventClose'], isFalse);
      expect(map['hasShadow'], isFalse);
      expect(map['trafficLightsVisible'], isFalse);
    });

    test('copyWith overrides the given fields and keeps the others', () {
      const WindowConfig original = WindowConfig(
        size: Size(800, 600),
        minSize: Size(400, 300),
        title: 'Original',
        titleBarStyle: TitleBarStyle.hidden,
        alwaysOnTop: true,
        resizeBorderWidth: 6,
      );

      final WindowConfig copy = original.copyWith(
        size: const Size(1000, 700),
        title: 'Copy',
        showWhenReady: false,
        center: true,
      );

      expect(copy.size, const Size(1000, 700));
      expect(copy.title, 'Copy');
      expect(copy.showWhenReady, isFalse);
      expect(copy.center, isTrue);
      expect(copy.minSize, const Size(400, 300));
      expect(copy.titleBarStyle, TitleBarStyle.hidden);
      expect(copy.alwaysOnTop, isTrue);
      expect(copy.resizeBorderWidth, 6);
      // The original is immutable.
      expect(original.size, const Size(800, 600));
      expect(original.showWhenReady, isTrue);
    });

    test('copyWith covers every field', () {
      const WindowConfig base = WindowConfig();
      final WindowConfig copy = base.copyWith(
        size: const Size(1, 2),
        minSize: const Size(3, 4),
        maxSize: const Size(5, 6),
        position: const Offset(7, 8),
        center: true,
        title: 't',
        titleBarStyle: TitleBarStyle.immersive,
        trafficLightsOffset: const Offset(9, 10),
        trafficLightsVisible: true,
        backgroundColor: const Color(0xFF000001),
        alwaysOnTop: true,
        resizable: false,
        preventClose: true,
        showWhenReady: false,
        hasShadow: false,
        resizeBorderWidth: 11,
      );

      expect(copy.toMap().keys, hasLength(21));
    });

    test('toString names the main fields', () {
      final String text = const WindowConfig(
        size: Size(800, 600),
        title: 'Studio',
        titleBarStyle: TitleBarStyle.hidden,
        preventClose: true,
      ).toString();

      expect(text, contains('Size(800.0, 600.0)'));
      expect(text, contains('Studio'));
      expect(text, contains('TitleBarStyle.hidden'));
      expect(text, contains('preventClose: true'));
    });
  });

  group('WindowEvent.fromMap', () {
    test('parses the type, bounds, flags, and timestamp', () {
      final WindowEvent? event = WindowEvent.fromMap(<Object?, Object?>{
        'type': 'resized',
        'x': 10,
        'y': 20.5,
        'width': 640,
        'height': 480.0,
        'isMaximized': true,
        'isFullScreen': false,
        'timestamp': 1234,
      });

      expect(event, isNotNull);
      expect(event!.type, WindowEventType.resized);
      expect(event.bounds, const Rect.fromLTWH(10, 20.5, 640, 480));
      expect(event.isMaximized, isTrue);
      expect(event.isFullScreen, isFalse);
      expect(event.timestamp, 1234);
      expect(event.toString(), contains('resized'));
    });

    test('has no bounds without a width and a height', () {
      final WindowEvent? event = WindowEvent.fromMap(<Object?, Object?>{
        'type': 'focused',
        'width': 640,
        'timestamp': 1,
      });

      expect(event!.type, WindowEventType.focused);
      expect(event.bounds, isNull);
      expect(event.isMaximized, isNull);
      expect(event.isFullScreen, isNull);
    });

    test('defaults a missing position to the origin', () {
      final WindowEvent? event = WindowEvent.fromMap(<Object?, Object?>{
        'type': 'moved',
        'width': 300,
        'height': 200,
        'timestamp': 1,
      });

      expect(event!.bounds, const Rect.fromLTWH(0, 0, 300, 200));
    });

    test('fills in a timestamp when the platform sends none', () {
      final int before = DateTime.now().millisecondsSinceEpoch;
      final WindowEvent? event = WindowEvent.fromMap(<Object?, Object?>{
        'type': 'blurred',
      });

      expect(event!.timestamp, greaterThanOrEqualTo(before));
    });

    test('returns null for an unknown or missing type', () {
      expect(
        WindowEvent.fromMap(<Object?, Object?>{'type': 'noSuchEvent'}),
        isNull,
      );
      expect(WindowEvent.fromMap(<Object?, Object?>{}), isNull);
    });
  });

  group('WindowStateSnapshot', () {
    test('fromMap reads every field', () {
      final WindowStateSnapshot snapshot = WindowStateSnapshot.fromMap(
        <Object?, Object?>{
          'x': 1,
          'y': 2,
          'width': 300,
          'height': 200.5,
          'isVisible': false,
          'isMaximized': true,
          'isMinimized': true,
          'isFullScreen': true,
          'isFocused': false,
          'isAlwaysOnTop': true,
          'isResizable': false,
          'isPreventClose': true,
          'isPositionAuthoritative': false,
        },
      );

      expect(snapshot.bounds, const Rect.fromLTWH(1, 2, 300, 200.5));
      expect(snapshot.isVisible, isFalse);
      expect(snapshot.isMaximized, isTrue);
      expect(snapshot.isMinimized, isTrue);
      expect(snapshot.isFullScreen, isTrue);
      expect(snapshot.isFocused, isFalse);
      expect(snapshot.isAlwaysOnTop, isTrue);
      expect(snapshot.isResizable, isFalse);
      expect(snapshot.isPreventClose, isTrue);
      expect(snapshot.isPositionAuthoritative, isFalse);
    });

    test('fromMap falls back to a plain 800x600 visible window', () {
      final WindowStateSnapshot snapshot = WindowStateSnapshot.fromMap(
        const <Object?, Object?>{},
      );

      expect(snapshot.bounds, const Rect.fromLTWH(0, 0, 800, 600));
      expect(snapshot.isVisible, isTrue);
      expect(snapshot.isMaximized, isFalse);
      expect(snapshot.isMinimized, isFalse);
      expect(snapshot.isFullScreen, isFalse);
      expect(snapshot.isFocused, isTrue);
      expect(snapshot.isAlwaysOnTop, isFalse);
      expect(snapshot.isResizable, isTrue);
      expect(snapshot.isPreventClose, isFalse);
      expect(snapshot.isPositionAuthoritative, isTrue);
    });

    test('toMap and fromMap round trip', () {
      const WindowStateSnapshot original = WindowStateSnapshot(
        bounds: Rect.fromLTWH(5, 6, 700, 500),
        isVisible: true,
        isMaximized: false,
        isMinimized: true,
        isFullScreen: false,
        isFocused: true,
        isAlwaysOnTop: true,
        isResizable: false,
        isPreventClose: true,
        isPositionAuthoritative: false,
      );

      final WindowStateSnapshot copy = WindowStateSnapshot.fromMap(
        original.toMap(),
      );

      expect(copy.toMap(), original.toMap());
      expect(copy.bounds, original.bounds);
    });

    test('toString lists the flags', () {
      const WindowStateSnapshot snapshot = WindowStateSnapshot(
        bounds: Rect.fromLTWH(0, 0, 10, 10),
        isVisible: true,
        isMaximized: false,
        isMinimized: false,
        isFullScreen: false,
        isFocused: true,
        isAlwaysOnTop: false,
        isResizable: true,
        isPreventClose: false,
      );

      expect(snapshot.toString(), contains('isVisible: true'));
      expect(snapshot.toString(), contains('isPositionAuthoritative: true'));
    });
  });

  group('wire values', () {
    test('TitleBarStyle round trips and falls back to normal', () {
      for (final TitleBarStyle style in TitleBarStyle.values) {
        expect(TitleBarStyle.fromWire(style.wireValue), style);
      }
      expect(TitleBarStyle.fromWire('hidden'), TitleBarStyle.hidden);
      expect(TitleBarStyle.fromWire('unknown'), TitleBarStyle.normal);
      expect(TitleBarStyle.fromWire(null), TitleBarStyle.normal);
    });

    test('ResizeEdge round trips and returns null for unknown values', () {
      for (final ResizeEdge edge in ResizeEdge.values) {
        expect(ResizeEdge.fromWire(edge.wireValue), edge);
      }
      expect(ResizeEdge.fromWire('bottomRight'), ResizeEdge.bottomRight);
      expect(ResizeEdge.fromWire('diagonal'), isNull);
      expect(ResizeEdge.fromWire(null), isNull);
    });

    test('WindowEventType round trips and returns null for unknown values', () {
      for (final WindowEventType type in WindowEventType.values) {
        expect(WindowEventType.fromWire(type.wireValue), type);
      }
      expect(WindowEventType.fromWire('exitedFullScreen'), isNotNull);
      expect(WindowEventType.fromWire('exploded'), isNull);
      expect(WindowEventType.fromWire(null), isNull);
    });

    test('the wire values are stable and unique', () {
      // The native plugins match on these strings, so renaming one is a
      // breaking change that this list is meant to catch.
      expect(
        TitleBarStyle.values.map((TitleBarStyle s) => s.wireValue),
        <String>['normal', 'hidden', 'immersive'],
      );
      expect(ResizeEdge.values.map((ResizeEdge e) => e.wireValue), <String>[
        'top',
        'bottom',
        'left',
        'right',
        'topLeft',
        'topRight',
        'bottomLeft',
        'bottomRight',
      ]);
      expect(
        WindowEventType.values.map((WindowEventType t) => t.wireValue),
        <String>[
          'resized',
          'moved',
          'maximized',
          'unmaximized',
          'minimized',
          'restored',
          'enteredFullScreen',
          'exitedFullScreen',
          'focused',
          'blurred',
          'closeRequested',
          'snapButtonHovered',
          'snapButtonLeft',
          'snapButtonClicked',
        ],
      );
    });
  });

  group('WindowListener', () {
    test('every callback is a harmless no-op by default', () {
      final WindowListener listener = WindowListener();

      listener.onWindowEvent(
        const WindowEvent(type: WindowEventType.focused, timestamp: 1),
      );
      listener.onWindowResize(const Size(1, 1));
      listener.onWindowMove(Offset.zero);
      listener.onWindowMaximize();
      listener.onWindowUnmaximize();
      listener.onWindowMinimize();
      listener.onWindowRestore();
      listener.onWindowEnterFullScreen();
      listener.onWindowLeaveFullScreen();
      listener.onWindowFocus();
      listener.onWindowBlur();
      listener.onWindowCloseRequested();
      listener.onSnapButtonHovered(true);
      listener.onSnapButtonClicked();
    });
  });
}

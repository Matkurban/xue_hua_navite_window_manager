import 'package:flutter/gestures.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const MethodChannel channel = MethodChannel(
    'xue_hua_navite_window_manager/methods',
  );
  final List<MethodCall> log = <MethodCall>[];
  bool windowIsMaximized = false;

  /// Calls that DragArea and WindowResizeEdgeArea make for the user's gesture.
  /// The Linux region bookkeeping they also publish is not part of that.
  List<String> gestureCalls() => log
      .where((MethodCall call) => call.method != 'setLinuxInteractiveRegion')
      .map(
        (MethodCall call) => call.method == 'startResizing'
            ? 'startResizing:${(call.arguments as Map<Object?, Object?>)['edge']}'
            : call.method,
      )
      .toList();

  setUp(() {
    log.clear();
    windowIsMaximized = false;
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
          log.add(call);
          if (call.method == 'getState') {
            return <String, Object?>{
              'x': 0.0,
              'y': 0.0,
              'width': 800.0,
              'height': 600.0,
              'isMaximized': windowIsMaximized,
            };
          }
          return null;
        });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  Future<void> doubleTap(WidgetTester tester, Finder finder) async {
    await tester.tap(finder);
    await tester.pump(const Duration(milliseconds: 60));
    await tester.tap(finder);
    await tester.pumpAndSettle(const Duration(milliseconds: 400));
  }

  group('DragArea', () {
    Widget area({
      bool doubleClickToMaximize = true,
      VoidCallback? onDragStart,
    }) => Directionality(
      textDirection: TextDirection.ltr,
      child: Center(
        child: DragArea(
          key: const Key('area'),
          doubleClickToMaximize: doubleClickToMaximize,
          onDragStart: onDragStart,
          child: const SizedBox(width: 300, height: 60),
        ),
      ),
    );

    testWidgets('a pan starts a native drag and reports onDragStart', (
      WidgetTester tester,
    ) async {
      int dragStarts = 0;
      await tester.pumpWidget(area(onDragStart: () => dragStarts++));

      await tester.drag(find.byKey(const Key('area')), const Offset(50, 0));
      // The double tap recognizer inside DragArea keeps a timer for 300 ms.
      await tester.pump(const Duration(seconds: 1));

      expect(dragStarts, 1);
      expect(gestureCalls(), contains('startDragging'));
    });

    testWidgets('a double tap maximizes a normal window', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(area());

      await doubleTap(tester, find.byKey(const Key('area')));

      expect(gestureCalls(), <String>['getState', 'maximize']);
    });

    testWidgets('a double tap restores a maximized window', (
      WidgetTester tester,
    ) async {
      windowIsMaximized = true;
      await tester.pumpWidget(area());

      await doubleTap(tester, find.byKey(const Key('area')));

      expect(gestureCalls(), <String>['getState', 'unmaximize']);
    });

    testWidgets('doubleClickToMaximize false ignores double taps', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(area(doubleClickToMaximize: false));

      await doubleTap(tester, find.byKey(const Key('area')));

      expect(gestureCalls(), isNot(contains('maximize')));
      expect(gestureCalls(), isNot(contains('unmaximize')));
    });
  });

  group('WindowResizeEdgeArea', () {
    Widget edges({bool enabled = true, double edgeThickness = 6}) =>
        Directionality(
          textDirection: TextDirection.ltr,
          child: SizedBox(
            width: 400,
            height: 300,
            child: WindowResizeEdgeArea(
              enabled: enabled,
              edgeThickness: edgeThickness,
              child: const ColoredBox(color: Color(0xFF336699)),
            ),
          ),
        );

    Future<void> press(
      WidgetTester tester,
      Offset position, {
      int buttons = kPrimaryMouseButton,
    }) async {
      final TestGesture gesture = await tester.startGesture(
        position,
        kind: PointerDeviceKind.mouse,
        buttons: buttons,
      );
      await gesture.up();
    }

    testWidgets('each edge and corner asks for its own resize', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(edges());
      final Rect area = tester.getRect(find.byType(WindowResizeEdgeArea));

      // The edges are 6 pixels thick and the corners 12x12.
      final Map<ResizeEdge, Offset> presses = <ResizeEdge, Offset>{
        ResizeEdge.top: Offset(area.center.dx, area.top + 3),
        ResizeEdge.bottom: Offset(area.center.dx, area.bottom - 3),
        ResizeEdge.left: Offset(area.left + 3, area.center.dy),
        ResizeEdge.right: Offset(area.right - 3, area.center.dy),
        ResizeEdge.topLeft: Offset(area.left + 6, area.top + 6),
        ResizeEdge.topRight: Offset(area.right - 6, area.top + 6),
        ResizeEdge.bottomLeft: Offset(area.left + 6, area.bottom - 6),
        ResizeEdge.bottomRight: Offset(area.right - 6, area.bottom - 6),
      };
      expect(presses.keys.toSet(), ResizeEdge.values.toSet());

      for (final MapEntry<ResizeEdge, Offset> entry in presses.entries) {
        log.clear();
        await press(tester, entry.value);
        expect(gestureCalls(), <String>[
          'startResizing:${entry.key.wireValue}',
        ], reason: '${entry.key}');
      }
    });

    testWidgets('only the primary mouse button resizes', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(edges());
      final Rect area = tester.getRect(find.byType(WindowResizeEdgeArea));

      await press(
        tester,
        Offset(area.center.dx, area.top + 3),
        buttons: kSecondaryMouseButton,
      );

      expect(gestureCalls(), isEmpty);
    });

    testWidgets('the middle of the child is not an edge', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(edges());

      await press(tester, tester.getCenter(find.byType(WindowResizeEdgeArea)));

      expect(gestureCalls(), isEmpty);
    });

    testWidgets('a disabled area has no edges', (WidgetTester tester) async {
      await tester.pumpWidget(edges(enabled: false));
      final Rect area = tester.getRect(find.byType(WindowResizeEdgeArea));

      await press(tester, Offset(area.center.dx, area.top + 3));

      expect(gestureCalls(), isEmpty);
    });

    testWidgets('edgeThickness sets how far the hit zone reaches', (
      WidgetTester tester,
    ) async {
      await tester.pumpWidget(edges(edgeThickness: 20));
      final Rect area = tester.getRect(find.byType(WindowResizeEdgeArea));

      // 15 pixels in is inside a 20 pixel edge but outside the default 6.
      await press(tester, Offset(area.center.dx, area.top + 15));

      expect(gestureCalls(), <String>['startResizing:top']);
    });
  });
}

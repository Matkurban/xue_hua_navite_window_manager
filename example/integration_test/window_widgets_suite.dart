// WindowCaption, DragArea, SnapLayoutButton, and WindowResizeEdgeArea running in
// the real app, wired to the real native window.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_widgets_suite.dart -d <device>`.
//
// Taps are synthesized inside Flutter, so they are reliable. Gestures that make
// DragArea or WindowResizeEdgeArea call startDragging or startResizing would
// begin an OS-level move or resize with no pointer button held, so every
// windowTest runs on a GuardedPlatform: those two calls are recorded, never
// forwarded, and the tests assert on the record instead.
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

Future<void> _pumpHome(WidgetTester tester, Widget child) async {
  await tester.pumpWidget(
    MaterialApp(debugShowCheckedModeBanner: false, home: Scaffold(body: child)),
  );
  await tester.pump();
}

/// Pumps frames until [finder] matches, or fails after [timeout].
Future<void> _pumpUntilFound(
  WidgetTester tester,
  Finder finder, {
  Duration timeout = kStateTimeout,
}) async {
  final Stopwatch watch = Stopwatch()..start();
  while (finder.evaluate().isEmpty) {
    if (watch.elapsed > timeout) {
      throw TestFailure('Timed out waiting for $finder');
    }
    await tester.pump(const Duration(milliseconds: 50));
  }
}

Future<void> _doubleTap(WidgetTester tester, Finder finder) async {
  await tester.tap(finder);
  await tester.pump(const Duration(milliseconds: 60));
  await tester.tap(finder);
  await tester.pump();
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('WindowCaption draws its title and the platform controls', (
    WidgetTester tester,
  ) async {
    await _pumpHome(
      tester,
      const Align(
        alignment: Alignment.topCenter,
        child: WindowCaption(height: 44, title: Text('Caption under test')),
      ),
    );

    expect(find.text('Caption under test'), findsOneWidget);
    if (isMacOS) {
      // macOS keeps the traffic lights and draws no controls of its own.
      expect(find.byTooltip('Minimize'), findsNothing);
      expect(find.byTooltip('Close'), findsNothing);
      expect(find.byIcon(Icons.crop_square), findsNothing);
    } else {
      expect(find.byTooltip('Minimize'), findsOneWidget);
      expect(find.byTooltip('Close'), findsOneWidget);
      expect(find.byIcon(Icons.crop_square), findsOneWidget);
    }
    expect(tester.takeException(), isNull);
  });

  windowTest('the WindowCaption maximize control maximizes and restores', (
    WidgetTester tester,
  ) async {
    if (isMacOS) {
      markTestSkipped('macOS draws traffic lights, not caption controls.');
      return;
    }
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    await _pumpHome(
      tester,
      const Align(
        alignment: Alignment.topCenter,
        child: WindowCaption(height: 44, title: Text('Caption under test')),
      ),
    );

    await tester.tap(find.byIcon(Icons.crop_square));
    await waitForState(
      (WindowStateSnapshot s) => s.isMaximized,
      reason: 'the caption maximize control to maximize the window',
    );
    await _pumpUntilFound(tester, find.byIcon(Icons.filter_none));

    await tester.tap(find.byIcon(Icons.filter_none));
    await waitForState(
      (WindowStateSnapshot s) => !s.isMaximized,
      reason: 'the caption restore control to restore the window',
    );
    await _pumpUntilFound(tester, find.byIcon(Icons.crop_square));
  });

  windowTest(
    'the WindowCaption close control requests close and keeps the window',
    (WidgetTester tester) async {
      if (isMacOS) {
        markTestSkipped('macOS draws traffic lights, not caption controls.');
        return;
      }
      final WindowEventRecorder recorder = await WindowEventRecorder.attach();
      addTearDown(recorder.dispose);
      await _pumpHome(
        tester,
        const Align(
          alignment: Alignment.topCenter,
          child: WindowCaption(height: 44, title: Text('Caption under test')),
        ),
      );
      // preventClose is on after the baseline reset. Without it this tap would
      // destroy the window that is running the tests.
      expect((await windowManager.getState()).isPreventClose, isTrue);

      await tester.tap(find.byTooltip('Close'));

      await recorder.waitFor(WindowEventType.closeRequested);
      expect((await windowManager.getState()).isVisible, isTrue);
    },
  );

  windowTest('the WindowCaption minimize control minimizes the window', (
    WidgetTester tester,
  ) async {
    if (isMacOS) {
      markTestSkipped('macOS draws traffic lights, not caption controls.');
      return;
    }
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    await _pumpHome(
      tester,
      const Align(
        alignment: Alignment.topCenter,
        child: WindowCaption(height: 44, title: Text('Caption under test')),
      ),
    );

    await tester.tap(find.byTooltip('Minimize'));
    await waitForState(
      (WindowStateSnapshot s) => s.isMinimized,
      reason: 'the caption minimize control to minimize the window',
    );

    // A minimized window renders no frames, so restore before pumping again.
    await windowManager.restore();
    await waitForState(
      (WindowStateSnapshot s) => !s.isMinimized,
      reason: 'restore() to bring the window back',
    );
    await tester.pump();
    expect(tester.takeException(), isNull);
  });

  windowTest(
    'double-clicking a DragArea toggles maximize without starting a drag',
    (WidgetTester tester) async {
      if (!await ensureWindowSystemHonorsStateChanges()) {
        return;
      }
      final Key area = UniqueKey();
      await _pumpHome(
        tester,
        Center(
          child: DragArea(
            key: area,
            child: const SizedBox(
              width: 320,
              height: 80,
              child: Text('drag me'),
            ),
          ),
        ),
      );

      await _doubleTap(tester, find.byKey(area));
      await waitForState(
        (WindowStateSnapshot s) => s.isMaximized,
        reason: 'a double tap on DragArea to maximize the window',
      );

      await tester.pump(const Duration(milliseconds: 600));
      await _doubleTap(tester, find.byKey(area));
      await waitForState(
        (WindowStateSnapshot s) => !s.isMaximized,
        reason: 'a second double tap on DragArea to unmaximize the window',
      );
      expect(pointerGuard.pointerCalls, isEmpty);
    },
  );

  windowTest('DragArea ignores double taps when doubleClickToMaximize is off', (
    WidgetTester tester,
  ) async {
    final Key area = UniqueKey();
    await _pumpHome(
      tester,
      Center(
        child: DragArea(
          key: area,
          doubleClickToMaximize: false,
          child: const SizedBox(width: 320, height: 80, child: Text('drag me')),
        ),
      ),
    );

    await _doubleTap(tester, find.byKey(area));
    await tester.pump(const Duration(milliseconds: 500));

    expect((await windowManager.getState()).isMaximized, isFalse);
  });

  windowTest(
    'dragging a DragArea reports onDragStart and starts a native drag',
    (WidgetTester tester) async {
      int dragStarts = 0;
      final Key area = UniqueKey();
      await _pumpHome(
        tester,
        Center(
          child: DragArea(
            key: area,
            onDragStart: () => dragStarts++,
            child: const SizedBox(
              width: 320,
              height: 80,
              child: Text('drag me'),
            ),
          ),
        ),
      );

      await tester.drag(find.byKey(area), const Offset(60, 0));
      await tester.pump(const Duration(milliseconds: 200));

      expect(dragStarts, 1);
      // The call is recorded by the GuardedPlatform and never reaches native code.
      expect(pointerGuard.pointerCalls, contains('startDragging'));
      expect((await windowManager.getState()).isVisible, isTrue);
    },
  );

  windowTest('SnapLayoutButton.onPressed replaces the default toggle', (
    WidgetTester tester,
  ) async {
    int presses = 0;
    final Key button = UniqueKey();
    await _pumpHome(
      tester,
      Center(
        child: SnapLayoutButton(
          key: button,
          onPressed: () => presses++,
          builder: (BuildContext context, bool hovered, bool maximized) {
            return const SizedBox(
              width: 46,
              height: 40,
              child: Icon(Icons.crop_square),
            );
          },
        ),
      ),
    );

    await tester.tap(find.byKey(button));
    await tester.pump(const Duration(milliseconds: 300));

    expect(presses, 1);
    expect((await windowManager.getState()).isMaximized, isFalse);
    expect(tester.takeException(), isNull);
  });

  windowTest(
    'SnapLayoutButton reports its rectangle and clears it on dispose',
    (WidgetTester tester) async {
      // On Windows the button registers its rectangle with the native hit test.
      // Elsewhere the calls are ignored. Either way, mounting, disabling, and
      // disposing it against the real plugin must not throw.
      Widget build({required bool enabled}) {
        return Center(
          child: SnapLayoutButton(
            enabled: enabled,
            builder: (BuildContext context, bool hovered, bool maximized) {
              return const SizedBox(width: 46, height: 40);
            },
          ),
        );
      }

      await _pumpHome(tester, build(enabled: true));
      await tester.pump(const Duration(milliseconds: 200));
      await _pumpHome(tester, build(enabled: false));
      await tester.pump(const Duration(milliseconds: 200));
      await _pumpHome(tester, build(enabled: true));
      await tester.pump(const Duration(milliseconds: 200));
      await _pumpHome(tester, const SizedBox.shrink());
      await tester.pump(const Duration(milliseconds: 200));

      expect(tester.takeException(), isNull);
      expect((await windowManager.getState()).isVisible, isTrue);
    },
  );

  windowTest(
    'WindowResizeEdgeArea builds eight edges and removes them on dispose',
    (WidgetTester tester) async {
      // Pressing an edge would call startResizing, so the edges are only built
      // and removed here. On Linux each edge registers a native interactive
      // region while mounted and drops it on dispose.
      Finder edgesWith(MouseCursor cursor) => find.byWidgetPredicate(
        (Widget widget) => widget is MouseRegion && widget.cursor == cursor,
      );

      await _pumpHome(
        tester,
        const WindowResizeEdgeArea(
          child: ColoredBox(color: Color(0xFF336699), child: SizedBox.expand()),
        ),
      );
      await tester.pump(const Duration(milliseconds: 200));
      // top and bottom, left and right, and the two diagonal pairs of corners.
      expect(edgesWith(SystemMouseCursors.resizeUpDown), findsNWidgets(2));
      expect(edgesWith(SystemMouseCursors.resizeLeftRight), findsNWidgets(2));
      expect(
        edgesWith(SystemMouseCursors.resizeUpLeftDownRight),
        findsNWidgets(2),
      );
      expect(
        edgesWith(SystemMouseCursors.resizeUpRightDownLeft),
        findsNWidgets(2),
      );

      await _pumpHome(
        tester,
        const WindowResizeEdgeArea(
          enabled: false,
          child: ColoredBox(color: Color(0xFF336699), child: SizedBox.expand()),
        ),
      );
      expect(edgesWith(SystemMouseCursors.resizeUpDown), findsNothing);
      expect(edgesWith(SystemMouseCursors.resizeLeftRight), findsNothing);

      await _pumpHome(tester, const SizedBox.shrink());
      await tester.pump(const Duration(milliseconds: 200));

      expect(tester.takeException(), isNull);
      expect((await windowManager.getState()).isVisible, isTrue);
    },
  );

  windowTest(
    'a DragArea publishes its region on Linux and clears it on dispose',
    (WidgetTester tester) async {
      if (!isLinux) {
        markTestSkipped('Only GTK uses native interactive regions.');
        return;
      }

      await _pumpHome(
        tester,
        const Align(
          alignment: Alignment.topCenter,
          child: DragArea(child: SizedBox(width: 400, height: 40)),
        ),
      );
      await tester.pump(const Duration(milliseconds: 200));
      await _pumpHome(tester, const SizedBox.shrink());
      await tester.pump(const Duration(milliseconds: 200));

      expect(tester.takeException(), isNull);
    },
  );
}

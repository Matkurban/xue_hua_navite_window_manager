// alwaysOnTop, preventClose, title, title bar styles, opacity, background color,
// shadow, and the platform-specific calls (macOS traffic lights, Windows Snap
// Layouts and drag rectangles, Linux interactive regions), exercised against
// the real native window of the example app.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_behavior_suite.dart -d <device>`.
//
// Not covered, because they need a real pointer press or end the process:
//  * startDragging / startResizing begin an operating-system move or resize
//    that follows the pointer. Without a pressed button they do nothing on
//    macOS and Linux, and on Windows they enter a modal loop that waits for
//    mouse input and would stall the test run.
//  * destroy() closes the window and ends the process running the tests.
//  * close() with preventClose off would do the same, so close() is only ever
//    called through requestCloseSafely().
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('setAlwaysOnTop updates isAlwaysOnTop and can be reversed', (
    WidgetTester tester,
  ) async {
    await windowManager.setAlwaysOnTop(true);
    final WindowStateSnapshot floating = await waitForState(
      (WindowStateSnapshot s) => s.isAlwaysOnTop,
      reason: 'setAlwaysOnTop(true) to be reported',
    );
    expect(floating.isVisible, isTrue);

    await windowManager.setAlwaysOnTop(false);
    await waitForState(
      (WindowStateSnapshot s) => !s.isAlwaysOnTop,
      reason: 'setAlwaysOnTop(false) to be reported',
    );
  });

  windowTest('setAlwaysOnTop does not change the window geometry', (
    WidgetTester tester,
  ) async {
    final Rect before = await waitForStableBounds();

    await windowManager.setAlwaysOnTop(true);
    await waitForState(
      (WindowStateSnapshot s) => s.isAlwaysOnTop,
      reason: 'setAlwaysOnTop(true) to be reported',
    );

    final Rect after = await waitForStableBounds();
    expectSizeNear(after.size, before.size);
    if (await positionsAreAuthoritative()) {
      expectOffsetNear(after.topLeft, before.topLeft);
    }
  });

  windowTest('setPreventClose updates isPreventClose and can be reversed', (
    WidgetTester tester,
  ) async {
    // preventClose goes off and straight back on. While it is off, close()
    // would destroy this window, so nothing in between asks for one.
    await windowManager.setPreventClose(false);
    await waitForState(
      (WindowStateSnapshot s) => !s.isPreventClose,
      reason: 'setPreventClose(false) to be reported',
    );

    await windowManager.setPreventClose(true);
    await waitForState(
      (WindowStateSnapshot s) => s.isPreventClose,
      reason: 'setPreventClose(true) to be reported',
    );
  });

  windowTest('requestCloseSafely refuses to close when preventClose is off', (
    WidgetTester tester,
  ) async {
    await windowManager.setPreventClose(false);
    await waitForState(
      (WindowStateSnapshot s) => !s.isPreventClose,
      reason: 'setPreventClose(false) to be reported',
    );

    await expectLater(requestCloseSafely(), throwsA(isA<TestFailure>()));

    await windowManager.setPreventClose(true);
    await waitForState(
      (WindowStateSnapshot s) => s.isPreventClose,
      reason: 'setPreventClose(true) to be reported',
    );
  });

  windowTest('setTitle accepts plain, unicode, empty, and long titles', (
    WidgetTester tester,
  ) async {
    // The plugin has no getter for the title, so this checks that the native
    // side accepts every shape of string and that the window keeps working.
    final List<String> titles = <String>[
      'Plain title',
      'Unicode 雪花 窗口 ✓ — ünïcödé',
      '',
      'x' * 600,
    ];
    for (final String title in titles) {
      await windowManager.setTitle(title);
    }
    await windowManager.setTitle(kBaselineTitle);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('setTitleBarStyle cycles through every style', (
    WidgetTester tester,
  ) async {
    final Size size = (await windowManager.getBounds()).size;

    for (final TitleBarStyle style in TitleBarStyle.values) {
      await windowManager.setTitleBarStyle(
        style,
        trafficLightsOffset: const Offset(18, 16),
        trafficLightsVisible: true,
      );
      final WindowStateSnapshot state = await windowManager.getState();
      expect(state.isVisible, isTrue, reason: '$style');
      // Changing the frame can move the reported origin (a removed title bar
      // shifts it), but never resizes the window.
      expectSizeNear(
        (await waitForStableBounds()).size,
        size,
        reason: 'size with $style',
      );
    }

    await windowManager.setTitleBarStyle(TitleBarStyle.normal);
    expectSizeNear((await waitForStableBounds()).size, size);
  });

  windowTest('setTitleBarStyle works without optional traffic light values', (
    WidgetTester tester,
  ) async {
    await windowManager.setTitleBarStyle(TitleBarStyle.hidden);
    await windowManager.setTitleBarStyle(TitleBarStyle.immersive);
    await windowManager.setTitleBarStyle(TitleBarStyle.normal);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('configure applies every title bar style', (
    WidgetTester tester,
  ) async {
    final Size size = (await windowManager.getBounds()).size;

    for (final TitleBarStyle style in TitleBarStyle.values) {
      final WindowStateSnapshot snapshot =
          await XueHuaNaviteWindowManager.configure(
            WindowConfig(
              titleBarStyle: style,
              trafficLightsOffset: const Offset(20, 18),
              trafficLightsVisible: true,
              showWhenReady: false,
            ),
          );
      expect(snapshot.isVisible, isTrue, reason: '$style');
      expectSizeNear(
        (await waitForStableBounds()).size,
        size,
        reason: 'size with $style',
      );
    }
  });

  windowTest('setOpacity accepts the whole range and clamps outside it', (
    WidgetTester tester,
  ) async {
    // 2.0 is clamped to 1.0 before it is sent. Opacity has no getter, so the
    // window must stay visible and keep its size.
    final Size size = (await windowManager.getBounds()).size;

    for (final double opacity in <double>[0.35, 0.8, 2.0, 1.0]) {
      await windowManager.setOpacity(opacity);
    }

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
    expectSizeNear(state.bounds.size, size);
  });

  windowTest('setBackgroundColor accepts opaque and translucent colors', (
    WidgetTester tester,
  ) async {
    final Size size = (await windowManager.getBounds()).size;

    for (final Color color in const <Color>[
      Color(0xFF112233),
      Color(0xFFFFFFFF),
      Color(0xCC336699),
      Color(0xFF202020),
    ]) {
      await windowManager.setBackgroundColor(color);
    }

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
    expectSizeNear(state.bounds.size, size);
  });

  windowTest('setHasShadow toggles without disturbing the window', (
    WidgetTester tester,
  ) async {
    final Size size = (await windowManager.getBounds()).size;

    await windowManager.setHasShadow(false);
    await windowManager.setHasShadow(true);
    await windowManager.setHasShadow(false);
    await windowManager.setHasShadow(true);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
    expectSizeNear(state.bounds.size, size);
  });

  windowTest('traffic light calls are safe on every platform', (
    WidgetTester tester,
  ) async {
    // Only macOS draws traffic lights. Windows and Linux accept the calls and
    // ignore them, which is the documented contract.
    await windowManager.setTitleBarStyle(
      TitleBarStyle.hidden,
      trafficLightsOffset: const Offset(16, 14),
      trafficLightsVisible: true,
    );
    await windowManager.setTrafficLightsOffset(const Offset(24, 20));
    await windowManager.setTrafficLightsVisible(false);
    await windowManager.setTrafficLightsVisible(true);
    await windowManager.setTrafficLightsOffset(const Offset(12, 12));
    await windowManager.setTitleBarStyle(TitleBarStyle.normal);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('Snap Layout button rectangle can be set and cleared', (
    WidgetTester tester,
  ) async {
    // Windows 11 uses the rectangle for WM_NCHITTEST. Other platforms accept
    // and ignore it.
    await windowManager.setSnapLayoutButtonRect(
      const Rect.fromLTWH(600, 0, 46, 40),
    );
    await windowManager.setSnapLayoutButtonRect(
      const Rect.fromLTWH(640, 0, 46, 40),
    );
    await windowManager.setSnapLayoutButtonRect(null);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('title bar drag rectangles can be set, replaced, and cleared', (
    WidgetTester tester,
  ) async {
    await windowManager.setTitleBarDragRects(
      dragRects: const <Rect>[Rect.fromLTWH(0, 0, 500, 40)],
      excludeRects: const <Rect>[Rect.fromLTWH(10, 5, 60, 30)],
    );
    await windowManager.setTitleBarDragRects(
      dragRects: const <Rect>[
        Rect.fromLTWH(0, 0, 300, 40),
        Rect.fromLTWH(320, 0, 200, 40),
      ],
    );
    await windowManager.setTitleBarDragRects(dragRects: const <Rect>[]);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('Linux interactive regions can be registered and removed', (
    WidgetTester tester,
  ) async {
    if (!isLinux) {
      markTestSkipped('Linux interactive regions are only used by GTK.');
      return;
    }
    final int moveId =
        XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId();
    await windowManager.setLinuxInteractiveRegion(
      id: moveId,
      present: true,
      kind: 'move',
      rect: const Rect.fromLTWH(0, 0, 400, 40),
    );

    final List<int> edgeIds = <int>[];
    for (final ResizeEdge edge in ResizeEdge.values) {
      final int id =
          XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId();
      edgeIds.add(id);
      await windowManager.setLinuxInteractiveRegion(
        id: id,
        present: true,
        kind: edge.wireValue,
        rect: const Rect.fromLTWH(0, 0, 6, 6),
      );
    }

    // Registering the same id again replaces the rectangle.
    await windowManager.setLinuxInteractiveRegion(
      id: moveId,
      present: true,
      kind: 'move',
      rect: const Rect.fromLTWH(0, 0, 420, 44),
    );

    for (final int id in <int>[moveId, ...edgeIds]) {
      await windowManager.setLinuxInteractiveRegion(id: id, present: false);
    }
    // Removing an id that is already gone is harmless.
    await windowManager.setLinuxInteractiveRegion(id: moveId, present: false);

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('Linux interactive regions reject an unknown kind', (
    WidgetTester tester,
  ) async {
    if (!isLinux) {
      markTestSkipped('Linux interactive regions are only used by GTK.');
      return;
    }

    await expectLater(
      windowManager.setLinuxInteractiveRegion(
        id: XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId(),
        present: true,
        kind: 'not-a-region-kind',
        rect: const Rect.fromLTWH(0, 0, 10, 10),
      ),
      throwsA(isA<PlatformException>()),
    );
  });

  windowTest('setLinuxInteractiveRegion is a no-op away from Linux', (
    WidgetTester tester,
  ) async {
    if (isLinux) {
      markTestSkipped('This test covers the macOS and Windows no-op path.');
      return;
    }

    // The controller returns before reaching native code, so even a kind that
    // Linux would reject completes normally.
    await windowManager.setLinuxInteractiveRegion(
      id: XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId(),
      present: true,
      kind: 'not-a-region-kind',
      rect: const Rect.fromLTWH(0, 0, 10, 10),
    );
  });
}

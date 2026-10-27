// show / hide / focus / blur, maximize / unmaximize, minimize / restore, and
// fullscreen, exercised against the real native window of the example app.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_lifecycle_suite.dart -d <device>`.
//
// The window system performs these changes, so each test waits for the state it
// expects. Every test ends with the window restored (see windowTest), because a
// minimized or fullscreen window would stall the tests that follow.
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('hide hides the window and show reveals it again', (
    WidgetTester tester,
  ) async {
    await windowManager.hide();
    await waitForState(
      (WindowStateSnapshot s) => !s.isVisible,
      reason: 'hide() to take effect',
    );

    // The process must survive an empty screen. On macOS the example host
    // returns false from applicationShouldTerminateAfterLastWindowClosed for
    // exactly this reason; reaching this line proves it.
    await windowManager.show();
    final WindowStateSnapshot shown = await waitForState(
      (WindowStateSnapshot s) => s.isVisible,
      reason: 'show() to take effect',
    );
    expect(shown.isMinimized, isFalse);
  });

  windowTest('show(focus: false) reveals the window', (
    WidgetTester tester,
  ) async {
    await windowManager.hide();
    await waitForState(
      (WindowStateSnapshot s) => !s.isVisible,
      reason: 'hide() to take effect',
    );

    await windowManager.show(focus: false);

    await waitForState(
      (WindowStateSnapshot s) => s.isVisible,
      reason: 'show(focus: false) to take effect',
    );
  });

  windowTest('hide and show leave the window size unchanged', (
    WidgetTester tester,
  ) async {
    final Size before = (await windowManager.getBounds()).size;

    await windowManager.hide();
    await waitForState((WindowStateSnapshot s) => !s.isVisible, reason: 'hide');
    await windowManager.show();
    await waitForState((WindowStateSnapshot s) => s.isVisible, reason: 'show');

    // The position is not compared: some window managers place a window again
    // when it is mapped.
    expectSizeNear((await waitForStableBounds()).size, before);
  });

  windowTest('focus gives the window input focus', (WidgetTester tester) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }

    await windowManager.focus();

    final WindowStateSnapshot state = await waitForState(
      (WindowStateSnapshot s) => s.isFocused,
      reason: 'focus() to give the window input focus',
    );
    expect(state.isVisible, isTrue);
  });

  windowTest('blur keeps the window on screen and focus brings it back', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final Size before = (await windowManager.getBounds()).size;

    await windowManager.blur();

    // blur() sends the window behind the others. It neither hides nor resizes
    // it, and not every window system also moves keyboard focus.
    final WindowStateSnapshot afterBlur = await windowManager.getState();
    expect(afterBlur.isVisible, isTrue);
    expect(afterBlur.isMinimized, isFalse);
    expectSizeNear(afterBlur.bounds.size, before);

    await windowManager.focus();
    await waitForState(
      (WindowStateSnapshot s) => s.isFocused,
      reason: 'focus() after blur() to give the window input focus',
    );
  });

  windowTest('maximize fills the work area and unmaximize restores the bounds', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final Rect before = await waitForStableBounds();

    await windowManager.maximize();
    final WindowStateSnapshot maximized = await waitForState(
      (WindowStateSnapshot s) => s.isMaximized,
      reason: 'maximize() to take effect',
    );
    expect(maximized.bounds.width, greaterThan(before.width));
    expect(maximized.bounds.height, greaterThan(before.height));

    await windowManager.unmaximize();
    await waitForState(
      (WindowStateSnapshot s) => !s.isMaximized,
      reason: 'unmaximize() to take effect',
    );
    final Rect after = await waitForBounds(
      (Rect bounds) => sizeIsNear(bounds.size, before.size),
      reason: 'unmaximize() to restore the previous size',
    );
    if (await positionsAreAuthoritative()) {
      expectOffsetNear(after.topLeft, before.topLeft);
    }
  });

  windowTest('toggleMaximize switches between maximized and normal', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }

    await windowManager.toggleMaximize();
    await waitForState(
      (WindowStateSnapshot s) => s.isMaximized,
      reason: 'the first toggleMaximize() to maximize',
    );

    await windowManager.toggleMaximize();
    await waitForState(
      (WindowStateSnapshot s) => !s.isMaximized,
      reason: 'the second toggleMaximize() to unmaximize',
    );
  });

  windowTest('minimize iconifies the window and restore brings it back', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final Size before = (await windowManager.getBounds()).size;

    await windowManager.minimize();
    await waitForState(
      (WindowStateSnapshot s) => s.isMinimized,
      reason: 'minimize() to take effect',
    );

    // restore() has to run before anything else needs a frame: a minimized
    // window is not rendered.
    await windowManager.restore();
    final WindowStateSnapshot restored = await waitForState(
      (WindowStateSnapshot s) => !s.isMinimized && s.isVisible,
      reason: 'restore() to bring the window back',
    );
    expect(restored.isMaximized, isFalse);
    expectSizeNear((await waitForStableBounds()).size, before);
  });

  windowTest('restore also leaves the maximized state', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }

    await windowManager.maximize();
    await waitForState(
      (WindowStateSnapshot s) => s.isMaximized,
      reason: 'maximize() to take effect',
    );

    await windowManager.restore();
    await waitForState(
      (WindowStateSnapshot s) => !s.isMaximized,
      reason: 'restore() to leave the maximized state',
    );
  });

  windowTest('setFullScreen enters and leaves native fullscreen', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final WindowEventRecorder recorder = await WindowEventRecorder.attach();
    addTearDown(recorder.dispose);
    final Size before = (await windowManager.getBounds()).size;

    await windowManager.setFullScreen(true);
    final WindowStateSnapshot full = await waitForState(
      (WindowStateSnapshot s) => s.isFullScreen,
      reason: 'setFullScreen(true) to take effect',
      timeout: kFullScreenTimeout,
    );
    expect(full.isFullScreen, isTrue);
    await recorder.waitFor(
      WindowEventType.enteredFullScreen,
      timeout: kFullScreenTimeout,
    );

    await windowManager.setFullScreen(false);
    await waitForState(
      (WindowStateSnapshot s) => !s.isFullScreen,
      reason: 'setFullScreen(false) to take effect',
      timeout: kFullScreenTimeout,
    );
    await recorder.waitFor(
      WindowEventType.exitedFullScreen,
      timeout: kFullScreenTimeout,
    );

    // macOS reports the end of the exit animation a moment later.
    final Rect after = await waitForBounds(
      (Rect bounds) => sizeIsNear(bounds.size, before),
      reason: 'the window size to come back after leaving fullscreen',
      timeout: kFullScreenTimeout,
    );
    expectSizeNear(after.size, before);
  });
}

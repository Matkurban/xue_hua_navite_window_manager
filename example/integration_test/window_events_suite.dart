// The native window event stream and WindowListener, exercised against the real
// native window of the example app.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_events_suite.dart -d <device>`.
//
// Events are asserted by presence, never by exact sequence or count: platforms
// differ in how many resized and moved events one operation produces (GTK sends
// one pair for every configure-event, AppKit and Win32 usually one).
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

/// Attaches a recorder and drops the events left over from the baseline reset.
Future<WindowEventRecorder> _recorderAtRest() async {
  final WindowEventRecorder recorder = await WindowEventRecorder.attach();
  addTearDown(recorder.dispose);
  await waitForStableBounds();
  await Future<void>.delayed(const Duration(milliseconds: 200));
  recorder.clear();
  return recorder;
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('setSize emits a resized event with the new bounds', (
    WidgetTester tester,
  ) async {
    final WindowEventRecorder recorder = await _recorderAtRest();
    final Size target = sizes.smaller;

    await windowManager.setSize(target);

    final WindowEvent event = await recorder.waitFor(
      WindowEventType.resized,
      where: (WindowEvent e) =>
          e.bounds != null && sizeIsNear(e.bounds!.size, target),
    );
    expectSizeNear(event.bounds!.size, target, reason: 'resized event');
    expect(
      recorder.resizeSizes.any((Size size) => sizeIsNear(size, target)),
      isTrue,
      reason: 'onWindowResize sizes: ${recorder.resizeSizes}',
    );
    expect(
      recorder.listenerEvents.map((WindowEvent e) => e.type),
      contains(WindowEventType.resized),
    );
  });

  windowTest('setPosition emits a moved event with the new origin', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped(
        'Wayland clients cannot place or read absolute window positions.',
      );
      return;
    }
    final WindowEventRecorder recorder = await _recorderAtRest();
    const Offset target = Offset(150, 130);

    await windowManager.setPosition(target);

    final WindowEvent event = await recorder.waitFor(
      WindowEventType.moved,
      where: (WindowEvent e) =>
          e.bounds != null && offsetIsNear(e.bounds!.topLeft, target),
    );
    expectOffsetNear(event.bounds!.topLeft, target, reason: 'moved event');
    expect(
      recorder.movePositions.any((Offset p) => offsetIsNear(p, target)),
      isTrue,
      reason: 'onWindowMove positions: ${recorder.movePositions}',
    );
  });

  windowTest('maximize and unmaximize emit events and callbacks', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final WindowEventRecorder recorder = await _recorderAtRest();

    await windowManager.maximize();
    final WindowEvent maximized = await recorder.waitFor(
      WindowEventType.maximized,
    );
    expect(maximized.isMaximized, isTrue);
    await recorder.waitForCallback('maximize');

    await windowManager.unmaximize();
    final WindowEvent unmaximized = await recorder.waitFor(
      WindowEventType.unmaximized,
    );
    expect(unmaximized.isMaximized, isFalse);
    await recorder.waitForCallback('unmaximize');
  });

  windowTest('minimize and restore emit events and callbacks', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final WindowEventRecorder recorder = await _recorderAtRest();

    await windowManager.minimize();
    await recorder.waitFor(WindowEventType.minimized);
    await recorder.waitForCallback('minimize');

    await windowManager.restore();
    await recorder.waitFor(WindowEventType.restored);
    await recorder.waitForCallback('restore');
  });

  windowTest('hiding and showing the window emits blurred and focused', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    await windowManager.focus();
    await waitForState(
      (WindowStateSnapshot s) => s.isFocused,
      reason: 'the window to have focus before it is hidden',
    );
    final WindowEventRecorder recorder = await _recorderAtRest();

    await windowManager.hide();
    if (isLinux) {
      // GTK reports focus loss through window-state events, and whether an
      // unmapped window still receives one depends on the window manager
      // (xfwm4 delivers it, openbox does not). Without it there is nothing to
      // compare the focus that show() restores against.
      try {
        await recorder.waitFor(
          WindowEventType.blurred,
          timeout: const Duration(seconds: 3),
        );
      } on TestFailure {
        markTestSkipped(
          'This window manager does not report focus loss when a window is '
          'hidden.',
        );
        return;
      }
    } else {
      await recorder.waitFor(WindowEventType.blurred);
    }
    await recorder.waitForCallback('blur');

    await windowManager.show();
    await recorder.waitFor(WindowEventType.focused);
    await recorder.waitForCallback('focus');
  });

  windowTest(
    'close with preventClose emits closeRequested and keeps the window',
    (WidgetTester tester) async {
      final WindowEventRecorder recorder = await _recorderAtRest();

      await requestCloseSafely();

      final WindowEvent event = await recorder.waitFor(
        WindowEventType.closeRequested,
      );
      await recorder.waitForCallback('closeRequested');
      expect(event.bounds, isNotNull);

      // The window is still alive and answering.
      final WindowStateSnapshot state = await windowManager.getState();
      expect(state.isVisible, isTrue);
      expect(state.isPreventClose, isTrue);
      expect(state.bounds.width, greaterThan(0));
    },
  );

  windowTest('close can be requested again after it was intercepted', (
    WidgetTester tester,
  ) async {
    final WindowEventRecorder recorder = await _recorderAtRest();

    await requestCloseSafely();
    await recorder.waitFor(WindowEventType.closeRequested);
    recorder.clear();

    await requestCloseSafely();
    await recorder.waitFor(WindowEventType.closeRequested);

    expect((await windowManager.getState()).isVisible, isTrue);
  });

  windowTest('event payloads carry bounds, flags, and a current timestamp', (
    WidgetTester tester,
  ) async {
    final WindowEventRecorder recorder = await _recorderAtRest();
    final int before = DateTime.now().millisecondsSinceEpoch;

    await windowManager.setSize(sizes.larger);
    final WindowEvent event = await recorder.waitFor(
      WindowEventType.resized,
      where: (WindowEvent e) =>
          e.bounds != null && sizeIsNear(e.bounds!.size, sizes.larger),
    );

    expect(event.bounds, isNotNull);
    expect(event.isMaximized, isFalse);
    expect(event.isFullScreen, isFalse);
    expect(event.timestamp, greaterThanOrEqualTo(before - 60 * 1000));
    expect(
      event.timestamp,
      lessThanOrEqualTo(DateTime.now().millisecondsSinceEpoch + 60 * 1000),
    );
    expect(event.toString(), contains('resized'));
  });

  windowTest('removeListener stops the typed callbacks', (
    WidgetTester tester,
  ) async {
    final WindowEventRecorder recorder = await _recorderAtRest();
    windowManager.removeListener(recorder);

    await windowManager.setSize(sizes.smaller);
    await recorder.waitFor(
      WindowEventType.resized,
      where: (WindowEvent e) =>
          e.bounds != null && sizeIsNear(e.bounds!.size, sizes.smaller),
    );
    await Future<void>.delayed(const Duration(milliseconds: 200));

    // The raw stream subscription still works; the listener no longer does.
    expect(recorder.listenerEvents, isEmpty);
    expect(recorder.resizeSizes, isEmpty);
  });

  windowTest('every subscriber of the event stream receives each event', (
    WidgetTester tester,
  ) async {
    final WindowEventRecorder first = await _recorderAtRest();
    final WindowEventRecorder second = await _recorderAtRest();
    final Size target = sizes.smaller;

    await windowManager.setSize(target);

    for (final WindowEventRecorder recorder in <WindowEventRecorder>[
      first,
      second,
    ]) {
      await recorder.waitFor(
        WindowEventType.resized,
        where: (WindowEvent e) =>
            e.bounds != null && sizeIsNear(e.bounds!.size, target),
      );
    }
  });

  windowTest('lastSnapshot follows native events', (WidgetTester tester) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    final WindowEventRecorder recorder = await _recorderAtRest();
    // getState() refreshes lastSnapshot itself. Seed it, then rely on events
    // alone for the rest of the test.
    await windowManager.getState();

    await windowManager.maximize();
    await recorder.waitFor(WindowEventType.maximized);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    expect(windowManager.lastSnapshot?.isMaximized, isTrue);

    await windowManager.unmaximize();
    await recorder.waitFor(WindowEventType.unmaximized);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    expect(windowManager.lastSnapshot?.isMaximized, isFalse);

    await windowManager.minimize();
    await recorder.waitFor(WindowEventType.minimized);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    expect(windowManager.lastSnapshot?.isMinimized, isTrue);

    await windowManager.restore();
    await recorder.waitFor(WindowEventType.restored);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    expect(windowManager.lastSnapshot?.isMinimized, isFalse);
  });
}

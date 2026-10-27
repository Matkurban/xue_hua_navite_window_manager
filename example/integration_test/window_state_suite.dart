// Initialization, getState, and the atomic configure() contract, exercised
// against the real native window of the example app.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_state_suite.dart -d <device>`.
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('the method channel implementation is registered and answers', (
    WidgetTester tester,
  ) async {
    expect(
      XueHuaNaviteWindowManagerPlatform.instance,
      isA<MethodChannelXueHuaNaviteWindowManager>(),
    );
    final Rect bounds = await windowManager.getBounds();
    expect(bounds.width, greaterThan(0));
    expect(bounds.height, greaterThan(0));
  });

  windowTest('ensureInitialized can be awaited repeatedly', (
    WidgetTester tester,
  ) async {
    await windowManager.ensureInitialized();
    await windowManager.ensureInitialized();
    await XueHuaNaviteWindowManagerPlatform.instance.ensureInitialized();

    final WindowStateSnapshot state = await windowManager.getState();
    expect(state.isVisible, isTrue);
  });

  windowTest('getState describes the baseline window', (
    WidgetTester tester,
  ) async {
    final WindowStateSnapshot state = await windowManager.getState();

    expect(state.isVisible, isTrue);
    expect(state.isMaximized, isFalse);
    expect(state.isMinimized, isFalse);
    expect(state.isFullScreen, isFalse);
    expect(state.isAlwaysOnTop, isFalse);
    expect(state.isResizable, isTrue);
    expect(state.isPreventClose, isTrue);
    expectSizeNear(state.bounds.size, sizes.base, reason: 'baseline size');
    if (isMacOS || isWindows) {
      expect(state.isPositionAuthoritative, isTrue);
    }
  });

  windowTest('getState refreshes lastSnapshot', (WidgetTester tester) async {
    final WindowStateSnapshot state = await windowManager.getState();

    expect(identical(windowManager.lastSnapshot, state), isTrue);

    await windowManager.setAlwaysOnTop(true);
    final WindowStateSnapshot updated = await windowManager.getState();
    expect(updated.isAlwaysOnTop, isTrue);
    expect(windowManager.lastSnapshot?.isAlwaysOnTop, isTrue);
  });

  windowTest('configure applies many properties in a single call', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    final WindowStateSnapshot snapshot =
        await XueHuaNaviteWindowManager.configure(
          WindowConfig(
            size: metrics.larger,
            minSize: metrics.minLimit,
            maxSize: metrics.maxLimit,
            position: const Offset(100, 120),
            title: 'configured by the integration test',
            titleBarStyle: TitleBarStyle.normal,
            backgroundColor: const Color(0xFF123456),
            alwaysOnTop: true,
            resizable: true,
            preventClose: true,
            hasShadow: false,
            resizeBorderWidth: 8,
            showWhenReady: false,
          ),
        );

    expect(snapshot.isAlwaysOnTop, isTrue);
    expect(snapshot.isResizable, isTrue);
    expect(snapshot.isPreventClose, isTrue);

    final Rect bounds = await waitForSize(metrics.larger);
    expectSizeNear(bounds.size, metrics.larger);
    if (await positionsAreAuthoritative()) {
      await waitForPosition(const Offset(100, 120));
    }
    final WindowStateSnapshot fresh = await windowManager.getState();
    expect(fresh.isAlwaysOnTop, isTrue);
    expect(fresh.isPreventClose, isTrue);
  });

  windowTest('configure can turn resizable and alwaysOnTop off again', (
    WidgetTester tester,
  ) async {
    final WindowStateSnapshot locked =
        await XueHuaNaviteWindowManager.configure(
          const WindowConfig(
            resizable: false,
            alwaysOnTop: true,
            showWhenReady: false,
          ),
        );
    expect(locked.isResizable, isFalse);
    expect(locked.isAlwaysOnTop, isTrue);

    final WindowStateSnapshot unlocked =
        await XueHuaNaviteWindowManager.configure(
          const WindowConfig(
            resizable: true,
            alwaysOnTop: false,
            showWhenReady: false,
          ),
        );
    expect(unlocked.isResizable, isTrue);
    expect(unlocked.isAlwaysOnTop, isFalse);
  });

  windowTest('configure clamps the requested size to minSize and maxSize', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    await XueHuaNaviteWindowManager.configure(
      WindowConfig(
        size: const Size(40, 40),
        minSize: metrics.minLimit,
        maxSize: metrics.maxLimit,
        showWhenReady: false,
      ),
    );
    await waitForSize(
      metrics.minLimit,
      reason: 'a 40x40 request to be raised to minSize',
    );

    await XueHuaNaviteWindowManager.configure(
      WindowConfig(
        size: Size(metrics.maxLimit.width * 3, metrics.maxLimit.height * 3),
        minSize: metrics.minLimit,
        maxSize: metrics.maxLimit,
        showWhenReady: false,
      ),
    );
    await waitForSize(
      metrics.maxLimit,
      reason: 'an oversized request to be lowered to maxSize',
    );
  });

  windowTest('configure places the window at an explicit position', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped(
        'Wayland clients cannot place or read absolute window positions.',
      );
      return;
    }

    await XueHuaNaviteWindowManager.configure(
      const WindowConfig(position: Offset(140, 110), showWhenReady: false),
    );
    await waitForPosition(const Offset(140, 110));
  });

  windowTest('configure(center: true) centers the window whatever its size', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped('Wayland compositors choose where windows are placed.');
      return;
    }
    final TestSizes metrics = sizes;

    await XueHuaNaviteWindowManager.configure(
      WindowConfig(size: metrics.smaller, center: true, showWhenReady: false),
    );
    await waitForSize(metrics.smaller);
    final Rect small = await waitForStableBounds();

    await XueHuaNaviteWindowManager.configure(
      WindowConfig(size: metrics.larger, center: true, showWhenReady: false),
    );
    await waitForSize(metrics.larger);
    final Rect large = await waitForStableBounds();

    // The display's work area is not exposed to Dart, so compare the two
    // centered windows: both must sit on the same center point.
    expectOffsetNear(
      large.center,
      small.center,
      tolerance: 3,
      reason: 'center of the larger window',
    );
  });

  windowTest('configure leaves properties that were not given untouched', (
    WidgetTester tester,
  ) async {
    await windowManager.setAlwaysOnTop(true);
    final WindowStateSnapshot before = await windowManager.getState();

    await XueHuaNaviteWindowManager.configure(
      const WindowConfig(title: 'only the title changes', showWhenReady: false),
    );

    final WindowStateSnapshot after = await windowManager.getState();
    expect(after.isAlwaysOnTop, isTrue);
    expect(after.isResizable, before.isResizable);
    expect(after.isPreventClose, before.isPreventClose);
    expectSizeNear(after.bounds.size, before.bounds.size);
    if (await positionsAreAuthoritative()) {
      expectOffsetNear(after.bounds.topLeft, before.bounds.topLeft);
    }
  });

  windowTest('markReadyToShow reveals a hidden window', (
    WidgetTester tester,
  ) async {
    await windowManager.hide();
    await waitForState(
      (WindowStateSnapshot s) => !s.isVisible,
      reason: 'the window to hide',
    );

    await XueHuaNaviteWindowManagerPlatform.instance.markReadyToShow();

    final WindowStateSnapshot state = await waitForState(
      (WindowStateSnapshot s) => s.isVisible,
      reason: 'markReadyToShow to reveal the window',
    );
    expect(state.isMinimized, isFalse);
  });

  windowTest('showWhenReady reveals the window after the first frame', (
    WidgetTester tester,
  ) async {
    // macOS conceals the window with alpha 0 and keeps it ordered in, because a
    // window that is ordered out is not asked for frames. The other platforms
    // hide it, which is the state a freshly launched runner is in.
    if (!isMacOS) {
      await windowManager.hide();
      await waitForState(
        (WindowStateSnapshot s) => !s.isVisible,
        reason: 'the window to hide',
      );
    }

    await XueHuaNaviteWindowManager.configure(
      WindowConfig(
        size: sizes.base,
        position: sizes.origin,
        title: kBaselineTitle,
        showWhenReady: true,
      ),
    );
    await tester.pumpWidget(const SizedBox.shrink());

    final WindowStateSnapshot state = await waitForState(
      (WindowStateSnapshot s) => s.isVisible,
      reason: 'showWhenReady to reveal the window after the first frame',
    );
    expect(state.isMinimized, isFalse);
  });

  windowTest('a native snapshot survives a toMap and fromMap round trip', (
    WidgetTester tester,
  ) async {
    final WindowStateSnapshot native = await windowManager.getState();
    final WindowStateSnapshot copy = WindowStateSnapshot.fromMap(
      native.toMap(),
    );

    expect(copy.bounds, native.bounds);
    expect(copy.isVisible, native.isVisible);
    expect(copy.isMaximized, native.isMaximized);
    expect(copy.isMinimized, native.isMinimized);
    expect(copy.isFullScreen, native.isFullScreen);
    expect(copy.isFocused, native.isFocused);
    expect(copy.isAlwaysOnTop, native.isAlwaysOnTop);
    expect(copy.isResizable, native.isResizable);
    expect(copy.isPreventClose, native.isPreventClose);
    expect(copy.isPositionAuthoritative, native.isPositionAuthoritative);
  });
}

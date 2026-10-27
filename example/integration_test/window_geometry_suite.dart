// setSize / setPosition / setBounds / center, the min and max size limits, and
// resizable, exercised against the real native window of the example app.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/window_geometry_suite.dart -d <device>`.
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'support/window_test_support.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('setSize resizes the window and keeps its top-left corner', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    final Rect before = await windowManager.getBounds();

    await windowManager.setSize(metrics.smaller);
    final Rect smaller = await waitForSize(metrics.smaller);
    if (await positionsAreAuthoritative()) {
      expectOffsetNear(smaller.topLeft, before.topLeft);
    }

    await windowManager.setSize(metrics.larger);
    final Rect larger = await waitForSize(metrics.larger);
    if (await positionsAreAuthoritative()) {
      expectOffsetNear(larger.topLeft, before.topLeft);
    }
  });

  windowTest('getSize, getPosition, and getBounds describe the same window', (
    WidgetTester tester,
  ) async {
    await windowManager.setSize(sizes.smaller);
    await waitForSize(sizes.smaller);
    final Rect bounds = await waitForStableBounds();

    final Size size = await windowManager.getSize();
    final Offset position = await windowManager.getPosition();

    expectSizeNear(size, bounds.size, reason: 'getSize');
    expectOffsetNear(position, bounds.topLeft, reason: 'getPosition');
  });

  windowTest('setPosition moves the window and keeps its size', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped(
        'Wayland clients cannot place or read absolute window positions.',
      );
      return;
    }
    final Size size = (await windowManager.getBounds()).size;

    await windowManager.setPosition(const Offset(140, 120));
    final Rect first = await waitForPosition(const Offset(140, 120));
    expectSizeNear(first.size, size, reason: 'size after the first move');
    expectOffsetNear(await windowManager.getPosition(), const Offset(140, 120));

    await windowManager.setPosition(const Offset(90, 100));
    final Rect second = await waitForPosition(const Offset(90, 100));
    expectSizeNear(second.size, size, reason: 'size after the second move');
  });

  windowTest('setBounds sets position and size together', (
    WidgetTester tester,
  ) async {
    final Size size = sizes.larger;
    final Rect target = Rect.fromLTWH(100, 110, size.width, size.height);

    await windowManager.setBounds(target);

    final Rect bounds = await waitForSize(size);
    if (await positionsAreAuthoritative()) {
      await waitForPosition(target.topLeft);
    }
    expectSizeNear(bounds.size, size);
  });

  windowTest('animated setSize, setPosition, and setBounds reach their target', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;

    // macOS animates the frame change; the other platforms apply it at once.
    // Either way the window must end up exactly at the requested geometry.
    await windowManager.setSize(metrics.smaller, animate: true);
    await waitForSize(metrics.smaller);

    final Rect target = Rect.fromLTWH(
      110,
      130,
      metrics.larger.width,
      metrics.larger.height,
    );
    await windowManager.setBounds(target, animate: true);
    await waitForSize(metrics.larger);

    if (await positionsAreAuthoritative()) {
      await waitForPosition(target.topLeft);
      await windowManager.setPosition(const Offset(80, 90), animate: true);
      await waitForPosition(const Offset(80, 90));
    }
  });

  windowTest('center reports whether the window system applied the move', (
    WidgetTester tester,
  ) async {
    final bool authoritative = await positionsAreAuthoritative();

    final bool applied = await windowManager.center();

    // Windows, macOS, and X11 move the window and report true. Wayland cannot
    // move a mapped window and reports false, together with an origin that is
    // not authoritative.
    expect(applied, authoritative);
  });

  windowTest('center places the window on a size-independent anchor', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped('Wayland compositors choose where windows are placed.');
      return;
    }
    final TestSizes metrics = sizes;

    await windowManager.setPosition(const Offset(30, 40));
    final Rect corner = await waitForPosition(const Offset(30, 40));
    expect(await windowManager.center(), isTrue);
    final Rect centered = await waitForBounds(
      (Rect bounds) => !offsetIsNear(bounds.topLeft, corner.topLeft, 5),
      reason: 'center() to move the window away from the corner',
    );
    final Rect stable = await waitForStableBounds();
    expectSizeNear(stable.size, centered.size);

    // center(size:) resizes first, then centers: the window keeps the same
    // center point because that point only depends on the display.
    expect(await windowManager.center(size: metrics.smaller), isTrue);
    await waitForSize(metrics.smaller);
    final Rect resized = await waitForStableBounds();
    expectSizeNear(resized.size, metrics.smaller, reason: 'center(size:)');
    expectOffsetNear(
      resized.center,
      stable.center,
      tolerance: 3,
      reason: 'center of the resized window',
    );
  });

  windowTest('center is stable when called twice', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped('Wayland compositors choose where windows are placed.');
      return;
    }

    await windowManager.center();
    final Rect first = await waitForStableBounds();
    await windowManager.center();
    final Rect second = await waitForStableBounds();

    expectOffsetNear(second.topLeft, first.topLeft, reason: 'second center()');
    expectSizeNear(second.size, first.size, reason: 'second center()');
  });

  windowTest('setMinimumSize raises smaller programmatic sizes to the limit', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    await windowManager.setMinimumSize(metrics.minLimit);

    await windowManager.setSize(const Size(50, 50));
    await waitForSize(
      metrics.minLimit,
      reason: 'setSize(50x50) to be raised to the minimum',
    );

    await windowManager.setBounds(const Rect.fromLTWH(100, 100, 60, 60));
    await waitForSize(
      metrics.minLimit,
      reason: 'setBounds with a 60x60 size to be raised to the minimum',
    );

    await windowManager.center(size: const Size(30, 30));
    await waitForSize(
      metrics.minLimit,
      reason: 'center(size: 30x30) to be raised to the minimum',
    );

    // Sizes above the minimum are untouched.
    await windowManager.setSize(metrics.larger);
    await waitForSize(metrics.larger);
  });

  windowTest('setMaximumSize lowers larger programmatic sizes to the limit', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    final Size huge = Size(metrics.maxLimit.width * 3, metrics.maxLimit.height * 3);
    await windowManager.setMaximumSize(metrics.maxLimit);

    await windowManager.setSize(huge);
    await waitForSize(
      metrics.maxLimit,
      reason: 'an oversized setSize to be lowered to the maximum',
    );

    await windowManager.setBounds(
      Rect.fromLTWH(100, 100, huge.width, huge.height),
    );
    await waitForSize(
      metrics.maxLimit,
      reason: 'an oversized setBounds to be lowered to the maximum',
    );

    await windowManager.center(size: huge);
    await waitForSize(
      metrics.maxLimit,
      reason: 'an oversized center(size:) to be lowered to the maximum',
    );

    // Sizes below the maximum are untouched.
    await windowManager.setSize(metrics.smaller);
    await waitForSize(metrics.smaller);
  });

  windowTest('minimum and maximum limits apply together', (
    WidgetTester tester,
  ) async {
    final TestSizes metrics = sizes;
    await windowManager.setMinimumSize(metrics.minLimit);
    await windowManager.setMaximumSize(metrics.maxLimit);

    await windowManager.setSize(const Size(10, 10));
    await waitForSize(metrics.minLimit);

    await windowManager.setSize(kUnboundedSize);
    await waitForSize(metrics.maxLimit);

    await windowManager.setSize(metrics.base);
    await waitForSize(metrics.base);
  });

  windowTest('setResizable updates isResizable and can be reversed', (
    WidgetTester tester,
  ) async {
    await windowManager.setResizable(false);
    final WindowStateSnapshot locked = await waitForState(
      (WindowStateSnapshot s) => !s.isResizable,
      reason: 'setResizable(false) to be reported',
    );
    expect(locked.isVisible, isTrue);

    await windowManager.setResizable(true);
    await waitForState(
      (WindowStateSnapshot s) => s.isResizable,
      reason: 'setResizable(true) to be reported',
    );

    // A window that was made resizable again accepts new sizes.
    await windowManager.setSize(sizes.smaller);
    await waitForSize(sizes.smaller);
  });
}

// The example Studio app, driven through its own UI against the real native
// window: telemetry, behavior chips, presets, color and opacity controls, the
// lifecycle buttons, and the intercepted-close dialog.
//
// Run through plugin_integration_test.dart, or alone with
// `flutter test integration_test/studio_app_suite.dart -d <device>`.
//
// The dialog's "Force Destroy" button is never tapped: it calls destroy() and
// would end the process running the tests. "Minimize Instead" is not tapped
// either; minimize is covered by the lifecycle and widget suites.
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';
import 'package:xue_hua_navite_window_manager_example/main.dart';

import 'support/window_test_support.dart';

Future<void> _settle(WidgetTester tester) {
  return tester.pumpAndSettle(
    const Duration(milliseconds: 100),
    EnginePhase.sendSemanticsUpdate,
    const Duration(seconds: 20),
  );
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

/// Scrolls [finder] into view and taps it. The Studio is a scrolling dashboard
/// and the window is smaller than its full height.
Future<void> _tap(WidgetTester tester, Finder finder) async {
  await tester.ensureVisible(finder);
  await _settle(tester);
  await tester.tap(finder);
  await tester.pump();
}

Finder _chip(String label) => find.widgetWithText(FilterChip, label);

String _sizeBadge(Size size) =>
    '${size.width.toStringAsFixed(0)} × ${size.height.toStringAsFixed(0)}';

/// Sizes the window for the dashboard and pumps the real example app.
Future<void> _launchStudio(WidgetTester tester) async {
  final Size studio = sizes.studio;
  await windowManager.setSize(studio);
  await waitForSize(studio);
  await tester.pumpWidget(const WindowManagerExampleApp());
  await _settle(tester);
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  windowTest('the Studio renders and shows the live window geometry', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);

    expect(find.text('XueHua Native Window Manager Studio'), findsWidgets);
    expect(
      find.text(
        '1. Atomic Batch Configuration (Single-Channel Zero-Jump Presets)',
      ),
      findsOneWidget,
    );

    // The telemetry badge is filled from getState(); it must show the size the
    // native window really has.
    final Rect bounds = await windowManager.getBounds();
    await _pumpUntilFound(tester, find.text(_sizeBadge(bounds.size)));
    expect(tester.takeException(), isNull);
  });

  windowTest('Sync State reads the native flags into the chips', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);

    // alwaysOnTop changes natively without an event, so only Sync State can
    // tell the Studio about it.
    await windowManager.setAlwaysOnTop(true);
    await _tap(tester, find.text('Sync State'));
    await _settle(tester);

    expect(tester.widget<FilterChip>(_chip('Always On Top')).selected, isTrue);
    await windowManager.setAlwaysOnTop(false);
    await _tap(tester, find.text('Sync State'));
    await _settle(tester);
    expect(tester.widget<FilterChip>(_chip('Always On Top')).selected, isFalse);
  });

  windowTest('the behavior chips drive the native window', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);

    await _tap(tester, _chip('Always On Top'));
    await waitForState(
      (WindowStateSnapshot s) => s.isAlwaysOnTop,
      reason: 'the Always On Top chip to float the window',
    );
    await _tap(tester, _chip('Always On Top'));
    await waitForState(
      (WindowStateSnapshot s) => !s.isAlwaysOnTop,
      reason: 'the Always On Top chip to stop floating the window',
    );

    await _tap(tester, _chip('Resizable'));
    await waitForState(
      (WindowStateSnapshot s) => !s.isResizable,
      reason: 'the Resizable chip to lock the window size',
    );
    await _tap(tester, _chip('Resizable'));
    await waitForState(
      (WindowStateSnapshot s) => s.isResizable,
      reason: 'the Resizable chip to unlock the window size',
    );

    // Prevent Close goes off and straight back on. While it is off, a close
    // request would destroy this window, so nothing in between asks for one.
    const String preventClose = 'Prevent Close (Intercept WM_CLOSE)';
    await _tap(tester, _chip(preventClose));
    await waitForState(
      (WindowStateSnapshot s) => !s.isPreventClose,
      reason: 'the Prevent Close chip to turn interception off',
    );
    await _tap(tester, _chip(preventClose));
    await waitForState(
      (WindowStateSnapshot s) => s.isPreventClose,
      reason: 'the Prevent Close chip to turn interception back on',
    );

    await _tap(tester, _chip('Native Shadow & Rounded Corners'));
    await _tap(tester, _chip('Native Shadow & Rounded Corners'));
    expect(tester.takeException(), isNull);
  });

  windowTest('the opacity slider and color swatches reach the native window', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);

    // A swatch whose color is already the background shows a check mark, so
    // match the label with or without it.
    await _tap(tester, find.textContaining('Secondary Container'));
    await _settle(tester);
    expect(find.text('Secondary Container ✓'), findsOneWidget);

    await _tap(tester, find.textContaining('Theme Surface'));
    await _settle(tester);
    expect(find.text('Theme Surface ✓'), findsOneWidget);
    expect(find.text('Secondary Container ✓'), findsNothing);

    // The third slider is the window opacity. Dragging it calls setOpacity.
    final Finder opacity = find.byType(Slider).at(2);
    await tester.ensureVisible(opacity);
    await _settle(tester);
    expect(find.text('100%'), findsOneWidget);
    await tester.drag(opacity, const Offset(-80, 0));
    await tester.pump(const Duration(milliseconds: 200));
    expect(find.text('100%'), findsNothing);

    await windowManager.setOpacity(1.0);
    expect((await windowManager.getState()).isVisible, isTrue);
    expect(tester.takeException(), isNull);
  });

  windowTest('the atomic presets apply their size to the native window', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);
    final Size screen = sizes.screen;

    // A preset is only asserted when the display can hold it; otherwise the
    // window system may legitimately limit the window.
    const Size plain = Size(1100, 720);
    await _tap(tester, find.text('System TitleBar (1100×720)'));
    if (screen.width >= plain.width + 100 &&
        screen.height >= plain.height + 140) {
      await waitForSize(plain, reason: 'the System TitleBar preset');
    }

    const Size compact = Size(980, 660);
    await _tap(tester, find.text('Compact Immersive (980×660)'));
    if (screen.width >= compact.width + 100 &&
        screen.height >= compact.height + 140) {
      await waitForSize(compact, reason: 'the Compact Immersive preset');
    }

    const Size studio = Size(1280, 820);
    await _tap(tester, find.text('Studio Preset (1280×820 Centered)'));
    if (screen.width >= studio.width + 100 &&
        screen.height >= studio.height + 140) {
      await waitForSize(studio, reason: 'the Studio preset');
    }
    expect((await windowManager.getState()).isVisible, isTrue);
    expect(tester.takeException(), isNull);
  });

  windowTest('the Center button moves the window to the display center', (
    WidgetTester tester,
  ) async {
    if (!await positionsAreAuthoritative()) {
      markTestSkipped('Wayland compositors choose where windows are placed.');
      return;
    }
    await _launchStudio(tester);
    await windowManager.setPosition(const Offset(20, 40));
    final Rect corner = await waitForPosition(const Offset(20, 40));

    await _tap(tester, find.widgetWithText(ElevatedButton, 'Center'));

    await waitForBounds(
      (Rect bounds) => !offsetIsNear(bounds.topLeft, corner.topLeft, 5),
      reason: 'the Center button to move the window',
    );
    expect(tester.takeException(), isNull);
  });

  windowTest('the Maximize button toggles the native maximized state', (
    WidgetTester tester,
  ) async {
    if (!await ensureWindowSystemHonorsStateChanges()) {
      return;
    }
    await _launchStudio(tester);

    await _tap(tester, find.widgetWithText(ElevatedButton, 'Maximize'));
    await waitForState(
      (WindowStateSnapshot s) => s.isMaximized,
      reason: 'the Maximize button to maximize the window',
    );
    await _pumpUntilFound(
      tester,
      find.widgetWithText(ElevatedButton, 'Unmaximize'),
    );

    await _tap(tester, find.widgetWithText(ElevatedButton, 'Unmaximize'));
    await waitForState(
      (WindowStateSnapshot s) => !s.isMaximized,
      reason: 'the Unmaximize button to restore the window',
    );
    await _pumpUntilFound(
      tester,
      find.widgetWithText(ElevatedButton, 'Maximize'),
    );
  });

  windowTest('native events appear in the Studio event log', (
    WidgetTester tester,
  ) async {
    await _launchStudio(tester);

    await windowManager.setSize(sizes.smaller);

    await _pumpUntilFound(tester, find.textContaining('resized ->'));
    expect(tester.takeException(), isNull);
  });

  windowTest(
    'Request Close shows the confirmation dialog and Keep Open keeps the window',
    (WidgetTester tester) async {
      await _launchStudio(tester);
      // preventClose is on after the baseline reset. Without it the button would
      // destroy the window that is running the tests.
      expect((await windowManager.getState()).isPreventClose, isTrue);

      await _tap(tester, find.widgetWithText(ElevatedButton, 'Request Close'));

      await _pumpUntilFound(tester, find.text('Intercepted Window Close'));
      await tester.tap(find.text('Keep Open'));
      await _settle(tester);

      expect(find.text('Intercepted Window Close'), findsNothing);
      final WindowStateSnapshot state = await windowManager.getState();
      expect(state.isVisible, isTrue);
      expect(state.isPreventClose, isTrue);
    },
  );
}

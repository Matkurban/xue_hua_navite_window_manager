import 'package:flutter_test/flutter_test.dart';
import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager_example/controllers/window_studio_controller.dart';
import 'package:xue_hua_navite_window_manager_example/main.dart';

void main() {
  testWidgets('Verify WindowManagerExampleApp renders studio dashboard', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(const WindowManagerExampleApp());
    await tester.pump();

    expect(find.text('XueHua Native Window Manager Studio'), findsWidgets);
    expect(
      find.text(
        '1. Atomic Batch Configuration (Single-Channel Zero-Jump Presets)',
      ),
      findsOneWidget,
    );
    expect(
      find.text('5. Launch at Startup (Login Auto-Start)'),
      findsOneWidget,
    );
  });

  testWidgets('studio layout does not overflow at a minimize-sized window', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(159, 27);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(const WindowManagerExampleApp());
    await tester.pump();

    expect(tester.takeException(), isNull);
  });

  test(
    'WindowStudioController tracks WindowListener events and startup args',
    () {
      bool closeIntercepted = false;
      final WindowStudioController controller = WindowStudioController(
        onCloseIntercepted: () => closeIntercepted = true,
      );
      addTearDown(controller.dispose);

      expect(controller.currentStartupConfig.arguments, isEmpty);
      controller.setStartupPassMinimizedArg(true);
      expect(
        controller.currentStartupConfig.arguments,
        equals(const <String>['--minimized']),
      );

      controller.onWindowResize(const Size(960, 640));
      expect(controller.currentBounds.size, const Size(960, 640));
      expect(controller.eventLogs.first, contains('resized -> 960 x 640'));

      controller.onWindowCloseRequested();
      expect(closeIntercepted, isTrue);

      controller.clearLogs();
      expect(controller.eventLogs, isEmpty);
    },
  );
}

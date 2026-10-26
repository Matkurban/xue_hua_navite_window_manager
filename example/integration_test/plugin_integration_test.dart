import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('XueHuaNaviteWindowManager atomic configure integration test',
      (WidgetTester tester) async {
    final WindowStateSnapshot snapshot =
        await XueHuaNaviteWindowManager.configure(
      const WindowConfig(
        size: Size(1024, 720),
        minSize: Size(640, 480),
        center: true,
        title: 'Integration Test Window',
        titleBarStyle: TitleBarStyle.hidden,
        trafficLightsOffset: Offset(16, 16),
        showWhenReady: true,
      ),
    );
    expect(snapshot.bounds.width, greaterThan(0));
    expect(snapshot.bounds.height, greaterThan(0));
  });
}

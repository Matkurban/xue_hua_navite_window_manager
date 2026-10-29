// Integration tests for xue_hua_navite_window_manager.
//
// Run all of them, on a desktop with a display, from the example directory:
//
//   flutter test integration_test -d macos     # or: -d windows, -d linux
//
// This is the only `*_test.dart` file here on purpose. `flutter test` starts a
// fresh app for every test file, and on desktop (Flutter 3.47) every launch
// after the first fails with "The log reader stopped unexpectedly, or never
// started", because the tool reuses one closed log reader. Running every suite
// from this file keeps `flutter test integration_test` working: one launch, one
// native window, all suites.
//
// A single suite can still be run on its own:
//
//   flutter test integration_test/window_geometry_suite.dart -d macos
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import 'launch_at_startup_suite.dart' as launch_at_startup;
import 'studio_app_suite.dart' as studio;
import 'window_behavior_suite.dart' as behavior;
import 'window_events_suite.dart' as events;
import 'window_geometry_suite.dart' as geometry;
import 'window_lifecycle_suite.dart' as lifecycle;
import 'window_state_suite.dart' as state;
import 'window_widgets_suite.dart' as widgets;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('XueHuaNaviteWindowManager atomic configure integration test', (
    WidgetTester tester,
  ) async {
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

  group('state', state.main);
  group('geometry', geometry.main);
  group('lifecycle', lifecycle.main);
  group('behavior', behavior.main);
  group('events', events.main);
  group('widgets', widgets.main);
  group('studio', studio.main);
  group('launchAtStartup', launch_at_startup.main);
}

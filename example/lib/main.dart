import 'dart:async';

import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';
import 'package:xue_hua_navite_window_manager_example/app_theme.dart';

import 'window_studio_dashboard.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Atomically configure the desktop window in a single platform call before
  // revealing it on the first rendered Flutter frame (Zero-Flash Startup).
  try {
    await XueHuaNaviteWindowManager.configure(
      const WindowConfig(
        size: Size(1240, 820),
        minSize: Size(860, 580),
        // maxSize: Size(2560, 1600),
        center: true,
        title: 'XueHua Native Window Manager Studio',
        titleBarStyle: TitleBarStyle.hidden,
        trafficLightsOffset: Offset(16, 14),
        trafficLightsVisible: true,
        backgroundColor: Color(0xFF141622),
        alwaysOnTop: false,
        resizable: true,
        preventClose: true,
        hasShadow: true,
        resizeBorderWidth: 6.0,
        showWhenReady: true,
      ),
    );
  } catch (_) {
    // Gracefully ignore platform channel errors in headless widget test environments.
  }

  runApp(const WindowManagerExampleApp());
}

/// Root application widget demonstrating all features of `xue_hua_navite_window_manager`.
class WindowManagerExampleApp extends StatelessWidget {
  const WindowManagerExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'XueHua Native Window Manager Studio',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light,
      darkTheme: AppTheme.dark,
      themeMode: ThemeMode.dark,
      home: const WindowStudioDashboard(),
    );
  }
}

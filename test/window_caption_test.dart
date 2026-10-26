import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:material_ui/material_ui.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final _CaptionFakePlatform platform = _CaptionFakePlatform();

  setUp(() {
    XueHuaNaviteWindowManagerPlatform.instance = platform;
    platform.maximizeCount = 0;
    platform.unmaximizeCount = 0;
    platform.maximized = false;
  });

  testWidgets('WindowCaption does not overflow at minimize-sized width', (
    WidgetTester tester,
  ) async {
    await _pumpCaption(
      tester,
      const Size(159, 27),
      title: const Row(
        children: <Widget>[
          Icon(Icons.desktop_windows_rounded),
          SizedBox(width: 10),
          Text('XueHua Native Window Manager Studio'),
          SizedBox(width: 12),
          Text('ACTIVE'),
        ],
      ),
      actions: const <Widget>[
        Icon(Icons.filter_center_focus),
        Icon(Icons.fullscreen_rounded),
        SizedBox(width: 4),
      ],
    );

    expect(tester.takeException(), isNull);
    expect(find.byIcon(Icons.crop_square), findsOneWidget);
  });

  testWidgets('WindowCaption keeps the title and controls at a normal width', (
    WidgetTester tester,
  ) async {
    await _pumpCaption(
      tester,
      const Size(960, 44),
      title: const Text('Studio Title'),
      actions: const <Widget>[Icon(Icons.filter_center_focus)],
    );

    expect(tester.takeException(), isNull);
    expect(find.text('Studio Title'), findsOneWidget);
    expect(find.byTooltip('Minimize'), findsOneWidget);
    expect(find.byTooltip('Close'), findsOneWidget);
  });

  testWidgets('maximize control toggles on tap and on snapButtonClicked', (
    WidgetTester tester,
  ) async {
    await _pumpCaption(
      tester,
      const Size(960, 44),
      title: const Text('Studio Title'),
    );

    await tester.tap(find.byIcon(Icons.crop_square));
    await tester.pump();
    expect(platform.maximizeCount, 1);
    expect(platform.maximized, isTrue);
  });

  testWidgets('SnapLayoutButton onPressed replaces the default toggle', (
    WidgetTester tester,
  ) async {
    var pressed = 0;
    await tester.pumpWidget(
      MaterialApp(
        home: Center(
          child: SnapLayoutButton(
            onPressed: () => pressed++,
            builder: (BuildContext context, bool hovered, bool maximized) {
              return const SizedBox(width: 46, height: 40, child: Text('max'));
            },
          ),
        ),
      ),
    );
    await tester.pump();

    await tester.tap(find.text('max'));
    await tester.pump();
    expect(pressed, 1);
    expect(platform.maximizeCount, 0);
  });
}

Future<void> _pumpCaption(
  WidgetTester tester,
  Size size, {
  required Widget title,
  List<Widget> actions = const <Widget>[],
}) async {
  await tester.pumpWidget(
    MaterialApp(
      home: Center(
        child: SizedBox(
          width: size.width,
          height: size.height,
          child: WindowCaption(height: 44, title: title, actions: actions),
        ),
      ),
    ),
  );
  await tester.pump();
}

class _CaptionFakePlatform extends XueHuaNaviteWindowManagerPlatform
    with MockPlatformInterfaceMixin {
  final StreamController<WindowEvent> events =
      StreamController<WindowEvent>.broadcast();

  int maximizeCount = 0;
  int unmaximizeCount = 0;
  bool maximized = false;

  @override
  Stream<WindowEvent> get windowEvents => events.stream;

  @override
  Future<void> ensureInitialized() async {}

  @override
  Future<WindowStateSnapshot> getState() async {
    return WindowStateSnapshot(
      bounds: const Rect.fromLTWH(0, 0, 960, 640),
      isVisible: true,
      isMaximized: maximized,
      isMinimized: false,
      isFullScreen: false,
      isFocused: true,
      isAlwaysOnTop: false,
      isResizable: true,
      isPreventClose: false,
    );
  }

  @override
  Future<void> maximize() async {
    maximizeCount++;
    maximized = true;
  }

  @override
  Future<void> unmaximize() async {
    unmaximizeCount++;
    maximized = false;
  }

  @override
  Future<void> setSnapLayoutButtonRect(Rect? rect) async {}

  @override
  Future<void> setLinuxInteractiveRegion({
    required int id,
    required bool present,
    String kind = 'move',
    Rect? rect,
  }) async {}
}

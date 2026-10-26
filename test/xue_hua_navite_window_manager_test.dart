import 'dart:async';
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

class MockXueHuaNaviteWindowManagerPlatform
    with MockPlatformInterfaceMixin
    implements XueHuaNaviteWindowManagerPlatform {
  final StreamController<WindowEvent> eventController =
      StreamController<WindowEvent>.broadcast();

  WindowConfig? lastConfig;
  bool readyMarked = false;
  Rect? snapRect;

  @override
  Stream<WindowEvent> get windowEvents => eventController.stream;

  @override
  Future<void> ensureInitialized() async {}

  @override
  Future<WindowStateSnapshot> configure(WindowConfig config) async {
    lastConfig = config;
    return WindowStateSnapshot(
      bounds: Rect.fromLTWH(
        config.position?.dx ?? 100,
        config.position?.dy ?? 100,
        config.size?.width ?? 1024,
        config.size?.height ?? 768,
      ),
      isVisible: true,
      isMaximized: false,
      isMinimized: false,
      isFullScreen: false,
      isFocused: true,
      isAlwaysOnTop: config.alwaysOnTop ?? false,
      isResizable: config.resizable ?? true,
      isPreventClose: config.preventClose ?? false,
    );
  }

  @override
  Future<void> markReadyToShow() async {
    readyMarked = true;
  }

  @override
  Future<WindowStateSnapshot> getState() async {
    return const WindowStateSnapshot(
      bounds: Rect.fromLTWH(100, 100, 1024, 768),
      isVisible: true,
      isMaximized: false,
      isMinimized: false,
      isFullScreen: false,
      isFocused: true,
      isAlwaysOnTop: false,
      isResizable: true,
      isPreventClose: false,
    );
  }

  @override
  Future<void> blur() async {}

  @override
  Future<bool> center({Size? size, bool animate = false}) async => true;

  @override
  Future<void> setLinuxInteractiveRegion({
    required int id,
    required bool present,
    String kind = 'move',
    Rect? rect,
  }) async {}

  @override
  Future<void> close() async {}

  @override
  Future<void> destroy() async {}

  @override
  Future<void> focus() async {}

  @override
  Future<Rect> getBounds() async => const Rect.fromLTWH(100, 100, 1024, 768);

  @override
  Future<Offset> getPosition() async => const Offset(100, 100);

  @override
  Future<Size> getSize() async => const Size(1024, 768);

  @override
  Future<void> hide() async {}

  @override
  Future<void> maximize() async {}

  @override
  Future<void> minimize() async {}

  @override
  Future<void> restore() async {}

  @override
  Future<void> setAlwaysOnTop(bool alwaysOnTop) async {}

  @override
  Future<void> setBackgroundColor(Color color) async {}

  @override
  Future<void> setBounds(Rect bounds, {bool animate = false}) async {}

  @override
  Future<void> setFullScreen(bool isFullScreen) async {}

  @override
  Future<void> setHasShadow(bool hasShadow) async {}

  @override
  Future<void> setMaximumSize(Size size) async {}

  @override
  Future<void> setMinimumSize(Size size) async {}

  @override
  Future<void> setOpacity(double opacity) async {}

  @override
  Future<void> setPosition(Offset position, {bool animate = false}) async {}

  @override
  Future<void> setPreventClose(bool preventClose) async {}

  @override
  Future<void> setResizable(bool resizable) async {}

  @override
  Future<void> setSize(Size size, {bool animate = false}) async {}

  @override
  Future<void> setSnapLayoutButtonRect(Rect? rect) async {
    snapRect = rect;
  }

  @override
  Future<void> setTitle(String title) async {}

  @override
  Future<void> setTitleBarDragRects({
    required List<Rect> dragRects,
    List<Rect> excludeRects = const <Rect>[],
  }) async {}

  @override
  Future<void> setTitleBarStyle(
    TitleBarStyle style, {
    Offset? trafficLightsOffset,
    bool? trafficLightsVisible,
  }) async {}

  @override
  Future<void> setTrafficLightsOffset(Offset offset) async {}

  @override
  Future<void> setTrafficLightsVisible(bool visible) async {}

  @override
  Future<void> show({bool focus = true}) async {}

  @override
  Future<void> startDragging() async {}

  @override
  Future<void> startResizing(ResizeEdge edge) async {}

  @override
  Future<void> unmaximize() async {}
}

class _TestWindowListener with WindowListener {
  bool closeRequested = false;
  Size? resizedTo;

  @override
  void onWindowCloseRequested() {
    closeRequested = true;
  }

  @override
  void onWindowResize(Size size) {
    resizedTo = size;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final XueHuaNaviteWindowManagerPlatform initialPlatform =
      XueHuaNaviteWindowManagerPlatform.instance;

  test('$MethodChannelXueHuaNaviteWindowManager is the default instance', () {
    expect(
      initialPlatform,
      isInstanceOf<MethodChannelXueHuaNaviteWindowManager>(),
    );
  });

  test('XueHuaNaviteWindowManager.configure and listener dispatch', () async {
    final MockXueHuaNaviteWindowManagerPlatform fakePlatform =
        MockXueHuaNaviteWindowManagerPlatform();
    XueHuaNaviteWindowManagerPlatform.instance = fakePlatform;

    final _TestWindowListener listener = _TestWindowListener();
    XueHuaNaviteWindowManager.instance.addListener(listener);

    final WindowStateSnapshot snapshot =
        await XueHuaNaviteWindowManager.configure(
          const WindowConfig(
            size: Size(1200, 800),
            center: true,
            titleBarStyle: TitleBarStyle.hidden,
            preventClose: true,
            showWhenReady: false,
          ),
        );

    expect(snapshot.bounds.size, const Size(1200, 800));
    expect(snapshot.isPreventClose, isTrue);

    fakePlatform.eventController.add(
      const WindowEvent(type: WindowEventType.closeRequested, timestamp: 1000),
    );
    fakePlatform.eventController.add(
      const WindowEvent(
        type: WindowEventType.resized,
        bounds: Rect.fromLTWH(100, 100, 1440, 900),
        timestamp: 1001,
      ),
    );

    await Future<void>.delayed(Duration.zero);
    expect(listener.closeRequested, isTrue);
    expect(listener.resizedTo, const Size(1440, 900));

    XueHuaNaviteWindowManager.instance.removeListener(listener);
  });
}

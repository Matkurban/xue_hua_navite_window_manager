import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final MethodChannelXueHuaNaviteWindowManager platform =
      MethodChannelXueHuaNaviteWindowManager();
  const MethodChannel channel = MethodChannel(
    'xue_hua_navite_window_manager/methods',
  );

  final List<MethodCall> log = <MethodCall>[];

  setUp(() {
    log.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
          log.add(methodCall);
          switch (methodCall.method) {
            case 'configure':
            case 'getState':
              return <String, dynamic>{
                'x': 100.0,
                'y': 120.0,
                'width': 1280.0,
                'height': 800.0,
                'isVisible': true,
                'isMaximized': false,
                'isMinimized': false,
                'isFullScreen': false,
                'isFocused': true,
                'isAlwaysOnTop': false,
                'isResizable': true,
                'isPreventClose': true,
              };
            case 'getBounds':
              return <String, dynamic>{
                'x': 100.0,
                'y': 120.0,
                'width': 1280.0,
                'height': 800.0,
              };
            default:
              return null;
          }
        });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  test('configure sends atomic WindowConfig map over MethodChannel', () async {
    const WindowConfig config = WindowConfig(
      size: Size(1280, 800),
      minSize: Size(800, 600),
      maxSize: Size(2560, 1440),
      center: true,
      title: 'XueHua Desktop Studio',
      titleBarStyle: TitleBarStyle.hidden,
      trafficLightsOffset: Offset(16, 18),
      backgroundColor: Color(0xFF1E1E2E),
      alwaysOnTop: false,
      resizable: true,
      preventClose: true,
      showWhenReady: true,
    );

    final WindowStateSnapshot snapshot = await platform.configure(config);
    expect(snapshot.bounds, const Rect.fromLTWH(100, 120, 1280, 800));
    expect(snapshot.isPreventClose, isTrue);

    expect(log, hasLength(1));
    expect(log.first.method, 'configure');
    final Map<Object?, Object?> args =
        log.first.arguments as Map<Object?, Object?>;
    expect(args['width'], 1280.0);
    expect(args['height'], 800.0);
    expect(args['minWidth'], 800.0);
    expect(args['minHeight'], 600.0);
    expect(args['center'], true);
    expect(args['title'], 'XueHua Desktop Studio');
    expect(args['titleBarStyle'], 'hidden');
    expect(args['trafficLightsOffsetX'], 16.0);
    expect(args['trafficLightsOffsetY'], 18.0);
    expect(args['backgroundColor'], 0xFF1E1E2E);
    expect(args['preventClose'], true);
  });

  test(
    'startDragging and startResizing invoke corresponding native methods',
    () async {
      await platform.startDragging();
      await platform.startResizing(ResizeEdge.bottomRight);

      expect(log, hasLength(2));
      expect(log[0].method, 'startDragging');
      expect(log[1].method, 'startResizing');
      expect(
        (log[1].arguments as Map<Object?, Object?>)['edge'],
        'bottomRight',
      );
    },
  );

  test('center treats a missing native result as applied', () async {
    expect(await platform.center(), isTrue);
    expect(log.single.method, 'center');
  });

  test(
    'center returns false when Linux reports the move was not applied',
    () async {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
            log.add(methodCall);
            return false;
          });
      expect(await platform.center(size: const Size(640, 480)), isFalse);
      final Map<Object?, Object?> args =
          log.single.arguments as Map<Object?, Object?>;
      expect(args['width'], 640.0);
      expect(args['height'], 480.0);
    },
  );

  test('setLinuxInteractiveRegion sends the hit rect', () async {
    await platform.setLinuxInteractiveRegion(
      id: 7,
      present: true,
      kind: 'move',
      rect: const Rect.fromLTWH(0, 0, 320, 40),
    );
    await platform.setLinuxInteractiveRegion(id: 7, present: false);

    expect(log, hasLength(2));
    expect(log[0].method, 'setLinuxInteractiveRegion');
    final Map<Object?, Object?> present =
        log[0].arguments as Map<Object?, Object?>;
    expect(present['id'], 7);
    expect(present['present'], isTrue);
    expect(present['kind'], 'move');
    expect(present['width'], 320.0);
    expect(present['height'], 40.0);
    expect((log[1].arguments as Map<Object?, Object?>)['present'], isFalse);
  });

  test('snapshot marks a missing position flag as authoritative', () {
    const WindowStateSnapshot snapshot = WindowStateSnapshot(
      bounds: Rect.fromLTWH(0, 0, 10, 10),
      isVisible: true,
      isMaximized: false,
      isMinimized: false,
      isFullScreen: false,
      isFocused: true,
      isAlwaysOnTop: false,
      isResizable: true,
      isPreventClose: false,
    );
    expect(snapshot.isPositionAuthoritative, isTrue);
    expect(
      WindowStateSnapshot.fromMap(<Object?, Object?>{
        'isPositionAuthoritative': false,
      }).isPositionAuthoritative,
      isFalse,
    );
  });

  test(
    'setSnapLayoutButtonRect sends coordinates or disables when null',
    () async {
      await platform.setSnapLayoutButtonRect(
        const Rect.fromLTWH(900, 0, 46, 40),
      );
      await platform.setSnapLayoutButtonRect(null);

      expect(log, hasLength(2));
      expect(log[0].method, 'setSnapLayoutButtonRect');
      expect((log[0].arguments as Map<Object?, Object?>)['enabled'], true);
      expect((log[1].arguments as Map<Object?, Object?>)['enabled'], false);
    },
  );
}

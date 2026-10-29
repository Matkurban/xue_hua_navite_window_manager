import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// One platform channel call: how Dart makes it, and what native receives.
class _Call {
  const _Call(this.name, this.invoke, this.method, [this.arguments]);

  final String name;
  final Future<Object?> Function(MethodChannelXueHuaNaviteWindowManager p)
  invoke;
  final String method;
  final Map<String, Object?>? arguments;
}

final List<_Call> _calls = <_Call>[
  _Call('ensureInitialized', (p) => p.ensureInitialized(), 'ensureInitialized'),
  _Call(
    'configure',
    (p) => p.configure(
      const WindowConfig(
        size: Size(640, 480),
        center: true,
        showWhenReady: false,
      ),
    ),
    'configure',
    <String, Object?>{
      'width': 640.0,
      'height': 480.0,
      'center': true,
      'showWhenReady': false,
    },
  ),
  _Call('markReadyToShow', (p) => p.markReadyToShow(), 'markReadyToShow'),
  _Call('getState', (p) => p.getState(), 'getState'),
  _Call('show', (p) => p.show(), 'show', <String, Object?>{'focus': true}),
  _Call(
    'show without focus',
    (p) => p.show(focus: false),
    'show',
    <String, Object?>{'focus': false},
  ),
  _Call('hide', (p) => p.hide(), 'hide'),
  _Call('focus', (p) => p.focus(), 'focus'),
  _Call('blur', (p) => p.blur(), 'blur'),
  _Call('close', (p) => p.close(), 'close'),
  _Call('destroy', (p) => p.destroy(), 'destroy'),
  _Call('maximize', (p) => p.maximize(), 'maximize'),
  _Call('unmaximize', (p) => p.unmaximize(), 'unmaximize'),
  _Call('minimize', (p) => p.minimize(), 'minimize'),
  _Call('restore', (p) => p.restore(), 'restore'),
  _Call(
    'setFullScreen',
    (p) => p.setFullScreen(true),
    'setFullScreen',
    <String, Object?>{'isFullScreen': true},
  ),
  _Call(
    'setBounds',
    (p) => p.setBounds(const Rect.fromLTWH(10, 20, 300, 200)),
    'setBounds',
    <String, Object?>{
      'x': 10.0,
      'y': 20.0,
      'width': 300.0,
      'height': 200.0,
      'animate': false,
    },
  ),
  _Call(
    'setBounds animated',
    (p) => p.setBounds(const Rect.fromLTWH(1, 2, 3, 4), animate: true),
    'setBounds',
    <String, Object?>{
      'x': 1.0,
      'y': 2.0,
      'width': 3.0,
      'height': 4.0,
      'animate': true,
    },
  ),
  _Call('getBounds', (p) => p.getBounds(), 'getBounds'),
  _Call(
    'setSize',
    (p) => p.setSize(const Size(500, 400)),
    'setSize',
    <String, Object?>{'width': 500.0, 'height': 400.0, 'animate': false},
  ),
  _Call(
    'setPosition',
    (p) => p.setPosition(const Offset(30, 40), animate: true),
    'setPosition',
    <String, Object?>{'x': 30.0, 'y': 40.0, 'animate': true},
  ),
  _Call('center', (p) => p.center(), 'center', <String, Object?>{
    'animate': false,
  }),
  _Call(
    'center with a size',
    (p) => p.center(size: const Size(640, 480), animate: true),
    'center',
    <String, Object?>{'width': 640.0, 'height': 480.0, 'animate': true},
  ),
  _Call(
    'setMinimumSize',
    (p) => p.setMinimumSize(const Size(320, 240)),
    'setMinimumSize',
    <String, Object?>{'width': 320.0, 'height': 240.0},
  ),
  _Call(
    'setMaximumSize',
    (p) => p.setMaximumSize(const Size(1920, 1080)),
    'setMaximumSize',
    <String, Object?>{'width': 1920.0, 'height': 1080.0},
  ),
  _Call(
    'setResizable',
    (p) => p.setResizable(false),
    'setResizable',
    <String, Object?>{'resizable': false},
  ),
  _Call(
    'setAlwaysOnTop',
    (p) => p.setAlwaysOnTop(true),
    'setAlwaysOnTop',
    <String, Object?>{'alwaysOnTop': true},
  ),
  _Call(
    'setPreventClose',
    (p) => p.setPreventClose(true),
    'setPreventClose',
    <String, Object?>{'preventClose': true},
  ),
  _Call(
    'setTitle',
    (p) => p.setTitle('Hello 雪花'),
    'setTitle',
    <String, Object?>{'title': 'Hello 雪花'},
  ),
  _Call(
    'setTitleBarStyle without options',
    (p) => p.setTitleBarStyle(TitleBarStyle.hidden),
    'setTitleBarStyle',
    <String, Object?>{'titleBarStyle': 'hidden'},
  ),
  _Call(
    'setTitleBarStyle with traffic lights',
    (p) => p.setTitleBarStyle(
      TitleBarStyle.immersive,
      trafficLightsOffset: const Offset(18, 16),
      trafficLightsVisible: false,
    ),
    'setTitleBarStyle',
    <String, Object?>{
      'titleBarStyle': 'immersive',
      'trafficLightsOffsetX': 18.0,
      'trafficLightsOffsetY': 16.0,
      'trafficLightsVisible': false,
    },
  ),
  _Call(
    'setTrafficLightsOffset',
    (p) => p.setTrafficLightsOffset(const Offset(20, 22)),
    'setTrafficLightsOffset',
    <String, Object?>{'x': 20.0, 'y': 22.0},
  ),
  _Call(
    'setTrafficLightsVisible',
    (p) => p.setTrafficLightsVisible(false),
    'setTrafficLightsVisible',
    <String, Object?>{'visible': false},
  ),
  _Call(
    'setBackgroundColor',
    (p) => p.setBackgroundColor(const Color(0xCC336699)),
    'setBackgroundColor',
    <String, Object?>{'backgroundColor': 0xCC336699},
  ),
  _Call('setOpacity', (p) => p.setOpacity(0.4), 'setOpacity', <String, Object?>{
    'opacity': 0.4,
  }),
  _Call(
    'setOpacity above the range is clamped to 1',
    (p) => p.setOpacity(1.7),
    'setOpacity',
    <String, Object?>{'opacity': 1.0},
  ),
  _Call(
    'setOpacity below the range is clamped to 0',
    (p) => p.setOpacity(-0.3),
    'setOpacity',
    <String, Object?>{'opacity': 0.0},
  ),
  _Call(
    'setHasShadow',
    (p) => p.setHasShadow(false),
    'setHasShadow',
    <String, Object?>{'hasShadow': false},
  ),
  _Call('startDragging', (p) => p.startDragging(), 'startDragging'),
  _Call(
    'startResizing',
    (p) => p.startResizing(ResizeEdge.topLeft),
    'startResizing',
    <String, Object?>{'edge': 'topLeft'},
  ),
  _Call(
    'setSnapLayoutButtonRect',
    (p) => p.setSnapLayoutButtonRect(const Rect.fromLTWH(900, 0, 46, 40)),
    'setSnapLayoutButtonRect',
    <String, Object?>{
      'enabled': true,
      'x': 900.0,
      'y': 0.0,
      'width': 46.0,
      'height': 40.0,
    },
  ),
  _Call(
    'setSnapLayoutButtonRect clears with null',
    (p) => p.setSnapLayoutButtonRect(null),
    'setSnapLayoutButtonRect',
    <String, Object?>{'enabled': false},
  ),
  _Call(
    'setLinuxInteractiveRegion',
    (p) => p.setLinuxInteractiveRegion(
      id: 3,
      present: true,
      kind: 'bottomRight',
      rect: const Rect.fromLTWH(1, 2, 3, 4),
    ),
    'setLinuxInteractiveRegion',
    <String, Object?>{
      'id': 3,
      'present': true,
      'kind': 'bottomRight',
      'x': 1.0,
      'y': 2.0,
      'width': 3.0,
      'height': 4.0,
    },
  ),
  _Call(
    'setLinuxInteractiveRegion removal',
    (p) => p.setLinuxInteractiveRegion(id: 3, present: false),
    'setLinuxInteractiveRegion',
    <String, Object?>{'id': 3, 'present': false, 'kind': 'move'},
  ),
  _Call(
    'setTitleBarDragRects',
    (p) => p.setTitleBarDragRects(
      dragRects: const <Rect>[Rect.fromLTWH(0, 0, 100, 40)],
      excludeRects: const <Rect>[Rect.fromLTWH(10, 5, 20, 30)],
    ),
    'setTitleBarDragRects',
    <String, Object?>{
      'dragRects': <Map<String, double>>[
        <String, double>{'x': 0.0, 'y': 0.0, 'width': 100.0, 'height': 40.0},
      ],
      'excludeRects': <Map<String, double>>[
        <String, double>{'x': 10.0, 'y': 5.0, 'width': 20.0, 'height': 30.0},
      ],
    },
  ),
  _Call(
    'getLaunchAtStartupState default',
    (p) => p.getLaunchAtStartupState(),
    'getLaunchAtStartupState',
  ),
  _Call(
    'getLaunchAtStartupState with config',
    (p) => p.getLaunchAtStartupState(
      config: const LaunchAtStartupConfig(
        appName: 'XueHua Studio',
        identifier: 'com.xuehua.studio',
        executablePath: '/opt/xuehua/app',
        arguments: <String>['--minimized'],
        msixTaskId: 'StudioStartupTask',
        macOSMode: MacOSLaunchAtStartupMode.launchAgent,
      ),
    ),
    'getLaunchAtStartupState',
    <String, Object?>{
      'appName': 'XueHua Studio',
      'identifier': 'com.xuehua.studio',
      'executablePath': '/opt/xuehua/app',
      'arguments': <String>['--minimized'],
      'msixTaskId': 'StudioStartupTask',
      'macOSMode': 'launchAgent',
    },
  ),
  _Call(
    'setLaunchAtStartup enable',
    (p) => p.setLaunchAtStartup(
      true,
      config: const LaunchAtStartupConfig(
        identifier: 'com.xuehua.studio',
        arguments: <String>['--silent'],
      ),
    ),
    'setLaunchAtStartup',
    <String, Object?>{
      'enabled': true,
      'identifier': 'com.xuehua.studio',
      'arguments': <String>['--silent'],
    },
  ),
  _Call(
    'setLaunchAtStartup disable',
    (p) => p.setLaunchAtStartup(false),
    'setLaunchAtStartup',
    <String, Object?>{'enabled': false},
  ),
  _Call(
    'openLaunchAtStartupSettings',
    (p) => p.openLaunchAtStartupSettings(),
    'openLaunchAtStartupSettings',
  ),
];

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const MethodChannel channel = MethodChannel(
    'xue_hua_navite_window_manager/methods',
  );
  final List<MethodCall> log = <MethodCall>[];

  setUp(() {
    log.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
          log.add(call);
          switch (call.method) {
            case 'configure':
            case 'getState':
              return <String, Object?>{
                'x': 1.0,
                'y': 2.0,
                'width': 300.0,
                'height': 200.0,
              };
            case 'getBounds':
              return <String, Object?>{
                'x': 1.0,
                'y': 2.0,
                'width': 300.0,
                'height': 200.0,
              };
            case 'getLaunchAtStartupState':
            case 'setLaunchAtStartup':
              return <String, Object?>{
                'isSupported': true,
                'isEnabled': true,
                'status': 'enabled',
                'backend': 'macosAppService',
                'identifier': 'com.xuehua.studio',
                'executablePath':
                    '/Applications/Studio.app/Contents/MacOS/Studio',
                'canOpenSettings': true,
              };
            case 'openLaunchAtStartupSettings':
              return true;
            default:
              return null;
          }
        });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  for (final _Call call in _calls) {
    test('${call.name} sends ${call.method}', () async {
      final MethodChannelXueHuaNaviteWindowManager platform =
          MethodChannelXueHuaNaviteWindowManager();

      await call.invoke(platform);

      expect(log, hasLength(1));
      expect(log.single.method, call.method);
      if (call.arguments == null) {
        expect(log.single.arguments, isNull);
      } else {
        expect(log.single.arguments, call.arguments);
      }
    });
  }

  test('every platform interface method has a case above', () {
    // Each case names the channel method it expects. This list is the full
    // set of methods the native plugins must implement.
    final Set<String> covered = _calls.map((_Call c) => c.method).toSet();

    expect(covered, <String>{
      'ensureInitialized',
      'configure',
      'markReadyToShow',
      'getState',
      'show',
      'hide',
      'focus',
      'blur',
      'close',
      'destroy',
      'maximize',
      'unmaximize',
      'minimize',
      'restore',
      'setFullScreen',
      'setBounds',
      'getBounds',
      'setSize',
      'setPosition',
      'center',
      'setMinimumSize',
      'setMaximumSize',
      'setResizable',
      'setAlwaysOnTop',
      'setPreventClose',
      'setTitle',
      'setTitleBarStyle',
      'setTrafficLightsOffset',
      'setTrafficLightsVisible',
      'setBackgroundColor',
      'setOpacity',
      'setHasShadow',
      'startDragging',
      'startResizing',
      'setSnapLayoutButtonRect',
      'setLinuxInteractiveRegion',
      'setTitleBarDragRects',
      'getLaunchAtStartupState',
      'setLaunchAtStartup',
      'openLaunchAtStartupSettings',
    });
  });

  group('results', () {
    test('getSize and getPosition read the bounds', () async {
      final MethodChannelXueHuaNaviteWindowManager platform =
          MethodChannelXueHuaNaviteWindowManager();

      expect(await platform.getSize(), const Size(300, 200));
      expect(await platform.getPosition(), const Offset(1, 2));
      expect(log.map((MethodCall c) => c.method), <String>[
        'getBounds',
        'getBounds',
      ]);
    });

    test(
      'getBounds falls back to 800x600 when native returns nothing',
      () async {
        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
            .setMockMethodCallHandler(channel, (MethodCall call) async => null);

        expect(
          await MethodChannelXueHuaNaviteWindowManager().getBounds(),
          const Rect.fromLTWH(0, 0, 800, 600),
        );
      },
    );

    test('configure and getState return the native snapshot', () async {
      final MethodChannelXueHuaNaviteWindowManager platform =
          MethodChannelXueHuaNaviteWindowManager();

      final WindowStateSnapshot configured = await platform.configure(
        const WindowConfig(),
      );
      final WindowStateSnapshot state = await platform.getState();

      expect(configured.bounds, const Rect.fromLTWH(1, 2, 300, 200));
      expect(state.bounds, const Rect.fromLTWH(1, 2, 300, 200));
    });

    test('a native error surfaces as a PlatformException', () async {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (MethodCall call) async {
            throw PlatformException(
              code: 'NO_WINDOW',
              message: 'Unable to resolve main window.',
            );
          });

      await expectLater(
        MethodChannelXueHuaNaviteWindowManager().getState(),
        throwsA(
          isA<PlatformException>().having(
            (PlatformException e) => e.code,
            'code',
            'NO_WINDOW',
          ),
        ),
      );
    });

    test('center reports a native false and treats null as applied', () async {
      final MethodChannelXueHuaNaviteWindowManager platform =
          MethodChannelXueHuaNaviteWindowManager();
      expect(await platform.center(), isTrue);

      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (MethodCall call) async => false);
      expect(await platform.center(), isFalse);

      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, (MethodCall call) async => true);
      expect(await platform.center(), isTrue);
    });

    test(
      'launch-at-startup methods parse native payload and handle null safely',
      () async {
        final MethodChannelXueHuaNaviteWindowManager platform =
            MethodChannelXueHuaNaviteWindowManager();

        final LaunchAtStartupState queried = await platform
            .getLaunchAtStartupState();
        expect(queried.isSupported, isTrue);
        expect(queried.isEnabled, isTrue);
        expect(queried.status, LaunchAtStartupStatus.enabled);
        expect(queried.backend, LaunchAtStartupBackend.macosAppService);
        expect(queried.identifier, 'com.xuehua.studio');
        expect(queried.canOpenSettings, isTrue);
        expect(await platform.openLaunchAtStartupSettings(), isTrue);

        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
            .setMockMethodCallHandler(channel, (MethodCall call) async => null);
        final LaunchAtStartupState fallback = await platform.setLaunchAtStartup(
          true,
        );
        expect(fallback.isSupported, isFalse);
        expect(fallback.isEnabled, isFalse);
        expect(fallback.status, LaunchAtStartupStatus.unsupported);
        expect(await platform.openLaunchAtStartupSettings(), isFalse);
      },
    );
  });

  group('windowEvents', () {
    const EventChannel events = EventChannel(
      'xue_hua_navite_window_manager/events',
    );

    tearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockStreamHandler(events, null);
    });

    test('delivers parsed events and drops malformed ones', () async {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockStreamHandler(
            events,
            MockStreamHandler.inline(
              onListen: (Object? arguments, MockStreamHandlerEventSink sink) {
                sink.success(<String, Object?>{
                  'type': 'maximized',
                  'width': 800,
                  'height': 600,
                  'isMaximized': true,
                  'timestamp': 42,
                });
                sink.success('not a map');
                sink.success(<String, Object?>{'type': 'noSuchEvent'});
                sink.success(<String, Object?>{
                  'type': 'closeRequested',
                  'timestamp': 43,
                });
                sink.endOfStream();
              },
            ),
          );

      final List<WindowEvent> received =
          await MethodChannelXueHuaNaviteWindowManager().windowEvents.toList();

      expect(received.map((WindowEvent e) => e.type), <WindowEventType>[
        WindowEventType.maximized,
        WindowEventType.closeRequested,
      ]);
      expect(received.first.bounds, const Rect.fromLTWH(0, 0, 800, 600));
      expect(received.first.isMaximized, isTrue);
    });

    test('is one broadcast stream per platform instance', () {
      final MethodChannelXueHuaNaviteWindowManager platform =
          MethodChannelXueHuaNaviteWindowManager();

      expect(identical(platform.windowEvents, platform.windowEvents), isTrue);
      expect(platform.windowEvents.isBroadcast, isTrue);
    });
  });

  group('the abstract platform interface', () {
    test('every method throws UnimplementedError until overridden', () async {
      final XueHuaNaviteWindowManagerPlatform bare = _BarePlatform();

      expect(() => bare.windowEvents, throwsUnimplementedError);
      expect(bare.ensureInitialized, throwsUnimplementedError);
      expect(
        () => bare.configure(const WindowConfig()),
        throwsUnimplementedError,
      );
      expect(bare.markReadyToShow, throwsUnimplementedError);
      expect(bare.getState, throwsUnimplementedError);
      expect(bare.show, throwsUnimplementedError);
      expect(bare.hide, throwsUnimplementedError);
      expect(bare.focus, throwsUnimplementedError);
      expect(bare.blur, throwsUnimplementedError);
      expect(bare.close, throwsUnimplementedError);
      expect(bare.destroy, throwsUnimplementedError);
      expect(bare.maximize, throwsUnimplementedError);
      expect(bare.unmaximize, throwsUnimplementedError);
      expect(bare.minimize, throwsUnimplementedError);
      expect(bare.restore, throwsUnimplementedError);
      expect(() => bare.setFullScreen(true), throwsUnimplementedError);
      expect(() => bare.setBounds(Rect.zero), throwsUnimplementedError);
      expect(bare.getBounds, throwsUnimplementedError);
      expect(() => bare.setSize(Size.zero), throwsUnimplementedError);
      expect(bare.getSize, throwsUnimplementedError);
      expect(() => bare.setPosition(Offset.zero), throwsUnimplementedError);
      expect(bare.getPosition, throwsUnimplementedError);
      expect(bare.center, throwsUnimplementedError);
      expect(() => bare.setMinimumSize(Size.zero), throwsUnimplementedError);
      expect(() => bare.setMaximumSize(Size.zero), throwsUnimplementedError);
      expect(() => bare.setResizable(true), throwsUnimplementedError);
      expect(() => bare.setAlwaysOnTop(true), throwsUnimplementedError);
      expect(() => bare.setPreventClose(true), throwsUnimplementedError);
      expect(() => bare.setTitle('t'), throwsUnimplementedError);
      expect(
        () => bare.setTitleBarStyle(TitleBarStyle.normal),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setTrafficLightsOffset(Offset.zero),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setTrafficLightsVisible(true),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setBackgroundColor(const Color(0x00000000)),
        throwsUnimplementedError,
      );
      expect(() => bare.setOpacity(1), throwsUnimplementedError);
      expect(() => bare.setHasShadow(true), throwsUnimplementedError);
      expect(bare.startDragging, throwsUnimplementedError);
      expect(
        () => bare.startResizing(ResizeEdge.top),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setSnapLayoutButtonRect(null),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setLinuxInteractiveRegion(id: 1, present: false),
        throwsUnimplementedError,
      );
      expect(
        () => bare.setTitleBarDragRects(dragRects: const <Rect>[]),
        throwsUnimplementedError,
      );
      expect(bare.getLaunchAtStartupState, throwsUnimplementedError);
      expect(() => bare.setLaunchAtStartup(true), throwsUnimplementedError);
      expect(bare.openLaunchAtStartupSettings, throwsUnimplementedError);
    });
  });
}

class _BarePlatform extends XueHuaNaviteWindowManagerPlatform {}

import 'dart:async';
import 'dart:io';
import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Records every platform call. Calls that return a value are written out; the
/// rest return a completed future from [noSuchMethod].
class _RecordingPlatform
    with MockPlatformInterfaceMixin
    implements XueHuaNaviteWindowManagerPlatform {
  final StreamController<WindowEvent> events =
      StreamController<WindowEvent>.broadcast();
  final List<String> calls = <String>[];
  int ensureInitializedCalls = 0;
  WindowConfig? lastConfig;
  LaunchAtStartupConfig? lastStartupConfig;
  bool? lastStartupEnabled;
  LaunchAtStartupState startupState = const LaunchAtStartupState(
    isSupported: true,
    isEnabled: false,
    status: LaunchAtStartupStatus.disabled,
    backend: LaunchAtStartupBackend.macosAppService,
    identifier: 'com.xuehua.studio',
    executablePath: '/Applications/Studio.app/Contents/MacOS/Studio',
    canOpenSettings: true,
  );
  bool openStartupSettingsResult = true;

  WindowStateSnapshot state = const WindowStateSnapshot(
    bounds: Rect.fromLTWH(10, 20, 800, 600),
    isVisible: true,
    isMaximized: false,
    isMinimized: false,
    isFullScreen: false,
    isFocused: true,
    isAlwaysOnTop: false,
    isResizable: true,
    isPreventClose: false,
  );

  void reset() {
    calls.clear();
    ensureInitializedCalls = 0;
    lastConfig = null;
    lastStartupConfig = null;
    lastStartupEnabled = null;
    startupState = const LaunchAtStartupState(
      isSupported: true,
      isEnabled: false,
      status: LaunchAtStartupStatus.disabled,
      backend: LaunchAtStartupBackend.macosAppService,
      identifier: 'com.xuehua.studio',
      executablePath: '/Applications/Studio.app/Contents/MacOS/Studio',
      canOpenSettings: true,
    );
    openStartupSettingsResult = true;
    state = const WindowStateSnapshot(
      bounds: Rect.fromLTWH(10, 20, 800, 600),
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
  Stream<WindowEvent> get windowEvents => events.stream;

  @override
  Future<void> ensureInitialized() async {
    ensureInitializedCalls++;
  }

  @override
  Future<WindowStateSnapshot> configure(WindowConfig config) async {
    lastConfig = config;
    calls.add('configure');
    return state;
  }

  @override
  Future<WindowStateSnapshot> getState() async {
    calls.add('getState');
    return state;
  }

  @override
  Future<Rect> getBounds() async {
    calls.add('getBounds');
    return state.bounds;
  }

  @override
  Future<Size> getSize() async {
    calls.add('getSize');
    return state.bounds.size;
  }

  @override
  Future<Offset> getPosition() async {
    calls.add('getPosition');
    return state.bounds.topLeft;
  }

  @override
  Future<bool> center({Size? size, bool animate = false}) async {
    calls.add('center(size: $size, animate: $animate)');
    return true;
  }

  @override
  Future<void> maximize() async {
    calls.add('maximize');
  }

  @override
  Future<void> unmaximize() async {
    calls.add('unmaximize');
  }

  @override
  Future<LaunchAtStartupState> getLaunchAtStartupState({
    LaunchAtStartupConfig? config,
  }) async {
    lastStartupConfig = config;
    calls.add('getLaunchAtStartupState');
    return startupState;
  }

  @override
  Future<LaunchAtStartupState> setLaunchAtStartup(
    bool enabled, {
    LaunchAtStartupConfig? config,
  }) async {
    lastStartupEnabled = enabled;
    lastStartupConfig = config;
    calls.add('setLaunchAtStartup($enabled)');
    startupState = LaunchAtStartupState(
      isSupported: startupState.isSupported,
      isEnabled: enabled,
      status: enabled
          ? LaunchAtStartupStatus.enabled
          : LaunchAtStartupStatus.disabled,
      backend: startupState.backend,
      identifier: config?.identifier ?? startupState.identifier,
      executablePath: startupState.executablePath,
      canOpenSettings: startupState.canOpenSettings,
    );
    return startupState;
  }

  @override
  Future<bool> openLaunchAtStartupSettings() async {
    calls.add('openLaunchAtStartupSettings');
    return openStartupSettingsResult;
  }

  @override
  dynamic noSuchMethod(Invocation invocation) {
    final String name = RegExp(r'Symbol\("(.+)"\)')
        .firstMatch(invocation.memberName.toString())!
        .group(1)!;
    final List<String> arguments = <String>[
      ...invocation.positionalArguments.map((Object? a) => '$a'),
      ...invocation.namedArguments.entries.map(
        (MapEntry<Symbol, Object?> e) =>
            '${RegExp(r'Symbol\("(.+)"\)').firstMatch(e.key.toString())!.group(1)}: ${e.value}',
      ),
    ];
    calls.add(arguments.isEmpty ? name : '$name(${arguments.join(', ')})');
    return Future<void>.value();
  }
}

class _ListenerLog with WindowListener {
  final List<String> log = <String>[];

  @override
  void onWindowEvent(WindowEvent event) => log.add('event:${event.type.name}');

  @override
  void onWindowResize(Size size) =>
      log.add('resize:${size.width}x${size.height}');

  @override
  void onWindowMove(Offset position) =>
      log.add('move:${position.dx},${position.dy}');

  @override
  void onWindowMaximize() => log.add('maximize');

  @override
  void onWindowUnmaximize() => log.add('unmaximize');

  @override
  void onWindowMinimize() => log.add('minimize');

  @override
  void onWindowRestore() => log.add('restore');

  @override
  void onWindowEnterFullScreen() => log.add('enterFullScreen');

  @override
  void onWindowLeaveFullScreen() => log.add('leaveFullScreen');

  @override
  void onWindowFocus() => log.add('focus');

  @override
  void onWindowBlur() => log.add('blur');

  @override
  void onWindowCloseRequested() => log.add('closeRequested');

  @override
  void onSnapButtonHovered(bool isHovered) => log.add('snapHover:$isHovered');

  @override
  void onSnapButtonClicked() => log.add('snapClick');
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  // The controller is a singleton that subscribes to the first platform's event
  // stream, so one platform is shared by every test in this file.
  final _RecordingPlatform platform = _RecordingPlatform();
  XueHuaNaviteWindowManagerPlatform.instance = platform;
  final XueHuaNaviteWindowManager manager = XueHuaNaviteWindowManager.instance;

  setUp(() {
    platform.reset();
    manager.configureLaunchAtStartup(null);
  });

  Future<void> deliver(WindowEvent event) async {
    platform.events.add(event);
    await Future<void>.delayed(Duration.zero);
  }

  test('NativeWindowController is the same type', () {
    expect(identical(NativeWindowController.instance, manager), isTrue);
  });

  test('ensureInitialized initializes the platform exactly once', () async {
    await manager.ensureInitialized();
    await manager.ensureInitialized();
    await manager.ensureInitialized();

    // Another test in this file may have initialized it first; the count never
    // exceeds one for the whole process.
    expect(platform.ensureInitializedCalls, lessThanOrEqualTo(1));
  });

  test('every command is forwarded with the same arguments', () async {
    await manager.show();
    await manager.show(focus: false);
    await manager.hide();
    await manager.focus();
    await manager.blur();
    await manager.close();
    await manager.destroy();
    await manager.maximize();
    await manager.unmaximize();
    await manager.minimize();
    await manager.restore();
    await manager.setFullScreen(true);
    await manager.setBounds(const Rect.fromLTWH(1, 2, 3, 4), animate: true);
    await manager.getBounds();
    await manager.setSize(const Size(5, 6), animate: true);
    await manager.getSize();
    await manager.setPosition(const Offset(7, 8), animate: true);
    await manager.getPosition();
    await manager.center(size: const Size(9, 10), animate: true);
    await manager.setMinimumSize(const Size(11, 12));
    await manager.setMaximumSize(const Size(13, 14));
    await manager.setResizable(false);
    await manager.setAlwaysOnTop(true);
    await manager.setPreventClose(true);
    await manager.setTitle('Title');
    await manager.setTitleBarStyle(
      TitleBarStyle.hidden,
      trafficLightsOffset: const Offset(15, 16),
      trafficLightsVisible: false,
    );
    await manager.setTrafficLightsOffset(const Offset(17, 18));
    await manager.setTrafficLightsVisible(true);
    await manager.setBackgroundColor(const Color(0xFF010203));
    await manager.setOpacity(0.5);
    await manager.setHasShadow(false);
    await manager.startDragging();
    await manager.startResizing(ResizeEdge.bottomLeft);
    await manager.setSnapLayoutButtonRect(const Rect.fromLTWH(19, 20, 21, 22));
    await manager.setTitleBarDragRects(
      dragRects: const <Rect>[Rect.fromLTWH(0, 0, 1, 1)],
      excludeRects: const <Rect>[Rect.fromLTWH(2, 2, 1, 1)],
    );

    expect(platform.calls, <String>[
      'show(focus: true)',
      'show(focus: false)',
      'hide',
      'focus',
      'blur',
      'close',
      'destroy',
      'maximize',
      'unmaximize',
      'minimize',
      'restore',
      'setFullScreen(true)',
      'setBounds(Rect.fromLTRB(1.0, 2.0, 4.0, 6.0), animate: true)',
      'getBounds',
      'setSize(Size(5.0, 6.0), animate: true)',
      'getSize',
      'setPosition(Offset(7.0, 8.0), animate: true)',
      'getPosition',
      'center(size: Size(9.0, 10.0), animate: true)',
      'setMinimumSize(Size(11.0, 12.0))',
      'setMaximumSize(Size(13.0, 14.0))',
      'setResizable(false)',
      'setAlwaysOnTop(true)',
      'setPreventClose(true)',
      'setTitle(Title)',
      'setTitleBarStyle(TitleBarStyle.hidden, trafficLightsOffset: Offset(15.0, 16.0), trafficLightsVisible: false)',
      'setTrafficLightsOffset(Offset(17.0, 18.0))',
      'setTrafficLightsVisible(true)',
      'setBackgroundColor(${const Color(0xFF010203)})',
      'setOpacity(0.5)',
      'setHasShadow(false)',
      'startDragging',
      'startResizing(ResizeEdge.bottomLeft)',
      'setSnapLayoutButtonRect(Rect.fromLTRB(19.0, 20.0, 40.0, 42.0))',
      'setTitleBarDragRects(dragRects: [Rect.fromLTRB(0.0, 0.0, 1.0, 1.0)], excludeRects: [Rect.fromLTRB(2.0, 2.0, 3.0, 3.0)])',
    ]);
  });

  test(
    'toggleMaximize maximizes a normal window and restores a maximized one',
    () async {
      await manager.toggleMaximize();
      expect(platform.calls, <String>['getState', 'maximize']);

      platform.reset();
      platform.state = WindowStateSnapshot(
        bounds: platform.state.bounds,
        isVisible: true,
        isMaximized: true,
        isMinimized: false,
        isFullScreen: false,
        isFocused: true,
        isAlwaysOnTop: false,
        isResizable: true,
        isPreventClose: false,
      );
      await manager.toggleMaximize();
      expect(platform.calls, <String>['getState', 'unmaximize']);
    },
  );

  test('getState caches the snapshot it returns', () async {
    final WindowStateSnapshot fresh = await manager.getState();

    expect(identical(manager.lastSnapshot, fresh), isTrue);
  });

  test(
    'configure sends showWhenReady false and never reveals when it is off',
    () async {
      final WindowStateSnapshot snapshot =
          await XueHuaNaviteWindowManager.configure(
            const WindowConfig(size: Size(400, 300), showWhenReady: false),
          );
      await Future<void>.delayed(Duration.zero);

      expect(platform.lastConfig!.showWhenReady, isFalse);
      expect(platform.lastConfig!.size, const Size(400, 300));
      expect(identical(manager.lastSnapshot, snapshot), isTrue);
      expect(platform.calls, <String>['configure']);
    },
  );

  testWidgets('configure reveals the window after the first frame', (
    WidgetTester tester,
  ) async {
    await manager.configureWindow(const WindowConfig(size: Size(400, 300)));

    // Native receives showWhenReady false so it can hold the window back...
    expect(platform.lastConfig!.showWhenReady, isFalse);
    expect(platform.calls, <String>['configure']);

    // ...and the controller reveals it once Flutter has drawn a frame.
    await tester.pump();
    await tester.pump();
    expect(platform.calls, <String>['configure', 'markReadyToShow']);
  });

  test(
    'setLinuxInteractiveRegion only reaches the platform on Linux',
    () async {
      await manager.setLinuxInteractiveRegion(
        id: 4,
        present: true,
        kind: 'move',
        rect: const Rect.fromLTWH(0, 0, 10, 10),
      );

      if (Platform.isLinux) {
        expect(platform.calls, hasLength(1));
        expect(platform.calls.single, startsWith('setLinuxInteractiveRegion'));
      } else {
        expect(platform.calls, isEmpty);
      }
    },
  );

  test('allocateLinuxInteractiveRegionId hands out increasing unique ids', () {
    final int first =
        XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId();
    final int second =
        XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId();

    expect(second, greaterThan(first));
  });

  group('launch at startup', () {
    test(
      'queries state, toggles startup, and opens OS startup settings',
      () async {
        expect(await manager.isLaunchAtStartupEnabled(), isFalse);
        expect(platform.lastStartupConfig, isNull);

        final LaunchAtStartupState enabled = await manager.setLaunchAtStartup(
          true,
        );
        expect(enabled.isEnabled, isTrue);
        expect(enabled.status, LaunchAtStartupStatus.enabled);
        expect(await manager.isLaunchAtStartupEnabled(), isTrue);

        final LaunchAtStartupState disabled = await manager.setLaunchAtStartup(
          false,
        );
        expect(disabled.isEnabled, isFalse);
        expect(disabled.status, LaunchAtStartupStatus.disabled);

        expect(await manager.openLaunchAtStartupSettings(), isTrue);
        expect(platform.calls, <String>[
          'getLaunchAtStartupState',
          'setLaunchAtStartup(true)',
          'getLaunchAtStartupState',
          'setLaunchAtStartup(false)',
          'openLaunchAtStartupSettings',
        ]);
      },
    );

    test(
      'configureLaunchAtStartup sets default config and per-call config overrides it',
      () async {
        const LaunchAtStartupConfig defaultConfig = LaunchAtStartupConfig(
          identifier: 'com.xuehua.default',
          arguments: <String>['--minimized'],
        );
        manager.configureLaunchAtStartup(defaultConfig);
        expect(manager.defaultLaunchAtStartupConfig, defaultConfig);

        await manager.getLaunchAtStartupState();
        expect(platform.lastStartupConfig, defaultConfig);

        const LaunchAtStartupConfig overrideConfig = LaunchAtStartupConfig(
          identifier: 'com.xuehua.override',
          macOSMode: MacOSLaunchAtStartupMode.launchAgent,
        );
        final LaunchAtStartupState updated = await manager.setLaunchAtStartup(
          true,
          config: overrideConfig,
        );
        expect(platform.lastStartupConfig, overrideConfig);
        expect(updated.identifier, 'com.xuehua.override');
      },
    );
  });

  group('events', () {
    late _ListenerLog listener;

    setUp(() async {
      listener = _ListenerLog();
      manager.addListener(listener);
      // Seed the cache so events have a snapshot to update.
      await manager.getState();
    });

    tearDown(() => manager.removeListener(listener));

    test('each event type reaches its typed callback', () async {
      const Rect bounds = Rect.fromLTWH(30, 40, 500, 400);
      final Map<WindowEventType, String> expected = <WindowEventType, String>{
        WindowEventType.resized: 'resize:500.0x400.0',
        WindowEventType.moved: 'move:30.0,40.0',
        WindowEventType.maximized: 'maximize',
        WindowEventType.unmaximized: 'unmaximize',
        WindowEventType.minimized: 'minimize',
        WindowEventType.restored: 'restore',
        WindowEventType.enteredFullScreen: 'enterFullScreen',
        WindowEventType.exitedFullScreen: 'leaveFullScreen',
        WindowEventType.focused: 'focus',
        WindowEventType.blurred: 'blur',
        WindowEventType.closeRequested: 'closeRequested',
        WindowEventType.snapButtonHovered: 'snapHover:true',
        WindowEventType.snapButtonLeft: 'snapHover:false',
        WindowEventType.snapButtonClicked: 'snapClick',
      };
      expect(expected.keys.toSet(), WindowEventType.values.toSet());

      for (final MapEntry<WindowEventType, String> entry in expected.entries) {
        listener.log.clear();
        await deliver(
          WindowEvent(type: entry.key, bounds: bounds, timestamp: 1),
        );
        expect(listener.log, <String>[
          'event:${entry.key.name}',
          entry.value,
        ], reason: '${entry.key}');
      }
    });

    test('resized and moved without bounds only reach onWindowEvent', () async {
      await deliver(
        const WindowEvent(type: WindowEventType.resized, timestamp: 1),
      );
      await deliver(
        const WindowEvent(type: WindowEventType.moved, timestamp: 2),
      );

      expect(listener.log, <String>['event:resized', 'event:moved']);
    });

    test('a removed listener is not called', () async {
      manager.removeListener(listener);

      await deliver(
        const WindowEvent(type: WindowEventType.focused, timestamp: 1),
      );

      expect(listener.log, isEmpty);
    });

    test('a listener removed during dispatch is skipped', () async {
      final _ListenerLog second = _ListenerLog();
      final _RemovesOther first = _RemovesOther(second);
      manager.addListener(first);
      manager.addListener(second);
      addTearDown(() {
        manager.removeListener(first);
        manager.removeListener(second);
      });

      await deliver(
        const WindowEvent(type: WindowEventType.focused, timestamp: 1),
      );

      expect(first.calls, 1);
      expect(second.log, isEmpty);
    });

    test('lastSnapshot follows the events that carry state', () async {
      await deliver(
        const WindowEvent(
          type: WindowEventType.maximized,
          bounds: Rect.fromLTWH(0, 0, 1920, 1080),
          timestamp: 1,
        ),
      );
      expect(manager.lastSnapshot!.isMaximized, isTrue);
      expect(
        manager.lastSnapshot!.bounds,
        const Rect.fromLTWH(0, 0, 1920, 1080),
      );

      await deliver(
        const WindowEvent(type: WindowEventType.unmaximized, timestamp: 2),
      );
      expect(manager.lastSnapshot!.isMaximized, isFalse);

      await deliver(
        const WindowEvent(type: WindowEventType.minimized, timestamp: 3),
      );
      expect(manager.lastSnapshot!.isMinimized, isTrue);
      await deliver(
        const WindowEvent(type: WindowEventType.restored, timestamp: 4),
      );
      expect(manager.lastSnapshot!.isMinimized, isFalse);

      await deliver(
        const WindowEvent(
          type: WindowEventType.enteredFullScreen,
          timestamp: 5,
        ),
      );
      expect(manager.lastSnapshot!.isFullScreen, isTrue);
      await deliver(
        const WindowEvent(type: WindowEventType.exitedFullScreen, timestamp: 6),
      );
      expect(manager.lastSnapshot!.isFullScreen, isFalse);

      await deliver(
        const WindowEvent(type: WindowEventType.blurred, timestamp: 7),
      );
      expect(manager.lastSnapshot!.isFocused, isFalse);
      await deliver(
        const WindowEvent(type: WindowEventType.focused, timestamp: 8),
      );
      expect(manager.lastSnapshot!.isFocused, isTrue);
    });

    test('event payload flags win over the event type', () async {
      await deliver(
        const WindowEvent(
          type: WindowEventType.resized,
          isMaximized: true,
          isFullScreen: true,
          timestamp: 1,
        ),
      );

      expect(manager.lastSnapshot!.isMaximized, isTrue);
      expect(manager.lastSnapshot!.isFullScreen, isTrue);
    });

    test('events expose the platform stream', () async {
      final Future<WindowEvent> next = manager.events.first;

      platform.events.add(
        const WindowEvent(type: WindowEventType.closeRequested, timestamp: 9),
      );

      expect((await next).type, WindowEventType.closeRequested);
    });
  });
}

class _RemovesOther with WindowListener {
  _RemovesOther(this.other);

  final WindowListener other;
  int calls = 0;

  @override
  void onWindowEvent(WindowEvent event) {
    calls++;
    XueHuaNaviteWindowManager.instance.removeListener(other);
  }
}

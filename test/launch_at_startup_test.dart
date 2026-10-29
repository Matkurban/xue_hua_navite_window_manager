import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  group('LaunchAtStartup enums', () {
    test('LaunchAtStartupStatus wire round-trip and fallback', () {
      for (final LaunchAtStartupStatus status in LaunchAtStartupStatus.values) {
        expect(LaunchAtStartupStatus.fromWire(status.wireValue), status);
      }
      expect(
        LaunchAtStartupStatus.fromWire(null),
        LaunchAtStartupStatus.disabled,
      );
      expect(
        LaunchAtStartupStatus.fromWire('unknown_status'),
        LaunchAtStartupStatus.disabled,
      );
    });

    test('MacOSLaunchAtStartupMode wire round-trip and fallback', () {
      for (final MacOSLaunchAtStartupMode mode
          in MacOSLaunchAtStartupMode.values) {
        expect(MacOSLaunchAtStartupMode.fromWire(mode.wireValue), mode);
      }
      expect(
        MacOSLaunchAtStartupMode.fromWire(null),
        MacOSLaunchAtStartupMode.automatic,
      );
      expect(
        MacOSLaunchAtStartupMode.fromWire('other'),
        MacOSLaunchAtStartupMode.automatic,
      );
    });

    test('LaunchAtStartupBackend wire round-trip and fallback', () {
      for (final LaunchAtStartupBackend backend
          in LaunchAtStartupBackend.values) {
        expect(LaunchAtStartupBackend.fromWire(backend.wireValue), backend);
      }
      expect(
        LaunchAtStartupBackend.fromWire(null),
        LaunchAtStartupBackend.none,
      );
      expect(
        LaunchAtStartupBackend.fromWire('futureBackend'),
        LaunchAtStartupBackend.none,
      );
    });
  });

  group('LaunchAtStartupConfig', () {
    test('default config emits empty wire map', () {
      const LaunchAtStartupConfig config = LaunchAtStartupConfig();
      expect(config.arguments, isEmpty);
      expect(config.macOSMode, MacOSLaunchAtStartupMode.automatic);
      expect(config.toMap(), isEmpty);
    });

    test(
      'populated config serializes all fields and ignores blank strings',
      () {
        const LaunchAtStartupConfig full = LaunchAtStartupConfig(
          appName: 'XueHua Studio',
          identifier: 'com.xuehua.studio',
          executablePath: '/Applications/XueHua.app/Contents/MacOS/XueHua',
          arguments: <String>['--minimized', '--profile=work'],
          msixTaskId: 'XueHuaStartupTask',
          macOSMode: MacOSLaunchAtStartupMode.launchAgent,
        );
        expect(full.toMap(), <String, Object?>{
          'appName': 'XueHua Studio',
          'identifier': 'com.xuehua.studio',
          'executablePath': '/Applications/XueHua.app/Contents/MacOS/XueHua',
          'arguments': <String>['--minimized', '--profile=work'],
          'msixTaskId': 'XueHuaStartupTask',
          'macOSMode': 'launchAgent',
        });

        const LaunchAtStartupConfig blankStrings = LaunchAtStartupConfig(
          appName: '   ',
          identifier: '',
          executablePath: '  ',
          msixTaskId: '\t',
        );
        expect(blankStrings.toMap(), isEmpty);
      },
    );

    test('copyWith updates and preserves fields; value equality holds', () {
      const LaunchAtStartupConfig base = LaunchAtStartupConfig(
        appName: 'Studio',
        identifier: 'com.xuehua.studio',
        executablePath: '/usr/bin/studio',
        arguments: <String>['--silent'],
        msixTaskId: 'TaskA',
        macOSMode: MacOSLaunchAtStartupMode.appService,
      );
      final LaunchAtStartupConfig same = base.copyWith();
      expect(same, equals(base));
      expect(same.hashCode, equals(base.hashCode));
      expect(same.toString(), contains('com.xuehua.studio'));

      final LaunchAtStartupConfig updated = base.copyWith(
        appName: 'Studio Pro',
        arguments: <String>['--minimized'],
        macOSMode: MacOSLaunchAtStartupMode.launchAgent,
      );
      expect(updated.appName, 'Studio Pro');
      expect(updated.identifier, 'com.xuehua.studio');
      expect(updated.executablePath, '/usr/bin/studio');
      expect(updated.msixTaskId, 'TaskA');
      expect(updated.arguments, <String>['--minimized']);
      expect(updated.macOSMode, MacOSLaunchAtStartupMode.launchAgent);
      expect(updated, isNot(equals(base)));
    });
  });

  group('LaunchAtStartupState', () {
    test('fromMap handles null and empty maps safely', () {
      final LaunchAtStartupState nullState = LaunchAtStartupState.fromMap(null);
      expect(nullState.isSupported, isFalse);
      expect(nullState.isEnabled, isFalse);
      expect(nullState.status, LaunchAtStartupStatus.unsupported);
      expect(nullState.backend, LaunchAtStartupBackend.none);
      expect(nullState.identifier, isNull);
      expect(nullState.executablePath, isNull);
      expect(nullState.canOpenSettings, isFalse);

      final LaunchAtStartupState emptyExe = LaunchAtStartupState.fromMap(
        <Object?, Object?>{
          'isSupported': true,
          'isEnabled': false,
          'status': 'disabledByUser',
          'backend': 'windowsRegistry',
          'identifier': 'XueHuaApp',
          'executablePath': '',
          'canOpenSettings': true,
        },
      );
      expect(emptyExe.isSupported, isTrue);
      expect(emptyExe.isEnabled, isFalse);
      expect(emptyExe.status, LaunchAtStartupStatus.disabledByUser);
      expect(emptyExe.backend, LaunchAtStartupBackend.windowsRegistry);
      expect(emptyExe.identifier, 'XueHuaApp');
      expect(emptyExe.executablePath, isNull);
      expect(emptyExe.canOpenSettings, isTrue);
    });

    test('fromMap/toMap round-trip and value equality', () {
      const LaunchAtStartupState state = LaunchAtStartupState(
        isSupported: true,
        isEnabled: true,
        status: LaunchAtStartupStatus.enabled,
        backend: LaunchAtStartupBackend.linuxXdgAutostart,
        identifier: 'com.xuehua.studio',
        executablePath: '/usr/local/bin/xuehua_studio',
        canOpenSettings: false,
      );
      final LaunchAtStartupState decoded = LaunchAtStartupState.fromMap(
        state.toMap(),
      );
      expect(decoded, equals(state));
      expect(decoded.hashCode, equals(state.hashCode));
      expect(decoded.toString(), contains('linuxXdgAutostart'));
    });
  });
}

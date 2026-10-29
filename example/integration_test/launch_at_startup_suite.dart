import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  final XueHuaNaviteWindowManager manager = XueHuaNaviteWindowManager.instance;

  testWidgets('getLaunchAtStartupState reports native backend and metadata', (
    WidgetTester tester,
  ) async {
    final LaunchAtStartupState state = await manager.getLaunchAtStartupState();
    final bool enabledHelper = await manager.isLaunchAtStartupEnabled();

    expect(state.isEnabled, enabledHelper);
    expect(state.isSupported, isTrue);
    expect(state.identifier, isNotNull);
    expect(state.identifier, isNotEmpty);
    expect(state.executablePath, isNotNull);
    expect(state.executablePath, isNotEmpty);

    if (Platform.isMacOS) {
      expect(
        state.backend,
        anyOf(
          LaunchAtStartupBackend.macosAppService,
          LaunchAtStartupBackend.macosLaunchAgent,
        ),
      );
      expect(state.canOpenSettings, isTrue);
    } else if (Platform.isWindows) {
      expect(
        state.backend,
        anyOf(
          LaunchAtStartupBackend.windowsRegistry,
          LaunchAtStartupBackend.windowsStartupTask,
        ),
      );
      expect(state.canOpenSettings, isTrue);
    } else if (Platform.isLinux) {
      expect(state.backend, LaunchAtStartupBackend.linuxXdgAutostart);
      expect(state.canOpenSettings, isFalse);
      expect(await manager.openLaunchAtStartupSettings(), isFalse);
    }
  });

  testWidgets(
    'setLaunchAtStartup exercises isolated registration without polluting host settings',
    (WidgetTester tester) async {
      final String isolatedId = 'com.xuehua.integration_test_$pid';

      if (Platform.isMacOS) {
        final bool isSandboxed =
            (Platform.environment['APP_SANDBOX_CONTAINER_ID'] ?? '').isNotEmpty;
        final LaunchAtStartupConfig launchAgentConfig = LaunchAtStartupConfig(
          identifier: isolatedId,
          arguments: const <String>['--minimized'],
          macOSMode: MacOSLaunchAtStartupMode.launchAgent,
        );

        if (isSandboxed) {
          final LaunchAtStartupState sandboxedState = await manager
              .getLaunchAtStartupState(config: launchAgentConfig);
          expect(sandboxedState.isSupported, isFalse);
          expect(sandboxedState.status, LaunchAtStartupStatus.unsupported);
          expect(
            sandboxedState.backend,
            LaunchAtStartupBackend.macosLaunchAgent,
          );

          await expectLater(
            manager.setLaunchAtStartup(true, config: launchAgentConfig),
            throwsA(
              isA<PlatformException>().having(
                (PlatformException e) => e.code,
                'code',
                'SANDBOX_RESTRICTION',
              ),
            ),
          );
        } else {
          addTearDown(() async {
            try {
              await manager.setLaunchAtStartup(
                false,
                config: launchAgentConfig,
              );
            } catch (_) {}
          });

          expect(
            await manager.isLaunchAtStartupEnabled(config: launchAgentConfig),
            isFalse,
          );
          final LaunchAtStartupState enabled = await manager.setLaunchAtStartup(
            true,
            config: launchAgentConfig,
          );
          expect(enabled.isEnabled, isTrue);
          expect(enabled.status, LaunchAtStartupStatus.enabled);
          expect(
            await manager.isLaunchAtStartupEnabled(config: launchAgentConfig),
            isTrue,
          );

          final LaunchAtStartupState disabled = await manager
              .setLaunchAtStartup(false, config: launchAgentConfig);
          expect(disabled.isEnabled, isFalse);
          expect(disabled.status, LaunchAtStartupStatus.disabled);
        }
        return;
      }

      final LaunchAtStartupState hostState = await manager
          .getLaunchAtStartupState();
      if (Platform.isWindows &&
          hostState.backend == LaunchAtStartupBackend.windowsStartupTask) {
        // MSIX StartupTask uses manifest-declared task IDs; skip registry round-trip.
        return;
      }

      final LaunchAtStartupConfig isolatedConfig = LaunchAtStartupConfig(
        appName: 'XueHua Integration Test',
        identifier: isolatedId,
        arguments: const <String>['--minimized', '--profile=integration test'],
      );
      addTearDown(() async {
        try {
          await manager.setLaunchAtStartup(false, config: isolatedConfig);
        } catch (_) {}
      });

      final LaunchAtStartupState before = await manager.getLaunchAtStartupState(
        config: isolatedConfig,
      );
      expect(before.isEnabled, isFalse);
      expect(before.status, LaunchAtStartupStatus.disabled);

      final LaunchAtStartupState enabled = await manager.setLaunchAtStartup(
        true,
        config: isolatedConfig,
      );
      expect(enabled.isEnabled, isTrue);
      expect(enabled.status, LaunchAtStartupStatus.enabled);
      expect(
        await manager.isLaunchAtStartupEnabled(config: isolatedConfig),
        isTrue,
      );

      final LaunchAtStartupState disabled = await manager.setLaunchAtStartup(
        false,
        config: isolatedConfig,
      );
      expect(disabled.isEnabled, isFalse);
      expect(disabled.status, LaunchAtStartupStatus.disabled);
      expect(
        await manager.isLaunchAtStartupEnabled(config: isolatedConfig),
        isFalse,
      );
    },
  );
}

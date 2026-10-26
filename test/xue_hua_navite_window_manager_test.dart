import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager_platform_interface.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockXueHuaNaviteWindowManagerPlatform
    with MockPlatformInterfaceMixin
    implements XueHuaNaviteWindowManagerPlatform {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final XueHuaNaviteWindowManagerPlatform initialPlatform = XueHuaNaviteWindowManagerPlatform.instance;

  test('$MethodChannelXueHuaNaviteWindowManager is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelXueHuaNaviteWindowManager>());
  });

  test('getPlatformVersion', () async {
    XueHuaNaviteWindowManager xueHuaNaviteWindowManagerPlugin = XueHuaNaviteWindowManager();
    MockXueHuaNaviteWindowManagerPlatform fakePlatform = MockXueHuaNaviteWindowManagerPlatform();
    XueHuaNaviteWindowManagerPlatform.instance = fakePlatform;

    expect(await xueHuaNaviteWindowManagerPlugin.getPlatformVersion(), '42');
  });
}

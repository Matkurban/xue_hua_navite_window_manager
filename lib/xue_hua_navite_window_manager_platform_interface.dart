import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'xue_hua_navite_window_manager_method_channel.dart';

abstract class XueHuaNaviteWindowManagerPlatform extends PlatformInterface {
  /// Constructs a XueHuaNaviteWindowManagerPlatform.
  XueHuaNaviteWindowManagerPlatform() : super(token: _token);

  static final Object _token = Object();

  static XueHuaNaviteWindowManagerPlatform _instance = MethodChannelXueHuaNaviteWindowManager();

  /// The default instance of [XueHuaNaviteWindowManagerPlatform] to use.
  ///
  /// Defaults to [MethodChannelXueHuaNaviteWindowManager].
  static XueHuaNaviteWindowManagerPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [XueHuaNaviteWindowManagerPlatform] when
  /// they register themselves.
  static set instance(XueHuaNaviteWindowManagerPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}

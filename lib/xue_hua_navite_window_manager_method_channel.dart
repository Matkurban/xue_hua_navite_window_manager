import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'xue_hua_navite_window_manager_platform_interface.dart';

/// An implementation of [XueHuaNaviteWindowManagerPlatform] that uses method channels.
class MethodChannelXueHuaNaviteWindowManager extends XueHuaNaviteWindowManagerPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('xue_hua_navite_window_manager');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>(
      'getPlatformVersion',
    );
    return version;
  }
}


import 'xue_hua_navite_window_manager_platform_interface.dart';

class XueHuaNaviteWindowManager {
  Future<String?> getPlatformVersion() {
    return XueHuaNaviteWindowManagerPlatform.instance.getPlatformVersion();
  }
}

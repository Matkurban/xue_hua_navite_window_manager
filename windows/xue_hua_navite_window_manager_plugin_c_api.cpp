#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "xue_hua_navite_window_manager_plugin.h"

void XueHuaNaviteWindowManagerPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  xue_hua_navite_window_manager::XueHuaNaviteWindowManagerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

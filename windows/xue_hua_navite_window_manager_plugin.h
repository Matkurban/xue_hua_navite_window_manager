#ifndef FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_
#define FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace xue_hua_navite_window_manager {

class XueHuaNaviteWindowManagerPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  XueHuaNaviteWindowManagerPlugin();

  virtual ~XueHuaNaviteWindowManagerPlugin();

  // Disallow copy and assign.
  XueHuaNaviteWindowManagerPlugin(const XueHuaNaviteWindowManagerPlugin&) = delete;
  XueHuaNaviteWindowManagerPlugin& operator=(const XueHuaNaviteWindowManagerPlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_H_

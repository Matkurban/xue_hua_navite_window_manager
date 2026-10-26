#include <flutter/method_call.h>
#include <flutter/method_result_functions.h>
#include <flutter/standard_method_codec.h>
#include <gtest/gtest.h>
#include <windows.h>

#include <memory>
#include <string>

#include "xue_hua_navite_window_manager_plugin.h"

namespace xue_hua_navite_window_manager {
namespace test {

namespace {

using flutter::EncodableValue;
using flutter::MethodCall;
using flutter::MethodResultFunctions;

}  // namespace

TEST(XueHuaNaviteWindowManagerPlugin, EnsureInitializedSucceeds) {
  XueHuaNaviteWindowManagerPlugin plugin;
  bool succeeded = false;
  plugin.HandleMethodCall(
      MethodCall("ensureInitialized", std::make_unique<EncodableValue>()),
      std::make_unique<MethodResultFunctions<>>(
          [&succeeded](const EncodableValue* result) { succeeded = true; },
          nullptr, nullptr));

  EXPECT_TRUE(succeeded);
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

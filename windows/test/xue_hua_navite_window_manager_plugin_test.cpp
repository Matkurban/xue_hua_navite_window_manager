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

TEST(SnapButtonHitTest, ActivatesOnlyWhenArmedReleaseStaysInside) {
  EXPECT_FALSE(ShouldActivateSnapButton(false, true, true));
  EXPECT_FALSE(ShouldActivateSnapButton(true, false, true));
  EXPECT_FALSE(ShouldActivateSnapButton(true, true, false));
  EXPECT_TRUE(ShouldActivateSnapButton(true, true, true));
}

TEST(SnapButtonHitTest, ReportsHtMaxButtonOnlyForCustomFrames) {
  const LogicalRect rect{900.0, 0.0, 46.0, 40.0};

  EXPECT_TRUE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 920.0, 10.0));
  EXPECT_TRUE(
      ShouldReportSnapButtonHit(true, false, "immersive", rect, 945.0, 39.0));
  // Contains() is half-open on the right and bottom edges.
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 946.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", rect, 920.0, 40.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "normal", rect, 920.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, true, "hidden", rect, 920.0, 10.0));
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(false, false, "hidden", rect, 920.0, 10.0));

  const LogicalRect empty{0.0, 0.0, 0.0, 40.0};
  EXPECT_FALSE(
      ShouldReportSnapButtonHit(true, false, "hidden", empty, 0.0, 10.0));
}

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

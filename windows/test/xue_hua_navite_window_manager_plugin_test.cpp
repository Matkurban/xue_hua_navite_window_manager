#include <flutter/method_call.h>
#include <flutter/method_result_functions.h>
#include <flutter/standard_method_codec.h>
#include <gtest/gtest.h>
#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <vector>

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

TEST(SizeClamp, SetSizeHonorsStoredMinimumAndMaximum) {
  // Repro: setMinimumSize(850, 560) then setSize(500, 400) must not stick.
  double width = 500.0;
  double height = 400.0;
  ClampDipSize(850.0, 560.0, 0.0, 0.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 850.0);
  EXPECT_DOUBLE_EQ(height, 560.0);

  width = 1400.0;
  height = 900.0;
  ClampDipSize(0.0, 0.0, 1050.0, 700.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 1050.0);
  EXPECT_DOUBLE_EQ(height, 700.0);

  width = 900.0;
  height = 640.0;
  ClampDipSize(850.0, 560.0, 1050.0, 700.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 900.0);
  EXPECT_DOUBLE_EQ(height, 640.0);

  // Unset limits (<= 0) do not move the requested size.
  width = 100.0;
  height = 80.0;
  ClampDipSize(0.0, 0.0, 0.0, 0.0, &width, &height);
  EXPECT_DOUBLE_EQ(width, 100.0);
  EXPECT_DOUBLE_EQ(height, 80.0);

  // Minimum wins when the stored maximum is below the minimum.
  EXPECT_DOUBLE_EQ(ClampDipAxis(50.0, 800.0, 100.0), 800.0);
  EXPECT_DOUBLE_EQ(ClampDipAxis(50.0, 600.0, 80.0), 600.0);
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

TEST(LaunchAtStartupWindows, SanitizeStartupIdCleansUnsafeChars) {
  EXPECT_EQ(SanitizeWindowsStartupId(L"  My\\App/Name:1*2?  "),
            L"My_App_Name_1_2_");
  EXPECT_EQ(SanitizeWindowsStartupId(L"   "), L"FlutterApp");
}

TEST(LaunchAtStartupWindows, CommandLineEscapingRoundTripsCommandLineToArgvW) {
  const std::wstring exe =
      L"C:\\Program Files\\XueHua Studio\\xue_hua_studio.exe";
  const std::vector<std::wstring> input_args = {
      L"--minimized",
      L"",
      L"hello world",
      L"C:\\Users\\Admin\\TrailingSlash\\",
      L"--json={\"key\":\"val\\\"ue\"}",
      L"back\\\\slash\"mix",
  };
  const std::wstring cmd = BuildWindowsStartupCommand(exe, input_args);

  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &argc);
  ASSERT_NE(argv, nullptr);
  ASSERT_EQ(argc, static_cast<int>(1 + input_args.size()));
  EXPECT_EQ(std::wstring(argv[0]), exe);
  for (size_t i = 0; i < input_args.size(); ++i) {
    EXPECT_EQ(std::wstring(argv[i + 1]), input_args[i]);
  }
  LocalFree(argv);
}

TEST(LaunchAtStartupWindows, StartupApprovedAndMsixStateEvaluation) {
  EXPECT_EQ(EvaluateWindowsStartupApprovedBytes(nullptr, 0),
            WindowsStartupApproval::kApproved);

  const BYTE approved_bytes[12] = {0x02, 0};
  const BYTE user_disabled_bytes[12] = {0x03, 0};
  const BYTE policy_disabled_bytes[12] = {0x09, 0};

  EXPECT_EQ(EvaluateWindowsStartupApprovedBytes(approved_bytes, 12),
            WindowsStartupApproval::kApproved);
  EXPECT_EQ(EvaluateWindowsStartupApprovedBytes(user_disabled_bytes, 12),
            WindowsStartupApproval::kDisabledByUser);
  EXPECT_EQ(EvaluateWindowsStartupApprovedBytes(policy_disabled_bytes, 12),
            WindowsStartupApproval::kDisabledByPolicy);

  const WindowsStartupWireState reg_enabled =
      EvaluateWindowsRegistryStartupState(true,
                                          WindowsStartupApproval::kApproved);
  EXPECT_TRUE(reg_enabled.is_enabled);
  EXPECT_STREQ(reg_enabled.status, "enabled");

  const WindowsStartupWireState reg_user_off =
      EvaluateWindowsRegistryStartupState(
          true, WindowsStartupApproval::kDisabledByUser);
  EXPECT_FALSE(reg_user_off.is_enabled);
  EXPECT_STREQ(reg_user_off.status, "disabledByUser");

  const WindowsStartupWireState reg_policy_off =
      EvaluateWindowsRegistryStartupState(
          false, WindowsStartupApproval::kDisabledByPolicy);
  EXPECT_FALSE(reg_policy_off.is_enabled);
  EXPECT_STREQ(reg_policy_off.status, "disabledByPolicy");

  EXPECT_STREQ(EvaluateWindowsMsixStartupState(false, 2).status, "disabled");
  EXPECT_STREQ(EvaluateWindowsMsixStartupState(true, 0).status, "disabled");
  EXPECT_STREQ(EvaluateWindowsMsixStartupState(true, 1).status,
               "disabledByUser");
  EXPECT_TRUE(EvaluateWindowsMsixStartupState(true, 2).is_enabled);
  EXPECT_STREQ(EvaluateWindowsMsixStartupState(true, 3).status,
               "disabledByPolicy");
  EXPECT_TRUE(EvaluateWindowsMsixStartupState(true, 4).is_enabled);
}

TEST(LaunchAtStartupWindows, GetStateWorksWithoutAttachedHwnd) {
  XueHuaNaviteWindowManagerPlugin plugin;
  flutter::EncodableMap args;
  args[EncodableValue("identifier")] =
      EncodableValue("XueHuaUnitTest_NonExistent_9f8e7d");

  bool succeeded = false;
  flutter::EncodableMap returned_map;
  plugin.HandleMethodCall(
      MethodCall("getLaunchAtStartupState",
                 std::make_unique<EncodableValue>(args)),
      std::make_unique<MethodResultFunctions<>>(
          [&succeeded, &returned_map](const EncodableValue* result) {
            succeeded = true;
            if (result != nullptr) {
              if (const auto* m = std::get_if<flutter::EncodableMap>(result)) {
                returned_map = *m;
              }
            }
          },
          nullptr, nullptr));

  ASSERT_TRUE(succeeded);
  EXPECT_EQ(std::get<bool>(returned_map.at(EncodableValue("isSupported"))),
            true);
  EXPECT_EQ(std::get<bool>(returned_map.at(EncodableValue("isEnabled"))),
            false);
  EXPECT_EQ(std::get<std::string>(returned_map.at(EncodableValue("status"))),
            "disabled");
  EXPECT_EQ(
      std::get<bool>(returned_map.at(EncodableValue("canOpenSettings"))),
      true);
}

}  // namespace test
}  // namespace xue_hua_navite_window_manager

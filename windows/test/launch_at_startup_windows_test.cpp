#include <flutter/method_call.h>
#include <flutter/method_result_functions.h>
#include <flutter/standard_method_codec.h>
#include <gtest/gtest.h>
#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <vector>

#include "launch_at_startup_logic.h"
#include "xue_hua_navite_window_manager_plugin.h"

namespace xue_hua_navite_window_manager {
namespace test {

namespace {

using flutter::EncodableValue;
using flutter::MethodCall;
using flutter::MethodResultFunctions;

}  // namespace

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

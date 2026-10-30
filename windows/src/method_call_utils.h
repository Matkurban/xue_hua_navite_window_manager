#ifndef FLUTTER_PLUGIN_XUE_HUA_METHOD_CALL_UTILS_H_
#define FLUTTER_PLUGIN_XUE_HUA_METHOD_CALL_UTILS_H_

#include <windows.h>

#include <flutter/encodable_value.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xue_hua_navite_window_manager {
namespace internal {

constexpr UINT_PTR kWindowSubclassId = 0x5848574D;  // 'XHWM'

// Posted after the snap-button release so the Dart listener does not run
// nested inside the mouse-up / SetCapture handler.
constexpr UINT kSnapButtonInvokeMessage = WM_APP + 0x58;

// Posted to flush queued EventChannel notifications at top-level on the Win32
// message pump, preventing re-entrant Dart widget/semantics mutations inside
// synchronous WndProc / SetWindowPos / UIAutomation hit-testing.
constexpr UINT kFlushQueuedEventsMessage = WM_APP + 0x59;

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);
std::wstring TrimWide(const std::wstring& input);

std::optional<double> GetDoubleArg(const flutter::EncodableMap& args,
                                   const char* key);
std::optional<bool> GetBoolArg(const flutter::EncodableMap& args,
                               const char* key);
std::optional<std::string> GetStringArg(const flutter::EncodableMap& args,
                                        const char* key);
std::optional<uint32_t> GetUInt32Arg(const flutter::EncodableMap& args,
                                     const char* key);
std::vector<std::wstring> GetWideStringListArg(
    const flutter::EncodableMap& args,
    const char* key);

int64_t CurrentEpochMillis();

}  // namespace internal
}  // namespace xue_hua_navite_window_manager

#endif  // FLUTTER_PLUGIN_XUE_HUA_METHOD_CALL_UTILS_H_

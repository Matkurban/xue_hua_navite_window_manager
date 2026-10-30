#include "method_call_utils.h"

#include <chrono>

namespace xue_hua_navite_window_manager {
namespace internal {

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return std::wstring();
  }
  int target_len = MultiByteToWideChar(
      CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (target_len <= 0) {
    return std::wstring();
  }
  std::wstring wide(static_cast<size_t>(target_len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      wide.data(), target_len);
  return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
  if (wide.empty()) {
    return std::string();
  }
  int target_len = WideCharToMultiByte(
      CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
      nullptr, nullptr);
  if (target_len <= 0) {
    return std::string();
  }
  std::string utf8(static_cast<size_t>(target_len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      utf8.data(), target_len, nullptr, nullptr);
  return utf8;
}

std::wstring TrimWide(const std::wstring& input) {
  size_t start = 0;
  while (start < input.size() &&
         (input[start] == L' ' || input[start] == L'\t' ||
          input[start] == L'\r' || input[start] == L'\n')) {
    ++start;
  }
  size_t end = input.size();
  while (end > start &&
         (input[end - 1] == L' ' || input[end - 1] == L'\t' ||
          input[end - 1] == L'\r' || input[end - 1] == L'\n')) {
    --end;
  }
  return input.substr(start, end - start);
}

std::optional<double> GetDoubleArg(const flutter::EncodableMap& args,
                                   const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* d = std::get_if<double>(&it->second)) {
    return *d;
  }
  if (const auto* i32 = std::get_if<int32_t>(&it->second)) {
    return static_cast<double>(*i32);
  }
  if (const auto* i64 = std::get_if<int64_t>(&it->second)) {
    return static_cast<double>(*i64);
  }
  return std::nullopt;
}

std::optional<bool> GetBoolArg(const flutter::EncodableMap& args,
                               const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* b = std::get_if<bool>(&it->second)) {
    return *b;
  }
  return std::nullopt;
}

std::optional<std::string> GetStringArg(const flutter::EncodableMap& args,
                                        const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* s = std::get_if<std::string>(&it->second)) {
    return *s;
  }
  return std::nullopt;
}

std::optional<uint32_t> GetUInt32Arg(const flutter::EncodableMap& args,
                                     const char* key) {
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return std::nullopt;
  }
  if (const auto* i64 = std::get_if<int64_t>(&it->second)) {
    return static_cast<uint32_t>(*i64);
  }
  if (const auto* i32 = std::get_if<int32_t>(&it->second)) {
    return static_cast<uint32_t>(*i32);
  }
  return std::nullopt;
}

std::vector<std::wstring> GetWideStringListArg(
    const flutter::EncodableMap& args,
    const char* key) {
  std::vector<std::wstring> out;
  auto it = args.find(flutter::EncodableValue(key));
  if (it == args.end() || it->second.IsNull()) {
    return out;
  }
  const auto* list = std::get_if<flutter::EncodableList>(&it->second);
  if (list == nullptr) {
    return out;
  }
  out.reserve(list->size());
  for (const auto& item : *list) {
    if (const auto* str = std::get_if<std::string>(&item)) {
      out.push_back(Utf8ToWide(*str));
    }
  }
  return out;
}

int64_t CurrentEpochMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace internal
}  // namespace xue_hua_navite_window_manager

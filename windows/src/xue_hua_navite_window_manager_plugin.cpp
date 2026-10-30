#include "xue_hua_navite_window_manager_plugin.h"

#include <utility>

#include "method_call_utils.h"

namespace xue_hua_navite_window_manager {

// static
void XueHuaNaviteWindowManagerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto method_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "xue_hua_navite_window_manager/methods",
          &flutter::StandardMethodCodec::GetInstance());

  auto event_channel =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          registrar->messenger(), "xue_hua_navite_window_manager/events",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<XueHuaNaviteWindowManagerPlugin>(registrar);
  auto* plugin_ptr = plugin.get();

  method_channel->SetMethodCallHandler(
      [plugin_ptr](const auto& call, auto result) {
        plugin_ptr->HandleMethodCall(call, std::move(result));
      });

  auto stream_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [plugin_ptr](
          const flutter::EncodableValue* arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        plugin_ptr->event_sink_ = std::move(events);
        return nullptr;
      },
      [plugin_ptr](const flutter::EncodableValue* arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        plugin_ptr->event_sink_.reset();
        return nullptr;
      });

  event_channel->SetStreamHandler(std::move(stream_handler));

  registrar->AddPlugin(std::move(plugin));
}

XueHuaNaviteWindowManagerPlugin::XueHuaNaviteWindowManagerPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar) {
  bg_brush_ = CreateSolidBrush(bg_color_ref_);
}

XueHuaNaviteWindowManagerPlugin::~XueHuaNaviteWindowManagerPlugin() {
  DetachSubclass();
  if (bg_brush_ != nullptr) {
    DeleteObject(bg_brush_);
    bg_brush_ = nullptr;
  }
}

HWND XueHuaNaviteWindowManagerPlugin::ResolveMainWindow() {
  if (subclassed_hwnd_ != nullptr && IsWindow(subclassed_hwnd_)) {
    return subclassed_hwnd_;
  }
  if (registrar_ != nullptr && registrar_->GetView() != nullptr) {
    HWND view_hwnd = registrar_->GetView()->GetNativeWindow();
    if (view_hwnd != nullptr) {
      HWND root_hwnd = GetAncestor(view_hwnd, GA_ROOT);
      if (root_hwnd != nullptr) {
        AttachSubclassIfNeeded(root_hwnd);
        return root_hwnd;
      }
    }
  }
  return nullptr;
}

void XueHuaNaviteWindowManagerPlugin::AttachSubclassIfNeeded(HWND hwnd) {
  if (hwnd == nullptr || !IsWindow(hwnd)) {
    return;
  }
  if (is_subclassed_ && subclassed_hwnd_ == hwnd) {
    return;
  }
  DetachSubclass();
  if (SetWindowSubclass(hwnd, &XueHuaNaviteWindowManagerPlugin::SubclassProc,
                        internal::kWindowSubclassId,
                        reinterpret_cast<DWORD_PTR>(this))) {
    subclassed_hwnd_ = hwnd;
    is_subclassed_ = true;
    was_maximized_ = IsZoomed(hwnd) != FALSE;
    was_minimized_ = IsIconic(hwnd) != FALSE;
  }
}

void XueHuaNaviteWindowManagerPlugin::DetachSubclass() {
  if (is_subclassed_ && subclassed_hwnd_ != nullptr &&
      IsWindow(subclassed_hwnd_)) {
    RemoveWindowSubclass(subclassed_hwnd_,
                         &XueHuaNaviteWindowManagerPlugin::SubclassProc,
                         internal::kWindowSubclassId);
  }
  is_subclassed_ = false;
  subclassed_hwnd_ = nullptr;
  pending_events_.clear();
  event_flush_posted_ = false;
}

void XueHuaNaviteWindowManagerPlugin::QueueWindowEvent(const std::string& type,
                                                       HWND hwnd) {
  if (!event_sink_ || hwnd == nullptr || !IsWindow(hwnd)) {
    return;
  }
  flutter::EncodableMap payload = GetBoundsMap(hwnd);
  payload[flutter::EncodableValue("type")] = flutter::EncodableValue(type);
  payload[flutter::EncodableValue("isMaximized")] =
      flutter::EncodableValue(IsZoomed(hwnd) != FALSE);
  payload[flutter::EncodableValue("isFullScreen")] =
      flutter::EncodableValue(is_fullscreen_);
  payload[flutter::EncodableValue("timestamp")] =
      flutter::EncodableValue(internal::CurrentEpochMillis());
  pending_events_.push_back(std::move(payload));

  if (!event_flush_posted_) {
    event_flush_posted_ = true;
    if (!PostMessageW(hwnd, internal::kFlushQueuedEventsMessage, 0, 0)) {
      event_flush_posted_ = false;
    }
  }
}

void XueHuaNaviteWindowManagerPlugin::FlushQueuedWindowEvents() {
  event_flush_posted_ = false;
  if (pending_events_.empty()) {
    return;
  }
  std::vector<flutter::EncodableMap> batch;
  batch.swap(pending_events_);
  if (!event_sink_) {
    return;
  }
  for (const auto& payload : batch) {
    if (!event_sink_) {
      break;
    }
    event_sink_->Success(flutter::EncodableValue(payload));
  }
}

}  // namespace xue_hua_navite_window_manager

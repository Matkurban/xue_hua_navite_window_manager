#include "plugin_internal.h"

G_DEFINE_TYPE(XueHuaNaviteWindowManagerPlugin,
              xue_hua_navite_window_manager_plugin,
              g_object_get_type())

static FlMethodErrorResponse* event_listen_cb(FlEventChannel* channel,
                                              FlValue* args,
                                              gpointer user_data) {
  (void)channel;
  (void)args;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  self->event_listening = TRUE;
  return nullptr;
}

static FlMethodErrorResponse* event_cancel_cb(FlEventChannel* channel,
                                              FlValue* args,
                                              gpointer user_data) {
  (void)channel;
  (void)args;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  self->event_listening = FALSE;
  return nullptr;
}

static void xue_hua_navite_window_manager_plugin_dispose(GObject* object) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(object);

  xue_hua_plugin_remove_button_press_hook(self);

  if (self->interactive_regions != nullptr) {
    g_array_free(self->interactive_regions, TRUE);
    self->interactive_regions = nullptr;
  }
  xue_hua_plugin_cancel_synthetic_release(self);
  xue_hua_plugin_disconnect_view_signals(self);
  xue_hua_plugin_unhook_window(self);
  g_weak_ref_clear(&self->view);
  g_clear_object(&self->method_channel);
  g_clear_object(&self->event_channel);
  G_OBJECT_CLASS(xue_hua_navite_window_manager_plugin_parent_class)
      ->dispose(object);
}

static void xue_hua_navite_window_manager_plugin_class_init(
    XueHuaNaviteWindowManagerPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose =
      xue_hua_navite_window_manager_plugin_dispose;
}

static void xue_hua_navite_window_manager_plugin_init(
    XueHuaNaviteWindowManagerPlugin* self) {
  g_weak_ref_init(&self->view, nullptr);
  self->is_resizable = TRUE;
  self->last_mouse_button = 1;
  self->interactive_regions =
      g_array_new(FALSE, TRUE, sizeof(XueHuaInteractiveRegion));
}

static void method_call_cb(FlMethodChannel* channel,
                           FlMethodCall* method_call,
                           gpointer user_data) {
  (void)channel;
  XueHuaNaviteWindowManagerPlugin* plugin =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  xue_hua_navite_window_manager_plugin_handle_method_call(plugin, method_call);
}

void xue_hua_navite_window_manager_plugin_register_with_registrar(
    FlPluginRegistrar* registrar) {
  XueHuaNaviteWindowManagerPlugin* plugin =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(g_object_new(
          xue_hua_navite_window_manager_plugin_get_type(), nullptr));

  // The generated registrant stores this registrar in g_autoptr and unrefs it
  // when fl_register_plugins returns. Do not keep that pointer. The view is
  // tracked with a weak ref captured while the registrar is still valid.
  FlView* view = fl_plugin_registrar_get_view(registrar);
  if (view != nullptr) {
    g_weak_ref_set(&plugin->view, view);
  }
  xue_hua_plugin_watch_view(plugin);
  xue_hua_plugin_install_button_press_hook(plugin);

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  plugin->method_channel = fl_method_channel_new(
      fl_plugin_registrar_get_messenger(registrar),
      "xue_hua_navite_window_manager/methods", FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(
      plugin->method_channel, method_call_cb, g_object_ref(plugin),
      g_object_unref);

  plugin->event_channel = fl_event_channel_new(
      fl_plugin_registrar_get_messenger(registrar),
      "xue_hua_navite_window_manager/events", FL_METHOD_CODEC(codec));
  fl_event_channel_set_stream_handlers(plugin->event_channel, event_listen_cb,
                                       event_cancel_cb, g_object_ref(plugin),
                                       g_object_unref);

  g_object_unref(plugin);
}

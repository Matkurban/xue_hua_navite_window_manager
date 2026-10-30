#ifndef XUE_HUA_PLUGIN_INTERNAL_H_
#define XUE_HUA_PLUGIN_INTERNAL_H_

#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "fl_value_utils.h"
#include "launch_at_startup_logic.h"
#include "window_logic.h"

#define XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST(                    \
      (obj), xue_hua_navite_window_manager_plugin_get_type(), \
      XueHuaNaviteWindowManagerPlugin))

struct _XueHuaNaviteWindowManagerPlugin {
  GObject parent_instance;

  // Weak: the view owns the method channel, which owns this plugin.
  GWeakRef view;
  // Borrowed. Cleared from the window's destroy handler.
  GtkWindow* hooked_window;

  FlMethodChannel* method_channel;
  FlEventChannel* event_channel;

  gboolean event_listening;
  gboolean prevent_close;
  gboolean is_force_closing;
  gboolean is_maximized;
  gboolean is_minimized;
  gboolean is_fullscreen;
  gboolean is_focused;
  gboolean is_always_on_top;
  gboolean is_resizable;

  gint min_width;
  gint min_height;
  gint max_width;
  gint max_height;

  // Deferred windowed geometry re-applied once the WM/compositor finishes
  // async unmaximize/unfullscreen.
  gboolean pending_restore_geometry;
  gboolean pending_has_size;
  gint pending_width;
  gint pending_height;
  gboolean pending_center;
  gboolean pending_has_pos;
  gint pending_x;
  gint pending_y;

  gboolean has_button_event;
  gint last_mouse_root_x;
  gint last_mouse_root_y;
  guint32 last_mouse_button;
  guint32 last_mouse_time;
  gboolean move_drag_active;
  gboolean resize_drag_active;
  gboolean warned_wayland_drag;
  guint32 sync_press_time;
  GArray* interactive_regions;
  guint synthetic_release_idle_id;

  gulong delete_event_id;
  gulong close_request_id;
  gulong configure_event_id;
  gulong window_state_id;
  gulong event_after_id;
  gulong destroy_id;
  gulong realize_id;
  gulong hierarchy_id;
  gboolean view_signals_installed;
  gulong button_press_hook_id;
  guint button_press_signal_id;
};

// Window resolution and hook management (window_hooks_and_events.cc)
GtkWindow* xue_hua_plugin_get_main_window(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_try_hook_window(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_unhook_window(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_watch_view(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_disconnect_view_signals(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_emit_window_event(XueHuaNaviteWindowManagerPlugin* self,
                                      GtkWindow* window,
                                      const gchar* type);

// Window frame controller (window_frame_controller.cc)
void xue_hua_plugin_apply_geometry_hints(XueHuaNaviteWindowManagerPlugin* self,
                                         GtkWindow* window);
void xue_hua_plugin_apply_background_color(XueHuaNaviteWindowManagerPlugin* self,
                                           GtkWindow* window,
                                           int64_t argb);
void xue_hua_plugin_apply_title_bar_style(GtkWindow* window, const gchar* style);
void xue_hua_plugin_show_main_window(XueHuaNaviteWindowManagerPlugin* self,
                                     GtkWindow* window,
                                     bool present);
void xue_hua_plugin_clear_pending_restore_geometry(
    XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_record_pending_restore_geometry(
    XueHuaNaviteWindowManagerPlugin* self,
    bool has_size,
    gint width,
    gint height,
    bool center,
    bool has_pos,
    gint x,
    gint y);
bool xue_hua_plugin_restore_window_for_geometry(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window,
    bool has_geometry_request);
bool xue_hua_plugin_center_window(GtkWindow* window, gint width, gint height);
XueHuaClampedSize xue_hua_plugin_clamp_to_stored_limits(
    XueHuaNaviteWindowManagerPlugin* self,
    int width,
    int height);
FlValue* xue_hua_plugin_get_bounds_map(GtkWindow* window);
FlValue* xue_hua_plugin_build_window_state_snapshot(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window);

// Interactive dragging & Wayland sync gestures (window_interactive_drag.cc)
void xue_hua_plugin_install_button_press_hook(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_remove_button_press_hook(XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_upsert_interactive_region(
    XueHuaNaviteWindowManagerPlugin* self,
    const XueHuaInteractiveRegion& region,
    bool present);
bool xue_hua_plugin_prepare_move_drag(XueHuaNaviteWindowManagerPlugin* self,
                                      GtkWindow* window,
                                      guint32* timestamp_out,
                                      gint* root_x_out,
                                      gint* root_y_out,
                                      gint* button_out);
bool xue_hua_plugin_async_drag_already_synced(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window);
void xue_hua_plugin_schedule_synthetic_release(
    XueHuaNaviteWindowManagerPlugin* self);
void xue_hua_plugin_cancel_synthetic_release(
    XueHuaNaviteWindowManagerPlugin* self);

// Method call dispatch (window_method_dispatcher.cc)
void xue_hua_navite_window_manager_plugin_handle_method_call(
    XueHuaNaviteWindowManagerPlugin* self,
    FlMethodCall* method_call);

#endif  // XUE_HUA_PLUGIN_INTERNAL_H_

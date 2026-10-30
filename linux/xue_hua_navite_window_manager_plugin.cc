#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gdk/gdk.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "xue_hua_navite_window_manager_plugin_private.h"
#include "xue_hua_window_logic.h"

#define XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST(                    \
      (obj), xue_hua_navite_window_manager_plugin_get_type(), \
      XueHuaNaviteWindowManagerPlugin))

struct _XueHuaNaviteWindowManagerPlugin {
  GObject parent_instance;

  // Weak: the view (via the engine messenger) owns the method channel, which
  // owns this plugin. A strong ref would leak the Flutter view.
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

  // Deferred windowed geometry re-applied once the X11 WM / Wayland compositor
  // finishes processing an async unmaximize / unfullscreen / deiconify request,
  // preventing the WM from overwriting the requested size with its saved
  // pre-maximize bounds.
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
  // Event time of a Wayland move/resize already started inside the button-press emission hook.
  // The matching async startDragging/startResizing call must not start again.
  guint32 sync_press_time;
  // XueHuaInteractiveRegion elements. Owned by the plugin.
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
  // Wayland never invokes gdk_window_add_filter (X11-only). Capture presses
  // with a button-press-event emission hook that runs on every backend before
  // FlView's event_box handler returns TRUE.
  gulong button_press_hook_id;
  guint button_press_signal_id;
};

G_DEFINE_TYPE(XueHuaNaviteWindowManagerPlugin,
              xue_hua_navite_window_manager_plugin,
              g_object_get_type())

// Forward declarations
static GtkWindow* get_main_window(XueHuaNaviteWindowManagerPlugin* self);
static bool center_window(GtkWindow* window, gint width, gint height);
static FlValue* build_window_state_snapshot(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window);
static void emit_window_event(XueHuaNaviteWindowManagerPlugin* self,
                              GtkWindow* window,
                              const gchar* type);
static void try_hook_window(XueHuaNaviteWindowManagerPlugin* self);
static void unhook_window(XueHuaNaviteWindowManagerPlugin* self);

// Lookup helpers for FlValue maps
static FlValue* lookup_map_value(FlValue* args, const gchar* key) {
  if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return nullptr;
  }
  return fl_value_lookup_string(args, key);
}

static gboolean get_double_arg(FlValue* args,
                               const gchar* key,
                               gdouble* out_val) {
  FlValue* v = lookup_map_value(args, key);
  if (v == nullptr) return FALSE;
  if (fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT) {
    *out_val = fl_value_get_float(v);
    return TRUE;
  }
  if (fl_value_get_type(v) == FL_VALUE_TYPE_INT) {
    *out_val = static_cast<gdouble>(fl_value_get_int(v));
    return TRUE;
  }
  return FALSE;
}

static gboolean get_bool_arg(FlValue* args,
                             const gchar* key,
                             gboolean default_val) {
  FlValue* v = lookup_map_value(args, key);
  if (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_BOOL) {
    return fl_value_get_bool(v);
  }
  return default_val;
}

static gboolean has_key_arg(FlValue* args, const gchar* key) {
  return lookup_map_value(args, key) != nullptr;
}

static gboolean display_is_wayland(GdkDisplay* display) {
  if (display == nullptr) {
    return FALSE;
  }
  // "wayland-0" when GTK is on the Wayland backend. An XWayland session
  // reports an X11 display name and can use the X11 drag fallback.
  const gchar* name = gdk_display_get_name(display);
  return name != nullptr && g_str_has_prefix(name, "wayland");
}

static void disconnect_signal_handlers_with_data(gpointer instance,
                                                 const gchar* signal_name,
                                                 GType owner_type,
                                                 gpointer data) {
  if (instance == nullptr || data == nullptr || signal_name == nullptr) {
    return;
  }
  const guint signal_id = g_signal_lookup(signal_name, owner_type);
  if (signal_id == 0) {
    return;
  }
  const GSignalMatchType match = static_cast<GSignalMatchType>(
      G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA);
  while (true) {
    const gulong handler_id = g_signal_handler_find(
        instance, match, signal_id, 0, nullptr, nullptr, data);
    if (handler_id == 0) {
      break;
    }
    g_signal_handler_disconnect(instance, handler_id);
  }
}

// Block every user handler for the signal, then unblock ours. Flutter's
// realize callback returns TRUE from delete-event and stops emission; if its
// user-data is not the FlView, the data match above misses it.
static void block_handlers_except(gpointer instance,
                                  const gchar* signal_name,
                                  GType owner_type,
                                  gulong keep_id) {
  if (instance == nullptr || keep_id == 0) {
    return;
  }
  const guint signal_id = g_signal_lookup(signal_name, owner_type);
  if (signal_id == 0) {
    return;
  }
  g_signal_handlers_block_matched(
      instance, static_cast<GSignalMatchType>(G_SIGNAL_MATCH_ID), signal_id, 0,
      nullptr, nullptr, nullptr);
  g_signal_handler_unblock(instance, keep_id);
}

// Flutter's FlView realize handler connects delete-event with the view as
// user data, calls fl_engine_request_app_exit, and returns TRUE. That stops
// emission before a plugin handler runs, so preventClose never sees the
// request and the process exits. Detach that handler; our handler decides.
static void suppress_embedder_close_handlers(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window) {
  if (window == nullptr) {
    return;
  }
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view != nullptr) {
    disconnect_signal_handlers_with_data(window, "delete-event", GTK_TYPE_WIDGET,
                                         view);
    disconnect_signal_handlers_with_data(
        window, "close-request", GTK_TYPE_WINDOW, view);
  }
  block_handlers_except(window, "delete-event", GTK_TYPE_WIDGET,
                        self->delete_event_id);
  block_handlers_except(window, "close-request", GTK_TYPE_WINDOW,
                        self->close_request_id);
}

static gboolean on_window_delete_event(GtkWidget* widget,
                                       GdkEvent* event,
                                       gpointer user_data) {
  (void)event;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (xue_hua_should_intercept_close(self->prevent_close != FALSE,
                                     self->is_force_closing != FALSE)) {
    emit_window_event(self, GTK_WINDOW(widget), "closeRequested");
    return TRUE;
  }
  return FALSE;
}

// GTK 4 renamed delete-event to close-request (no GdkEvent argument). GTK 3
// does not have the signal; it is connected only when g_signal_lookup finds it.
static gboolean on_window_close_request(GtkWindow* window, gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (xue_hua_should_intercept_close(self->prevent_close != FALSE,
                                     self->is_force_closing != FALSE)) {
    emit_window_event(self, window, "closeRequested");
    return TRUE;
  }
  return FALSE;
}

static gboolean on_window_configure_event(GtkWidget* widget,
                                          GdkEventConfigure* event,
                                          gpointer user_data) {
  (void)event;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  GtkWindow* window = GTK_WINDOW(widget);
  emit_window_event(self, window, "resized");
  emit_window_event(self, window, "moved");
  return FALSE;
}

static gboolean on_window_state_event(GtkWidget* widget,
                                      GdkEventWindowState* event,
                                      gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  GtkWindow* window = GTK_WINDOW(widget);

  if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
    const gboolean now_maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    if (self->is_maximized != now_maximized) {
      self->is_maximized = now_maximized;
      emit_window_event(self, window,
                        self->is_maximized ? "maximized" : "unmaximized");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
    const gboolean now_minimized =
        (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
    if (self->is_minimized != now_minimized) {
      self->is_minimized = now_minimized;
      emit_window_event(self, window,
                        self->is_minimized ? "minimized" : "restored");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    const gboolean now_fullscreen =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    if (self->is_fullscreen != now_fullscreen) {
      self->is_fullscreen = now_fullscreen;
      emit_window_event(
          self, window,
          self->is_fullscreen ? "enteredFullScreen" : "exitedFullScreen");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
    const gboolean now_focused =
        (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) != 0;
    if (self->is_focused != now_focused) {
      self->is_focused = now_focused;
      emit_window_event(self, window, self->is_focused ? "focused" : "blurred");
    }
  }

  if (self->pending_restore_geometry &&
      (event->new_window_state &
       (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN |
        GDK_WINDOW_STATE_ICONIFIED)) == 0) {
    self->pending_restore_geometry = FALSE;
    if (self->pending_has_size && self->pending_width > 0 &&
        self->pending_height > 0) {
      gtk_window_set_default_size(window, self->pending_width,
                                  self->pending_height);
      gtk_window_resize(window, self->pending_width, self->pending_height);
    }
    if (self->pending_center) {
      center_window(window, self->pending_has_size ? self->pending_width : 0,
                    self->pending_has_size ? self->pending_height : 0);
    } else if (self->pending_has_pos) {
      gtk_window_move(window, self->pending_x, self->pending_y);
    }
  }
  return FALSE;
}

static void emit_release_tree(GtkWidget* widget, GdkEvent* event);

static void emit_release_foreach(GtkWidget* widget, gpointer data) {
  emit_release_tree(widget, static_cast<GdkEvent*>(data));
}

static void emit_release_tree(GtkWidget* widget, GdkEvent* event) {
  if (widget == nullptr) {
    return;
  }
  gboolean handled = FALSE;
  g_signal_emit_by_name(widget, "button-release-event", event, &handled);
  if (GTK_IS_CONTAINER(widget)) {
    gtk_container_foreach(GTK_CONTAINER(widget), emit_release_foreach, event);
  }
}

static void synthesize_button_release(XueHuaNaviteWindowManagerPlugin* self) {
  if (!self->move_drag_active && !self->resize_drag_active) {
    return;
  }
  self->move_drag_active = FALSE;
  self->resize_drag_active = FALSE;

  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view == nullptr) {
    return;
  }

  GdkEvent* event = gdk_event_new(GDK_BUTTON_RELEASE);
  GdkEventButton* button = reinterpret_cast<GdkEventButton*>(event);
  button->button = self->last_mouse_button != 0 ? self->last_mouse_button : 1;
  button->time = self->last_mouse_time != 0 ? self->last_mouse_time
                                            : gtk_get_current_event_time();
  button->x_root = self->last_mouse_root_x;
  button->y_root = self->last_mouse_root_y;
  button->send_event = TRUE;
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(view));
  if (gdk_window != nullptr) {
    button->window = GDK_WINDOW(g_object_ref(gdk_window));
    gint origin_x = 0;
    gint origin_y = 0;
    gdk_window_get_origin(gdk_window, &origin_x, &origin_y);
    button->x = button->x_root - origin_x;
    button->y = button->y_root - origin_y;
  }
  // The WM grab consumes the real button-release, so Flutter keeps the
  // primary button down and drops the next press. Deliver a release to the
  // view after the native drag has been handed to the compositor.
  emit_release_tree(GTK_WIDGET(view), event);
  gdk_event_free(event);
}

static gboolean on_synthetic_release_idle(gpointer data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(data);
  self->synthetic_release_idle_id = 0;
  synthesize_button_release(self);
  return G_SOURCE_REMOVE;
}

static void schedule_synthetic_release(XueHuaNaviteWindowManagerPlugin* self) {
  if (self->synthetic_release_idle_id != 0) {
    return;
  }
  if (!self->move_drag_active && !self->resize_drag_active) {
    return;
  }
  self->synthetic_release_idle_id = g_idle_add(on_synthetic_release_idle, self);
}

static void on_window_event_after(GtkWidget* widget,
                                  GdkEvent* event,
                                  gpointer user_data) {
  (void)widget;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (event == nullptr) {
    return;
  }
  if (event->type != GDK_ENTER_NOTIFY && event->type != GDK_BUTTON_RELEASE &&
      event->type != GDK_GRAB_BROKEN) {
    return;
  }
  schedule_synthetic_release(self);
}

static void on_window_destroy(GtkWidget* widget, gpointer user_data) {
  (void)widget;
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (self->hooked_window == nullptr) {
    return;
  }
  // The window is finalizing and drops the remaining handlers with it.
  self->delete_event_id = 0;
  self->close_request_id = 0;
  self->configure_event_id = 0;
  self->window_state_id = 0;
  self->event_after_id = 0;
  self->destroy_id = 0;
  self->hooked_window = nullptr;
}

static void unhook_window(XueHuaNaviteWindowManagerPlugin* self) {
  if (self->hooked_window == nullptr) {
    return;
  }
  GtkWindow* window = self->hooked_window;
  self->hooked_window = nullptr;
  if (self->delete_event_id != 0) {
    g_signal_handler_disconnect(window, self->delete_event_id);
    self->delete_event_id = 0;
  }
  if (self->close_request_id != 0) {
    g_signal_handler_disconnect(window, self->close_request_id);
    self->close_request_id = 0;
  }
  if (self->configure_event_id != 0) {
    g_signal_handler_disconnect(window, self->configure_event_id);
    self->configure_event_id = 0;
  }
  if (self->window_state_id != 0) {
    g_signal_handler_disconnect(window, self->window_state_id);
    self->window_state_id = 0;
  }
  if (self->event_after_id != 0) {
    g_signal_handler_disconnect(window, self->event_after_id);
    self->event_after_id = 0;
  }
  if (self->destroy_id != 0) {
    g_signal_handler_disconnect(window, self->destroy_id);
    self->destroy_id = 0;
  }
}

static void ensure_window_hooks(XueHuaNaviteWindowManagerPlugin* self,
                                GtkWindow* window) {
  if (window == nullptr || gtk_widget_in_destruction(GTK_WIDGET(window))) {
    return;
  }
  if (self->hooked_window != window) {
    unhook_window(self);
    self->hooked_window = window;
    gtk_widget_add_events(GTK_WIDGET(window),
                          GDK_STRUCTURE_MASK | GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    self->delete_event_id = g_signal_connect(
        window, "delete-event", G_CALLBACK(on_window_delete_event), self);
    if (g_signal_lookup("close-request", GTK_TYPE_WINDOW) != 0) {
      self->close_request_id = g_signal_connect(
          window, "close-request", G_CALLBACK(on_window_close_request), self);
    }
    self->configure_event_id = g_signal_connect(
        window, "configure-event", G_CALLBACK(on_window_configure_event), self);
    self->window_state_id = g_signal_connect(
        window, "window-state-event", G_CALLBACK(on_window_state_event), self);
    self->event_after_id = g_signal_connect(
        window, "event-after", G_CALLBACK(on_window_event_after), self);
    self->destroy_id =
        g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), self);
  }
  // Realize can attach the embedder handler after our own connect.
  suppress_embedder_close_handlers(self, window);
}

static void try_hook_window(XueHuaNaviteWindowManagerPlugin* self) {
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view == nullptr) {
    return;
  }
  GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
  if (toplevel == nullptr || !GTK_IS_WINDOW(toplevel)) {
    return;
  }
  ensure_window_hooks(self, GTK_WINDOW(toplevel));
}

static void on_view_realize(GtkWidget* widget, gpointer user_data) {
  (void)widget;
  try_hook_window(XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data));
}

static void on_view_hierarchy_changed(GtkWidget* widget,
                                      GtkWidget* previous_toplevel,
                                      gpointer user_data) {
  (void)widget;
  (void)previous_toplevel;
  try_hook_window(XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data));
}

static void watch_view(XueHuaNaviteWindowManagerPlugin* self) {
  if (self->view_signals_installed) {
    try_hook_window(self);
    return;
  }
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view == nullptr) {
    return;
  }
  // after: FlView's realize handler is what connects the embedder close hook.
  self->realize_id = g_signal_connect_after(view, "realize",
                                            G_CALLBACK(on_view_realize), self);
  self->hierarchy_id = g_signal_connect(
      view, "hierarchy-changed", G_CALLBACK(on_view_hierarchy_changed), self);
  self->view_signals_installed = TRUE;
  try_hook_window(self);
}

static void remember_button_press(XueHuaNaviteWindowManagerPlugin* self,
                                  const GdkEventButton* event) {
  if (event == nullptr) {
    return;
  }
  self->has_button_event = TRUE;
  self->last_mouse_root_x = static_cast<gint>(event->x_root);
  self->last_mouse_root_y = static_cast<gint>(event->y_root);
  self->last_mouse_button = event->button;
  self->last_mouse_time = event->time;
}

static bool gdk_edge_from_kind(int kind, GdkWindowEdge* edge) {
  if (edge == nullptr) {
    return false;
  }
  switch (kind) {
    case XUE_HUA_INTERACTIVE_RESIZE_NORTH:
      *edge = GDK_WINDOW_EDGE_NORTH;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_SOUTH:
      *edge = GDK_WINDOW_EDGE_SOUTH;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_WEST:
      *edge = GDK_WINDOW_EDGE_WEST;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_EAST:
      *edge = GDK_WINDOW_EDGE_EAST;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_NORTH_WEST:
      *edge = GDK_WINDOW_EDGE_NORTH_WEST;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_NORTH_EAST:
      *edge = GDK_WINDOW_EDGE_NORTH_EAST;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_SOUTH_WEST:
      *edge = GDK_WINDOW_EDGE_SOUTH_WEST;
      return true;
    case XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST:
      *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
      return true;
    default:
      return false;
  }
}

static bool view_local_from_button(XueHuaNaviteWindowManagerPlugin* self,
                                   const GdkEventButton* event,
                                   double* x_out,
                                   double* y_out) {
  if (event == nullptr || event->window == nullptr || x_out == nullptr ||
      y_out == nullptr) {
    return false;
  }
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view == nullptr) {
    return false;
  }
  GdkWindow* view_window = gtk_widget_get_window(GTK_WIDGET(view));
  if (view_window == nullptr) {
    return false;
  }
  gint view_x = 0;
  gint view_y = 0;
  gint event_x = 0;
  gint event_y = 0;
  gdk_window_get_origin(view_window, &view_x, &view_y);
  gdk_window_get_origin(event->window, &event_x, &event_y);
  *x_out = xue_hua_view_local(event->x, event_x, view_x);
  *y_out = xue_hua_view_local(event->y, event_y, view_y);
  return true;
}

// GTK3 Wayland ignores the timestamp passed to gtk_window_begin_move_drag and
// sends the seat's last implicit-grab serial (set while translating this
// button event). Mutter, KWin, and wlroots accept that serial when the
// request is made before the press dispatch returns. A later method-channel
// call often loses the grab. X11 is unchanged: this returns without calling
// begin_move_drag, and startDragging keeps the async path.
//
// Must run during button-press-event emission (see the emission hook), not
// from a GDK window filter: filters are applied only on the X11 backend.
static void try_wayland_sync_gesture(XueHuaNaviteWindowManagerPlugin* self,
                                     GtkWindow* window,
                                     GdkEventButton* event) {
  if (self->interactive_regions == nullptr || event == nullptr ||
      window == nullptr) {
    return;
  }
  if (event->type != GDK_BUTTON_PRESS) {
    return;
  }
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  if (!display_is_wayland(display)) {
    return;
  }
  double local_x = 0.0;
  double local_y = 0.0;
  if (!view_local_from_button(self, event, &local_x, &local_y)) {
    return;
  }
  const XueHuaInteractiveRegion* regions = nullptr;
  if (self->interactive_regions->len > 0) {
    regions = &g_array_index(self->interactive_regions,
                             XueHuaInteractiveRegion, 0);
  }
  const XueHuaPressPlan plan = xue_hua_plan_interactive_press(
      true, event->button == GDK_BUTTON_PRIMARY, false, local_x, local_y,
      regions, static_cast<int>(self->interactive_regions->len));
  if (plan.action == XUE_HUA_PRESS_IGNORE) {
    return;
  }

  const gint root_x = static_cast<gint>(event->x_root);
  const gint root_y = static_cast<gint>(event->y_root);
  const guint32 timestamp = event->time;
  if (plan.action == XUE_HUA_PRESS_MOVE) {
    self->move_drag_active = TRUE;
    self->resize_drag_active = FALSE;
    self->sync_press_time = timestamp;
    g_info(
        "xue_hua_navite_window_manager: Wayland sync begin_move_drag at "
        "local (%.1f, %.1f) with press time %u",
        local_x, local_y, timestamp);
    gtk_window_begin_move_drag(window, event->button, root_x, root_y,
                               timestamp);
    return;
  }

  GdkWindowEdge edge = GDK_WINDOW_EDGE_SOUTH_EAST;
  if (!gdk_edge_from_kind(plan.kind, &edge)) {
    return;
  }
  if (!gtk_window_get_resizable(window) || gtk_window_is_maximized(window)) {
    return;
  }
  self->resize_drag_active = TRUE;
  self->move_drag_active = FALSE;
  self->sync_press_time = timestamp;
  g_info(
      "xue_hua_navite_window_manager: Wayland sync begin_resize_drag kind=%d "
      "at local (%.1f, %.1f) with press time %u",
      plan.kind, local_x, local_y, timestamp);
  gtk_window_begin_resize_drag(window, edge, event->button, root_x, root_y,
                               timestamp);
}

static bool async_drag_already_synced(XueHuaNaviteWindowManagerPlugin* self,
                                      GtkWindow* window) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  return display_is_wayland(display) && self->sync_press_time != 0 &&
         self->sync_press_time == self->last_mouse_time;
}

static bool get_int_arg(FlValue* args, const gchar* key, int64_t* out_val) {
  FlValue* v = lookup_map_value(args, key);
  if (v == nullptr) {
    return false;
  }
  if (fl_value_get_type(v) == FL_VALUE_TYPE_INT) {
    *out_val = fl_value_get_int(v);
    return true;
  }
  if (fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT) {
    *out_val = static_cast<int64_t>(std::lround(fl_value_get_float(v)));
    return true;
  }
  return false;
}

static void upsert_interactive_region(XueHuaNaviteWindowManagerPlugin* self,
                                      const XueHuaInteractiveRegion& region,
                                      bool present) {
  if (self->interactive_regions == nullptr) {
    return;
  }
  for (guint i = 0; i < self->interactive_regions->len; i++) {
    XueHuaInteractiveRegion* existing = &g_array_index(
        self->interactive_regions, XueHuaInteractiveRegion, i);
    if (existing->id != region.id) {
      continue;
    }
    if (!present) {
      g_array_remove_index(self->interactive_regions, i);
    } else {
      *existing = region;
    }
    return;
  }
  if (present) {
    XueHuaInteractiveRegion stored = region;
    g_array_append_val(self->interactive_regions, stored);
    g_info(
        "xue_hua_navite_window_manager: registered interactive region id=%d "
        "kind=%d rect=(%.1f, %.1f) %.1fx%.1f (count=%u)",
        stored.id, stored.kind, stored.x, stored.y, stored.width, stored.height,
        self->interactive_regions->len);
  }
}

// gdk_window_add_filter only runs on X11 (see gdk/x11/gdkeventsource.c).
// Wayland emits GdkEvents without consulting those filters, so the previous
// global filter never saw presses under weston/Mutter and left last_mouse_time
// unset. A button-press-event emission hook runs on every backend at the
// start of emission — before FlView's event_box handler returns TRUE.
static gboolean on_button_press_emission_hook(GSignalInvocationHint* ihint,
                                              guint n_param_values,
                                              const GValue* param_values,
                                              gpointer user_data) {
  (void)ihint;
  if (n_param_values < 2 || param_values == nullptr || user_data == nullptr) {
    return TRUE;
  }
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(&param_values[0]));
  GdkEvent* event = static_cast<GdkEvent*>(g_value_get_boxed(&param_values[1]));
  if (widget == nullptr || event == nullptr ||
      event->type != GDK_BUTTON_PRESS) {
    return TRUE;
  }
  if (self->hooked_window == nullptr || !GTK_IS_WINDOW(self->hooked_window)) {
    try_hook_window(self);
  }
  if (self->hooked_window == nullptr || !GTK_IS_WINDOW(self->hooked_window)) {
    return TRUE;
  }
  GtkWidget* top = gtk_widget_get_toplevel(widget);
  if (top != GTK_WIDGET(self->hooked_window)) {
    return TRUE;
  }
  // FlView's GtkEventBox owns the press. Capture the seat serial and, on
  // Wayland, start move/resize before that handler finishes.
  remember_button_press(self, &event->button);
  try_wayland_sync_gesture(self, self->hooked_window, &event->button);
  return TRUE;
}

static GtkWindow* get_main_window(XueHuaNaviteWindowManagerPlugin* self) {
  try_hook_window(self);
  if (self->hooked_window != nullptr && GTK_IS_WINDOW(self->hooked_window)) {
    return self->hooked_window;
  }
  return nullptr;
}

static XueHuaClampedSize clamp_to_stored_limits(
    XueHuaNaviteWindowManagerPlugin* self,
    int width,
    int height) {
  return xue_hua_clamp_window_size(width, height, self->min_width,
                                   self->min_height, self->max_width,
                                   self->max_height);
}

static void apply_geometry_hints(XueHuaNaviteWindowManagerPlugin* self,
                                 GtkWindow* window) {
  GdkGeometry geometry{};
  int mask = 0;

  if (self->min_width > 0 || self->min_height > 0) {
    geometry.min_width = self->min_width > 0 ? self->min_width : 1;
    geometry.min_height = self->min_height > 0 ? self->min_height : 1;
    mask |= GDK_HINT_MIN_SIZE;
  }
  if (self->max_width > 0 || self->max_height > 0) {
    geometry.max_width = self->max_width > 0 ? self->max_width : G_MAXINT;
    geometry.max_height = self->max_height > 0 ? self->max_height : G_MAXINT;
    mask |= GDK_HINT_MAX_SIZE;
  }

  if (mask != 0) {
    gtk_window_set_geometry_hints(window, nullptr, &geometry,
                                  static_cast<GdkWindowHints>(mask));
  }
}

static void apply_background_color(XueHuaNaviteWindowManagerPlugin* self,
                                   GtkWindow* window,
                                   int64_t argb) {
  (void)window;
  const gdouble a = ((argb >> 24) & 0xFF) / 255.0;
  const gdouble r = ((argb >> 16) & 0xFF) / 255.0;
  const gdouble g = ((argb >> 8) & 0xFF) / 255.0;
  const gdouble b = (argb & 0xFF) / 255.0;

  GdkRGBA rgba{r, g, b, a};
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view != nullptr) {
    fl_view_set_background_color(view, &rgba);
  }
}

static void apply_title_bar_style(GtkWindow* window, const gchar* style) {
  const gboolean custom_frame =
      (g_strcmp0(style, "hidden") == 0 || g_strcmp0(style, "immersive") == 0);
  GtkWidget* custom_titlebar = gtk_window_get_titlebar(window);
  if (custom_frame) {
    if (custom_titlebar != nullptr) {
      gtk_widget_set_visible(custom_titlebar, FALSE);
    }
    gtk_window_set_decorated(window, FALSE);
  } else {
    gtk_window_set_decorated(window, TRUE);
    if (custom_titlebar != nullptr) {
      gtk_widget_set_visible(custom_titlebar, TRUE);
    }
  }
}

static void show_main_window(XueHuaNaviteWindowManagerPlugin* self,
                             GtkWindow* window,
                             bool present) {
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view != nullptr) {
    gtk_widget_show(GTK_WIDGET(view));
  }
  // Use gtk_widget_show instead of gtk_widget_show_all on the top-level window
  // so a hidden custom GtkHeaderBar (TitleBarStyle.hidden / immersive) is never
  // force-shown recursively.
  gtk_widget_show(GTK_WIDGET(window));
  if (present) {
    gtk_window_present(window);
    if (view != nullptr) {
      gtk_widget_grab_focus(GTK_WIDGET(view));
    }
  }
}

static void clear_pending_restore_geometry(
    XueHuaNaviteWindowManagerPlugin* self) {
  self->pending_restore_geometry = FALSE;
  self->pending_has_size = FALSE;
  self->pending_center = FALSE;
  self->pending_has_pos = FALSE;
}

static void record_pending_restore_geometry(
    XueHuaNaviteWindowManagerPlugin* self,
    bool has_size,
    gint width,
    gint height,
    bool center,
    bool has_pos,
    gint x,
    gint y) {
  self->pending_restore_geometry = TRUE;
  self->pending_has_size = has_size ? TRUE : FALSE;
  self->pending_width = width;
  self->pending_height = height;
  self->pending_center = center ? TRUE : FALSE;
  self->pending_has_pos = has_pos ? TRUE : FALSE;
  self->pending_x = x;
  self->pending_y = y;
}

// Restores a fullscreen, maximized, or minimized GtkWindow before applying
// explicit windowed geometry. Returns true when an async WM transition was
// initiated on a mapped window so the caller can also record a deferred
// post-restore geometry pass for EWMH / Wayland compositors.
static bool restore_window_for_geometry(XueHuaNaviteWindowManagerPlugin* self,
                                        GtkWindow* window,
                                        bool has_geometry_request) {
  const bool live_maximized =
      (self->is_maximized != FALSE) ||
      (gtk_window_is_maximized(window) != FALSE);
  GdkWindow* gdk_win = gtk_widget_get_window(GTK_WIDGET(window));
  const GdkWindowState gdk_state =
      gdk_win != nullptr ? gdk_window_get_state(gdk_win)
                         : static_cast<GdkWindowState>(0);
  const bool live_fullscreen =
      (self->is_fullscreen != FALSE) ||
      ((gdk_state & GDK_WINDOW_STATE_FULLSCREEN) != 0);
  const bool live_minimized =
      (self->is_minimized != FALSE) ||
      ((gdk_state & GDK_WINDOW_STATE_ICONIFIED) != 0);

  const XueHuaGeometryRestorePlan plan = xue_hua_plan_geometry_restore(
      has_geometry_request, live_fullscreen, live_maximized, live_minimized);
  if (!plan.unfullscreen && !plan.unmaximize && !plan.deiconify) {
    return false;
  }

  if (plan.unfullscreen) {
    gtk_window_unfullscreen(window);
    if (self->is_fullscreen) {
      self->is_fullscreen = FALSE;
      emit_window_event(self, window, "exitedFullScreen");
    }
  }
  if (plan.unmaximize) {
    gtk_window_unmaximize(window);
    if (self->is_maximized) {
      self->is_maximized = FALSE;
      emit_window_event(self, window, "unmaximized");
    }
  }
  if (plan.deiconify) {
    gtk_window_deiconify(window);
    if (self->is_minimized) {
      self->is_minimized = FALSE;
      emit_window_event(self, window, "restored");
    }
  }
  return gtk_widget_get_mapped(GTK_WIDGET(window)) != FALSE;
}

// Returns whether the position change is known to be applied. Resizing still
// runs on every backend; only the move is backend-specific.
static bool center_window(GtkWindow* window, gint width, gint height) {
  if (width > 0 && height > 0) {
    gtk_window_resize(window, width, height);
    gtk_window_set_default_size(window, width, height);
  }

  GtkWidget* widget = GTK_WIDGET(window);
  GdkDisplay* display = gtk_widget_get_display(widget);
  const bool wayland = display_is_wayland(display) != FALSE;
  const bool mapped = gtk_widget_get_mapped(widget) != FALSE;
  const XueHuaCenterAction action = xue_hua_plan_center(mapped, wayland);

  // Unmapped windows, including the first show: GTK_WIN_POS_CENTER is read
  // when the window maps. On Wayland this is only a hint (xdg-shell lets the
  // compositor place the toplevel). On X11 it is honored.
  if (action == XUE_HUA_CENTER_MAP_HINT) {
    gtk_window_set_position(window, GTK_WIN_POS_CENTER);
    return xue_hua_center_position_applied(action, wayland);
  }
  // Mapped Wayland: do not call gtk_window_move. The request is ignored and
  // GTK would cache a position the compositor never applied.
  if (action == XUE_HUA_CENTER_WAYLAND_IGNORED) {
    return false;
  }

  gint target_w = width;
  gint target_h = height;
  if (target_w <= 0 || target_h <= 0) {
    gtk_window_get_size(window, &target_w, &target_h);
  }

  GdkMonitor* monitor = nullptr;
  GdkWindow* gdk_window = gtk_widget_get_window(widget);
  if (display != nullptr && gdk_window != nullptr) {
    monitor = gdk_display_get_monitor_at_window(display, gdk_window);
  }
  if (monitor == nullptr && display != nullptr &&
      gdk_display_get_n_monitors(display) > 0) {
    monitor = gdk_display_get_monitor(display, 0);
  }
  if (monitor == nullptr) {
    gtk_window_set_position(window, GTK_WIN_POS_CENTER);
    return true;
  }

  GdkRectangle area;
  gdk_monitor_get_workarea(monitor, &area);
  if (area.width <= 0 || area.height <= 0) {
    gdk_monitor_get_geometry(monitor, &area);
  }
  const XueHuaCenteredOrigin origin = xue_hua_compute_centered_origin(
      area.x, area.y, area.width, area.height, target_w, target_h);
  gtk_window_move(window, origin.x, origin.y);
  return true;
}

static gboolean query_pointer_root(GtkWindow* window, gint* x, gint* y) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  if (display == nullptr) {
    return FALSE;
  }
  GdkSeat* seat = gdk_display_get_default_seat(display);
  if (seat == nullptr) {
    return FALSE;
  }
  GdkDevice* pointer = gdk_seat_get_pointer(seat);
  if (pointer == nullptr) {
    return FALSE;
  }
  gdk_device_get_position(pointer, nullptr, x, y);
  return TRUE;
}

static gboolean prepare_move_drag(XueHuaNaviteWindowManagerPlugin* self,
                                  GtkWindow* window,
                                  guint32* timestamp_out,
                                  gint* root_x_out,
                                  gint* root_y_out,
                                  gint* button_out) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  const XueHuaMoveDragPlan plan = xue_hua_plan_move_drag(
      self->has_button_event != FALSE, self->last_mouse_time,
      gtk_get_current_event_time(), display_is_wayland(display) != FALSE);
  if (!plan.attempt) {
    if (!self->warned_wayland_drag) {
      self->warned_wayland_drag = TRUE;
      g_warning(
          "xue_hua_navite_window_manager: skipped async "
          "startDragging/startResizing on Wayland because no button-press "
          "serial was captured. DragArea and WindowResizeEdgeArea start the "
          "gesture inside the button-press emission hook; a later "
          "method-channel call cannot be honored once that serial is gone.");
    }
    return FALSE;
  }

  gint root_x = self->last_mouse_root_x;
  gint root_y = self->last_mouse_root_y;
  gint live_x = 0;
  gint live_y = 0;
  if (query_pointer_root(window, &live_x, &live_y)) {
    root_x = live_x;
    root_y = live_y;
  }
  *timestamp_out = plan.timestamp;
  *root_x_out = root_x;
  *root_y_out = root_y;
  *button_out =
      self->last_mouse_button != 0 ? static_cast<gint>(self->last_mouse_button)
                                   : 1;
  return TRUE;
}

static FlValue* get_bounds_map(GtkWindow* window) {
  gint x = 0, y = 0, width = 800, height = 600;
  gtk_window_get_position(window, &x, &y);
  gtk_window_get_size(window, &width, &height);

  FlValue* map = fl_value_new_map();
  fl_value_set_string_take(map, "x", fl_value_new_float(x));
  fl_value_set_string_take(map, "y", fl_value_new_float(y));
  fl_value_set_string_take(map, "width", fl_value_new_float(width));
  fl_value_set_string_take(map, "height", fl_value_new_float(height));
  return map;
}

static FlValue* build_window_state_snapshot(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window) {
  FlValue* map = get_bounds_map(window);
  fl_value_set_string_take(
      map, "isVisible",
      fl_value_new_bool(gtk_widget_get_visible(GTK_WIDGET(window))));
  fl_value_set_string_take(map, "isMaximized",
                           fl_value_new_bool(self->is_maximized));
  fl_value_set_string_take(map, "isMinimized",
                           fl_value_new_bool(self->is_minimized));
  fl_value_set_string_take(map, "isFullScreen",
                           fl_value_new_bool(self->is_fullscreen));
  fl_value_set_string_take(map, "isFocused",
                           fl_value_new_bool(gtk_window_is_active(window)));
  fl_value_set_string_take(map, "isAlwaysOnTop",
                           fl_value_new_bool(self->is_always_on_top));
  fl_value_set_string_take(
      map, "isResizable",
      fl_value_new_bool(gtk_window_get_resizable(window)));
  fl_value_set_string_take(map, "isPreventClose",
                           fl_value_new_bool(self->prevent_close));
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  fl_value_set_string_take(
      map, "isPositionAuthoritative",
      fl_value_new_bool(xue_hua_position_is_authoritative(
          display_is_wayland(display) != FALSE)));
  return map;
}

static void emit_window_event(XueHuaNaviteWindowManagerPlugin* self,
                              GtkWindow* window,
                              const gchar* type) {
  if (!self->event_listening || self->event_channel == nullptr) {
    return;
  }
  g_autoptr(FlValue) payload = get_bounds_map(window);
  fl_value_set_string_take(payload, "type", fl_value_new_string(type));
  fl_value_set_string_take(payload, "isMaximized",
                           fl_value_new_bool(self->is_maximized));
  fl_value_set_string_take(payload, "isFullScreen",
                           fl_value_new_bool(self->is_fullscreen));
  fl_value_set_string_take(
      payload, "timestamp",
      fl_value_new_int(g_get_real_time() / 1000));
  fl_event_channel_send(self->event_channel, payload, nullptr, nullptr);
}

FlMethodResponse* xue_hua_navite_window_manager_handle_ensure_initialized() {
  return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

static std::string get_string_arg_or_empty(FlValue* args, const gchar* key) {
  FlValue* v = lookup_map_value(args, key);
  if (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_STRING) {
    const gchar* s = fl_value_get_string(v);
    if (s != nullptr) {
      return std::string(s);
    }
  }
  return std::string();
}

static std::vector<std::string> get_string_list_arg(FlValue* args,
                                                    const gchar* key) {
  std::vector<std::string> out;
  FlValue* v = lookup_map_value(args, key);
  if (v == nullptr || fl_value_get_type(v) != FL_VALUE_TYPE_LIST) {
    return out;
  }
  const size_t len = fl_value_get_length(v);
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    FlValue* item = fl_value_get_list_value(v, i);
    if (item != nullptr && fl_value_get_type(item) == FL_VALUE_TYPE_STRING) {
      const gchar* s = fl_value_get_string(item);
      if (s != nullptr) {
        out.push_back(std::string(s));
      }
    }
  }
  return out;
}

static std::string resolve_linux_autostart_dir(const gchar* override_dir) {
  if (override_dir != nullptr && override_dir[0] != '\0') {
    return std::string(override_dir);
  }
  const gchar* xdg_env = g_getenv("XDG_CONFIG_HOME");
  if (xdg_env != nullptr && xdg_env[0] != '\0' && g_path_is_absolute(xdg_env)) {
    g_autofree gchar* built = g_build_filename(xdg_env, "autostart", nullptr);
    return built != nullptr ? std::string(built) : std::string();
  }
  const gchar* config_dir = g_get_user_config_dir();
  if (config_dir != nullptr && config_dir[0] != '\0') {
    g_autofree gchar* built =
        g_build_filename(config_dir, "autostart", nullptr);
    return built != nullptr ? std::string(built) : std::string();
  }
  return std::string();
}

static std::string read_proc_self_exe() {
  g_autoptr(GError) err = nullptr;
  g_autofree gchar* target = g_file_read_link("/proc/self/exe", &err);
  if (target == nullptr) {
    return std::string();
  }
  return std::string(target);
}

static std::string extract_linux_exe_basename(const std::string& path) {
  if (path.empty()) {
    return std::string("flutter_app");
  }
  g_autofree gchar* base = g_path_get_basename(path.c_str());
  if (base == nullptr || std::strcmp(base, ".") == 0 ||
      std::strcmp(base, "/") == 0) {
    return std::string("flutter_app");
  }
  return std::string(base);
}

struct LinuxResolvedStartupSpec {
  std::string autostart_dir;
  std::string identifier;
  std::string app_name;
  XueHuaLinuxStartupCommand command;
};

static LinuxResolvedStartupSpec resolve_linux_startup_spec(
    FlValue* args,
    const gchar* autostart_dir_override) {
  LinuxResolvedStartupSpec spec;
  spec.autostart_dir = resolve_linux_autostart_dir(autostart_dir_override);

  const std::string configured_exe =
      get_string_arg_or_empty(args, "executablePath");
  const std::vector<std::string> configured_args =
      get_string_list_arg(args, "arguments");
  const gchar* appimage_raw = g_getenv("APPIMAGE");
  const gchar* flatpak_raw = g_getenv("FLATPAK_ID");
  const std::string proc_exe = read_proc_self_exe();

  spec.command = xue_hua_resolve_linux_startup_command(
      configured_exe, configured_args,
      appimage_raw != nullptr ? std::string(appimage_raw) : std::string(),
      flatpak_raw != nullptr ? std::string(flatpak_raw) : std::string(),
      proc_exe);

  std::string raw_id = get_string_arg_or_empty(args, "identifier");
  const std::string raw_app_name = get_string_arg_or_empty(args, "appName");
  if (raw_id.empty()) {
    raw_id = raw_app_name;
  }
  if (raw_id.empty()) {
    const gchar* prgname = g_get_prgname();
    if (prgname != nullptr && prgname[0] != '\0') {
      raw_id = std::string(prgname);
    }
  }
  if (raw_id.empty()) {
    raw_id = extract_linux_exe_basename(
        !spec.command.reported_executable_path.empty()
            ? spec.command.reported_executable_path
            : proc_exe);
  }
  spec.identifier = xue_hua_sanitize_startup_id(raw_id);

  if (!raw_app_name.empty()) {
    spec.app_name = raw_app_name;
  } else {
    const gchar* g_app_name = g_get_application_name();
    if (g_app_name != nullptr && g_app_name[0] != '\0') {
      spec.app_name = std::string(g_app_name);
    } else {
      spec.app_name = spec.identifier;
    }
  }
  return spec;
}

static FlValue* build_linux_launch_at_startup_state_map(
    const LinuxResolvedStartupSpec& spec) {
  FlValue* map = fl_value_new_map();
  if (spec.autostart_dir.empty() || spec.command.program.empty()) {
    fl_value_set_string_take(map, "isSupported", fl_value_new_bool(FALSE));
    fl_value_set_string_take(map, "isEnabled", fl_value_new_bool(FALSE));
    fl_value_set_string_take(map, "status", fl_value_new_string("unsupported"));
    fl_value_set_string_take(map, "backend", fl_value_new_string("none"));
    fl_value_set_string_take(map, "identifier",
                             fl_value_new_string(spec.identifier.c_str()));
    fl_value_set_string_take(map, "executablePath", fl_value_new_null());
    fl_value_set_string_take(map, "canOpenSettings", fl_value_new_bool(FALSE));
    return map;
  }

  const std::string filename = spec.identifier + ".desktop";
  g_autofree gchar* desktop_path = g_build_filename(
      spec.autostart_dir.c_str(), filename.c_str(), nullptr);

  const bool file_exists =
      desktop_path != nullptr &&
      g_file_test(desktop_path, G_FILE_TEST_IS_REGULAR) != FALSE;
  bool valid_group = false;
  bool has_exec = false;
  bool hidden = false;
  bool has_gnome_enabled = false;
  bool gnome_enabled = true;

  if (file_exists) {
    g_autoptr(GKeyFile) key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, desktop_path, G_KEY_FILE_NONE,
                                  nullptr) &&
        g_key_file_has_group(key_file, G_KEY_FILE_DESKTOP_GROUP)) {
      valid_group = true;
      g_autofree gchar* exec_val = g_key_file_get_string(
          key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC,
          nullptr);
      has_exec = (exec_val != nullptr && exec_val[0] != '\0');

      g_autoptr(GError) hidden_err = nullptr;
      const gboolean hidden_val = g_key_file_get_boolean(
          key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_HIDDEN,
          &hidden_err);
      if (hidden_err == nullptr) {
        hidden = (hidden_val != FALSE);
      }

      g_autoptr(GError) gnome_err = nullptr;
      const gboolean gnome_val = g_key_file_get_boolean(
          key_file, G_KEY_FILE_DESKTOP_GROUP, "X-GNOME-Autostart-enabled",
          &gnome_err);
      if (gnome_err == nullptr) {
        has_gnome_enabled = true;
        gnome_enabled = (gnome_val != FALSE);
      }
    }
  }

  const XueHuaLinuxStartupStatus status_enum =
      xue_hua_evaluate_desktop_entry_state(file_exists, valid_group, has_exec,
                                           hidden, has_gnome_enabled,
                                           gnome_enabled);
  const gchar* status_wire = "disabled";
  gboolean is_enabled = FALSE;
  if (status_enum == XUE_HUA_LINUX_STARTUP_ENABLED) {
    status_wire = "enabled";
    is_enabled = TRUE;
  } else if (status_enum == XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER) {
    status_wire = "disabledByUser";
    is_enabled = FALSE;
  }

  fl_value_set_string_take(map, "isSupported", fl_value_new_bool(TRUE));
  fl_value_set_string_take(map, "isEnabled", fl_value_new_bool(is_enabled));
  fl_value_set_string_take(map, "status", fl_value_new_string(status_wire));
  fl_value_set_string_take(map, "backend",
                           fl_value_new_string("linuxXdgAutostart"));
  fl_value_set_string_take(map, "identifier",
                           fl_value_new_string(spec.identifier.c_str()));
  if (spec.command.reported_executable_path.empty()) {
    fl_value_set_string_take(map, "executablePath", fl_value_new_null());
  } else {
    fl_value_set_string_take(
        map, "executablePath",
        fl_value_new_string(spec.command.reported_executable_path.c_str()));
  }
  fl_value_set_string_take(map, "canOpenSettings", fl_value_new_bool(FALSE));
  return map;
}

FlMethodResponse*
xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
    FlValue* args,
    const gchar* autostart_dir_override) {
  const LinuxResolvedStartupSpec spec =
      resolve_linux_startup_spec(args, autostart_dir_override);
  g_autoptr(FlValue) state_map = build_linux_launch_at_startup_state_map(spec);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(state_map));
}

FlMethodResponse* xue_hua_navite_window_manager_handle_set_launch_at_startup(
    FlValue* args,
    const gchar* autostart_dir_override) {
  const LinuxResolvedStartupSpec spec =
      resolve_linux_startup_spec(args, autostart_dir_override);
  if (spec.autostart_dir.empty() || spec.command.program.empty()) {
    return FL_METHOD_RESPONSE(fl_method_error_response_new(
        "UNSUPPORTED",
        "Unable to resolve XDG autostart directory or executable path.",
        nullptr));
  }

  const bool enabled = get_bool_arg(args, "enabled", FALSE) != FALSE;
  const std::string filename = spec.identifier + ".desktop";
  g_autofree gchar* desktop_path = g_build_filename(
      spec.autostart_dir.c_str(), filename.c_str(), nullptr);

  if (enabled) {
    if (g_mkdir_with_parents(spec.autostart_dir.c_str(), 0700) != 0) {
      return FL_METHOD_RESPONSE(fl_method_error_response_new(
          "LAUNCH_AT_STARTUP_FAILED",
          "Failed to create XDG autostart directory.", nullptr));
    }
    const std::string desktop_content = xue_hua_build_autostart_desktop_entry(
        spec.app_name, spec.command.program, spec.command.arguments);
    g_autoptr(GError) write_err = nullptr;
    if (!g_file_set_contents(desktop_path, desktop_content.c_str(),
                             static_cast<gssize>(desktop_content.size()),
                             &write_err)) {
      return FL_METHOD_RESPONSE(fl_method_error_response_new(
          "LAUNCH_AT_STARTUP_FAILED",
          write_err != nullptr ? write_err->message
                               : "Failed to write autostart desktop entry.",
          nullptr));
    }
  } else {
    if (g_file_test(desktop_path, G_FILE_TEST_EXISTS)) {
      if (g_unlink(desktop_path) != 0 &&
          g_file_test(desktop_path, G_FILE_TEST_EXISTS)) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new(
            "LAUNCH_AT_STARTUP_FAILED",
            "Failed to remove autostart desktop entry.", nullptr));
      }
    }
  }

  g_autoptr(FlValue) state_map = build_linux_launch_at_startup_state_map(spec);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(state_map));
}

FlMethodResponse*
xue_hua_navite_window_manager_handle_open_launch_at_startup_settings() {
  g_autoptr(FlValue) result = fl_value_new_bool(FALSE);
  return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

static void xue_hua_navite_window_manager_plugin_handle_method_call(
    XueHuaNaviteWindowManagerPlugin* self,
    FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;
  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  if (strcmp(method, "ensureInitialized") == 0) {
    get_main_window(self);
    response = xue_hua_navite_window_manager_handle_ensure_initialized();
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "getLaunchAtStartupState") == 0) {
    response = xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
        args, nullptr);
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "setLaunchAtStartup") == 0) {
    response =
        xue_hua_navite_window_manager_handle_set_launch_at_startup(args, nullptr);
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "openLaunchAtStartupSettings") == 0) {
    response =
        xue_hua_navite_window_manager_handle_open_launch_at_startup_settings();
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  // Regions are stored on the plugin. The window may not be hooked yet; the
  // next button press hit-tests whatever was registered.
  if (strcmp(method, "setLinuxInteractiveRegion") == 0) {
    int64_t id = 0;
    if (!get_int_arg(args, "id", &id)) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "BAD_ARGS", "setLinuxInteractiveRegion requires an integer id.",
          nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    const bool present = get_bool_arg(args, "present", FALSE) != FALSE;
    XueHuaInteractiveRegion region{};
    region.id = static_cast<int>(id);
    region.kind = XUE_HUA_INTERACTIVE_MOVE;
    if (present) {
      FlValue* kind_val = lookup_map_value(args, "kind");
      const gchar* kind_wire = "move";
      if (kind_val != nullptr &&
          fl_value_get_type(kind_val) == FL_VALUE_TYPE_STRING) {
        kind_wire = fl_value_get_string(kind_val);
      }
      int kind = XUE_HUA_INTERACTIVE_MOVE;
      if (!xue_hua_interactive_kind_from_wire(kind_wire, &kind)) {
        response = FL_METHOD_RESPONSE(fl_method_error_response_new(
            "BAD_ARGS", "Unknown Linux interactive region kind.", nullptr));
        fl_method_call_respond(method_call, response, nullptr);
        return;
      }
      region.kind = kind;
      gdouble x = 0, y = 0, w = 0, h = 0;
      get_double_arg(args, "x", &x);
      get_double_arg(args, "y", &y);
      get_double_arg(args, "width", &w);
      get_double_arg(args, "height", &h);
      region.x = x;
      region.y = y;
      region.width = w;
      region.height = h;
    }
    upsert_interactive_region(self, region, present);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  GtkWindow* window = get_main_window(self);
  if (window == nullptr) {
    response = FL_METHOD_RESPONSE(fl_method_error_response_new(
        "NO_WINDOW", "Unable to resolve main GtkWindow.", nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "configure") == 0) {
    FlValue* bg_val = lookup_map_value(args, "backgroundColor");
    if (bg_val != nullptr && fl_value_get_type(bg_val) == FL_VALUE_TYPE_INT) {
      apply_background_color(self, window, fl_value_get_int(bg_val));
    }

    FlValue* title_val = lookup_map_value(args, "title");
    if (title_val != nullptr &&
        fl_value_get_type(title_val) == FL_VALUE_TYPE_STRING) {
      gtk_window_set_title(window, fl_value_get_string(title_val));
    }

    if (has_key_arg(args, "preventClose")) {
      self->prevent_close = get_bool_arg(args, "preventClose", FALSE);
    }
    if (has_key_arg(args, "alwaysOnTop")) {
      self->is_always_on_top = get_bool_arg(args, "alwaysOnTop", FALSE);
      gtk_window_set_keep_above(window, self->is_always_on_top);
    }
    if (has_key_arg(args, "resizable")) {
      self->is_resizable = get_bool_arg(args, "resizable", TRUE);
      gtk_window_set_resizable(window, self->is_resizable);
    }

    FlValue* style_val = lookup_map_value(args, "titleBarStyle");
    if (style_val != nullptr &&
        fl_value_get_type(style_val) == FL_VALUE_TYPE_STRING) {
      apply_title_bar_style(window, fl_value_get_string(style_val));
    }

    gdouble min_w = 0, min_h = 0, max_w = 0, max_h = 0;
    if (get_double_arg(args, "minWidth", &min_w)) {
      self->min_width = static_cast<gint>(std::lround(min_w));
    }
    if (get_double_arg(args, "minHeight", &min_h)) {
      self->min_height = static_cast<gint>(std::lround(min_h));
    }
    if (get_double_arg(args, "maxWidth", &max_w)) {
      self->max_width = static_cast<gint>(std::lround(max_w));
    }
    if (get_double_arg(args, "maxHeight", &max_h)) {
      self->max_height = static_cast<gint>(std::lround(max_h));
    }
    apply_geometry_hints(self, window);

    gdouble width = 0, height = 0;
    const bool has_size = (get_double_arg(args, "width", &width) &&
                           get_double_arg(args, "height", &height));
    const bool center = get_bool_arg(args, "center", FALSE) != FALSE;
    gdouble x = 0, y = 0;
    const bool has_pos = !center && get_double_arg(args, "x", &x) &&
                         get_double_arg(args, "y", &y);
    const bool has_geometry_request = has_size || center || has_pos;

    const bool async_restore =
        restore_window_for_geometry(self, window, has_geometry_request);

    gint target_w = 0;
    gint target_h = 0;
    if (has_size) {
      const XueHuaClampedSize clamped = clamp_to_stored_limits(
          self, static_cast<gint>(std::lround(width)),
          static_cast<gint>(std::lround(height)));
      target_w = clamped.width;
      target_h = clamped.height;
      gtk_window_set_default_size(window, target_w, target_h);
      gtk_window_resize(window, target_w, target_h);
    }

    const gint target_x = has_pos ? static_cast<gint>(std::lround(x)) : 0;
    const gint target_y = has_pos ? static_cast<gint>(std::lround(y)) : 0;
    if (center) {
      center_window(window, target_w, target_h);
    } else if (has_pos) {
      gtk_window_move(window, target_x, target_y);
    }

    if (async_restore) {
      record_pending_restore_geometry(self, has_size, target_w, target_h,
                                      center, has_pos, target_x, target_y);
    } else if (has_geometry_request) {
      clear_pending_restore_geometry(self);
    }

    if (get_bool_arg(args, "showWhenReady", FALSE)) {
      show_main_window(self, window, true);
    }

    g_autoptr(FlValue) snapshot = build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "markReadyToShow") == 0) {
    show_main_window(self, window, true);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getState") == 0) {
    g_autoptr(FlValue) snapshot = build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "show") == 0) {
    show_main_window(self, window, get_bool_arg(args, "focus", TRUE) != FALSE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "hide") == 0) {
    gtk_widget_hide(GTK_WIDGET(window));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "focus") == 0) {
    gtk_window_present(window);
    g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
    if (view != nullptr) {
      gtk_widget_grab_focus(GTK_WIDGET(view));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "blur") == 0) {
    GdkWindow* gdk_win = gtk_widget_get_window(GTK_WIDGET(window));
    if (gdk_win != nullptr) {
      gdk_window_lower(gdk_win);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "close") == 0) {
    if (xue_hua_should_intercept_close(self->prevent_close != FALSE,
                                       self->is_force_closing != FALSE)) {
      // Do not call gtk_window_close. The embedder's delete-event handler
      // would request an immediate app exit before our signal handler runs.
      emit_window_event(self, window, "closeRequested");
    } else {
      gtk_window_close(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "destroy") == 0) {
    self->is_force_closing = TRUE;
    gtk_widget_destroy(GTK_WIDGET(window));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "maximize") == 0) {
    clear_pending_restore_geometry(self);
    if (self->is_fullscreen) {
      gtk_window_unfullscreen(window);
    }
    gtk_window_maximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "unmaximize") == 0) {
    clear_pending_restore_geometry(self);
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "minimize") == 0) {
    clear_pending_restore_geometry(self);
    gtk_window_iconify(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "restore") == 0) {
    clear_pending_restore_geometry(self);
    gtk_window_unfullscreen(window);
    gtk_window_deiconify(window);
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setFullScreen") == 0) {
    clear_pending_restore_geometry(self);
    if (get_bool_arg(args, "isFullScreen", FALSE)) {
      gtk_window_fullscreen(window);
    } else {
      gtk_window_unfullscreen(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setBounds") == 0) {
    gdouble x = 0, y = 0, w = 0, h = 0;
    const bool has_pos =
        get_double_arg(args, "x", &x) && get_double_arg(args, "y", &y);
    const bool has_size =
        get_double_arg(args, "width", &w) && get_double_arg(args, "height", &h);
    const bool has_geometry_request = has_pos || has_size;
    const bool async_restore =
        restore_window_for_geometry(self, window, has_geometry_request);

    gint target_w = 0;
    gint target_h = 0;
    if (has_size) {
      const XueHuaClampedSize clamped = clamp_to_stored_limits(
          self, static_cast<gint>(std::lround(w)),
          static_cast<gint>(std::lround(h)));
      target_w = clamped.width;
      target_h = clamped.height;
      gtk_window_set_default_size(window, target_w, target_h);
      gtk_window_resize(window, target_w, target_h);
    }
    const gint target_x = has_pos ? static_cast<gint>(std::lround(x)) : 0;
    const gint target_y = has_pos ? static_cast<gint>(std::lround(y)) : 0;
    if (has_pos) {
      gtk_window_move(window, target_x, target_y);
    }
    if (async_restore) {
      record_pending_restore_geometry(self, has_size, target_w, target_h, false,
                                      has_pos, target_x, target_y);
    } else if (has_geometry_request) {
      clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getBounds") == 0) {
    g_autoptr(FlValue) bounds = get_bounds_map(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(bounds));
  } else if (strcmp(method, "setSize") == 0) {
    const bool async_restore = restore_window_for_geometry(self, window, true);
    gdouble w = 800, h = 600;
    get_double_arg(args, "width", &w);
    get_double_arg(args, "height", &h);
    const XueHuaClampedSize clamped = clamp_to_stored_limits(
        self, static_cast<gint>(std::lround(w)),
        static_cast<gint>(std::lround(h)));
    gtk_window_set_default_size(window, clamped.width, clamped.height);
    gtk_window_resize(window, clamped.width, clamped.height);
    if (async_restore) {
      record_pending_restore_geometry(self, true, clamped.width, clamped.height,
                                      false, false, 0, 0);
    } else {
      clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setPosition") == 0) {
    const bool async_restore = restore_window_for_geometry(self, window, true);
    gdouble x = 0, y = 0;
    get_double_arg(args, "x", &x);
    get_double_arg(args, "y", &y);
    const gint target_x = static_cast<gint>(std::lround(x));
    const gint target_y = static_cast<gint>(std::lround(y));
    gtk_window_move(window, target_x, target_y);
    if (async_restore) {
      record_pending_restore_geometry(self, false, 0, 0, false, true, target_x,
                                      target_y);
    } else {
      clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "center") == 0) {
    const bool async_restore = restore_window_for_geometry(self, window, true);
    gdouble w = 0, h = 0;
    gint target_w = 0;
    gint target_h = 0;
    const bool has_size =
        get_double_arg(args, "width", &w) && get_double_arg(args, "height", &h);
    if (has_size) {
      const XueHuaClampedSize clamped = clamp_to_stored_limits(
          self, static_cast<gint>(std::lround(w)),
          static_cast<gint>(std::lround(h)));
      target_w = clamped.width;
      target_h = clamped.height;
    }
    const bool applied = center_window(window, target_w, target_h);
    if (async_restore) {
      record_pending_restore_geometry(self, has_size, target_w, target_h, true,
                                      false, 0, 0);
    } else {
      clear_pending_restore_geometry(self);
    }
    g_autoptr(FlValue) applied_value = fl_value_new_bool(applied);
    response =
        FL_METHOD_RESPONSE(fl_method_success_response_new(applied_value));
  } else if (strcmp(method, "setMinimumSize") == 0) {
    gdouble w = 0, h = 0;
    get_double_arg(args, "width", &w);
    get_double_arg(args, "height", &h);
    self->min_width = static_cast<gint>(std::lround(w));
    self->min_height = static_cast<gint>(std::lround(h));
    apply_geometry_hints(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setMaximumSize") == 0) {
    gdouble w = 0, h = 0;
    get_double_arg(args, "width", &w);
    get_double_arg(args, "height", &h);
    self->max_width = static_cast<gint>(std::lround(w));
    self->max_height = static_cast<gint>(std::lround(h));
    apply_geometry_hints(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setResizable") == 0) {
    self->is_resizable = get_bool_arg(args, "resizable", TRUE);
    gtk_window_set_resizable(window, self->is_resizable);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setAlwaysOnTop") == 0) {
    self->is_always_on_top = get_bool_arg(args, "alwaysOnTop", FALSE);
    gtk_window_set_keep_above(window, self->is_always_on_top);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setPreventClose") == 0) {
    self->prevent_close = get_bool_arg(args, "preventClose", FALSE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setTitle") == 0) {
    FlValue* title_val = lookup_map_value(args, "title");
    if (title_val != nullptr &&
        fl_value_get_type(title_val) == FL_VALUE_TYPE_STRING) {
      gtk_window_set_title(window, fl_value_get_string(title_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setTitleBarStyle") == 0) {
    FlValue* style_val = lookup_map_value(args, "titleBarStyle");
    if (style_val != nullptr &&
        fl_value_get_type(style_val) == FL_VALUE_TYPE_STRING) {
      apply_title_bar_style(window, fl_value_get_string(style_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setBackgroundColor") == 0) {
    FlValue* bg_val = lookup_map_value(args, "backgroundColor");
    if (bg_val != nullptr && fl_value_get_type(bg_val) == FL_VALUE_TYPE_INT) {
      apply_background_color(self, window, fl_value_get_int(bg_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setOpacity") == 0) {
    gdouble opacity = 1.0;
    if (get_double_arg(args, "opacity", &opacity)) {
      gtk_widget_set_opacity(GTK_WIDGET(window), opacity);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "startDragging") == 0) {
    guint32 timestamp = 0;
    gint root_x = 0;
    gint root_y = 0;
    gint button = 1;
    // Wayland DragArea already called begin_move_drag inside the press hook.
    if (!async_drag_already_synced(self, window) &&
        prepare_move_drag(self, window, &timestamp, &root_x, &root_y,
                          &button)) {
      self->move_drag_active = TRUE;
      gtk_window_begin_move_drag(window, button, root_x, root_y, timestamp);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "startResizing") == 0) {
    FlValue* edge_val = lookup_map_value(args, "edge");
    if (edge_val != nullptr &&
        fl_value_get_type(edge_val) == FL_VALUE_TYPE_STRING) {
      const gchar* edge_str = fl_value_get_string(edge_val);
      GdkWindowEdge gdk_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
      if (g_strcmp0(edge_str, "top") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_NORTH;
      } else if (g_strcmp0(edge_str, "bottom") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_SOUTH;
      } else if (g_strcmp0(edge_str, "left") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_WEST;
      } else if (g_strcmp0(edge_str, "right") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_EAST;
      } else if (g_strcmp0(edge_str, "topLeft") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_NORTH_WEST;
      } else if (g_strcmp0(edge_str, "topRight") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_NORTH_EAST;
      } else if (g_strcmp0(edge_str, "bottomLeft") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_SOUTH_WEST;
      } else if (g_strcmp0(edge_str, "bottomRight") == 0) {
        gdk_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
      }
      guint32 timestamp = 0;
      gint root_x = 0;
      gint root_y = 0;
      gint button = 1;
      if (!async_drag_already_synced(self, window) &&
          prepare_move_drag(self, window, &timestamp, &root_x, &root_y,
                            &button)) {
        self->resize_drag_active = TRUE;
        gtk_window_begin_resize_drag(window, gdk_edge, button, root_x, root_y,
                                     timestamp);
      }
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setTrafficLightsOffset") == 0 ||
             strcmp(method, "setTrafficLightsVisible") == 0 ||
             strcmp(method, "setHasShadow") == 0 ||
             strcmp(method, "setSnapLayoutButtonRect") == 0 ||
             strcmp(method, "setTitleBarDragRects") == 0) {
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else {
    response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  }

  fl_method_call_respond(method_call, response, nullptr);
}

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

static void disconnect_view_signals(XueHuaNaviteWindowManagerPlugin* self) {
  if (!self->view_signals_installed) {
    return;
  }
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view != nullptr) {
    if (self->realize_id != 0) {
      g_signal_handler_disconnect(view, self->realize_id);
    }
    if (self->hierarchy_id != 0) {
      g_signal_handler_disconnect(view, self->hierarchy_id);
    }
  }
  self->realize_id = 0;
  self->hierarchy_id = 0;
  self->view_signals_installed = FALSE;
}

static void xue_hua_navite_window_manager_plugin_dispose(GObject* object) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(object);

  if (self->button_press_hook_id != 0 && self->button_press_signal_id != 0) {
    g_signal_remove_emission_hook(self->button_press_signal_id,
                                  self->button_press_hook_id);
    self->button_press_hook_id = 0;
  }
  if (self->interactive_regions != nullptr) {
    g_array_free(self->interactive_regions, TRUE);
    self->interactive_regions = nullptr;
  }
  if (self->synthetic_release_idle_id != 0) {
    g_source_remove(self->synthetic_release_idle_id);
    self->synthetic_release_idle_id = 0;
  }
  disconnect_view_signals(self);
  unhook_window(self);
  g_weak_ref_clear(&self->view);
  g_clear_object(&self->method_channel);
  g_clear_object(&self->event_channel);
  G_OBJECT_CLASS(xue_hua_navite_window_manager_plugin_parent_class)
      ->dispose(object);
}

static void xue_hua_navite_window_manager_plugin_class_init(
    XueHuaNaviteWindowManagerPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = xue_hua_navite_window_manager_plugin_dispose;
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
  watch_view(plugin);

  if (plugin->button_press_hook_id == 0) {
    plugin->button_press_signal_id =
        g_signal_lookup("button-press-event", GTK_TYPE_WIDGET);
    if (plugin->button_press_signal_id != 0) {
      plugin->button_press_hook_id = g_signal_add_emission_hook(
          plugin->button_press_signal_id, 0, on_button_press_emission_hook,
          plugin, nullptr);
    } else {
      g_warning(
          "xue_hua_navite_window_manager: button-press-event signal missing; "
          "Wayland sync drag will not work.");
    }
  }

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

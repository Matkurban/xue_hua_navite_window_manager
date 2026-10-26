#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstring>

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
    self->is_maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    emit_window_event(self, window,
                      self->is_maximized ? "maximized" : "unmaximized");
  }
  if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
    self->is_minimized =
        (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
    emit_window_event(self, window,
                      self->is_minimized ? "minimized" : "restored");
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    self->is_fullscreen =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    emit_window_event(
        self, window,
        self->is_fullscreen ? "enteredFullScreen" : "exitedFullScreen");
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
    self->is_focused =
        (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) != 0;
    emit_window_event(self, window, self->is_focused ? "focused" : "blurred");
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
  if (g_strcmp0(style, "hidden") == 0 || g_strcmp0(style, "immersive") == 0) {
    gtk_window_set_decorated(window, FALSE);
  } else {
    gtk_window_set_decorated(window, TRUE);
  }
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
    const gboolean has_size = get_double_arg(args, "width", &width) &&
                              get_double_arg(args, "height", &height);
    if (has_size) {
      gtk_window_resize(window, static_cast<gint>(std::lround(width)),
                        static_cast<gint>(std::lround(height)));
    }

    if (get_bool_arg(args, "center", FALSE)) {
      center_window(window, has_size ? static_cast<gint>(std::lround(width)) : 0,
                    has_size ? static_cast<gint>(std::lround(height)) : 0);
    } else {
      gdouble x = 0, y = 0;
      if (get_double_arg(args, "x", &x) && get_double_arg(args, "y", &y)) {
        gtk_window_move(window, static_cast<gint>(std::lround(x)),
                        static_cast<gint>(std::lround(y)));
      }
    }

    if (get_bool_arg(args, "showWhenReady", FALSE)) {
      gtk_widget_show_all(GTK_WIDGET(window));
      gtk_window_present(window);
    }

    g_autoptr(FlValue) snapshot = build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "markReadyToShow") == 0) {
    gtk_widget_show_all(GTK_WIDGET(window));
    gtk_window_present(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getState") == 0) {
    g_autoptr(FlValue) snapshot = build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "show") == 0) {
    gtk_widget_show_all(GTK_WIDGET(window));
    if (get_bool_arg(args, "focus", TRUE)) {
      gtk_window_present(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "hide") == 0) {
    gtk_widget_hide(GTK_WIDGET(window));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "focus") == 0) {
    gtk_window_present(window);
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
    gtk_window_maximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "unmaximize") == 0) {
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "minimize") == 0) {
    gtk_window_iconify(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "restore") == 0) {
    gtk_window_deiconify(window);
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setFullScreen") == 0) {
    if (get_bool_arg(args, "isFullScreen", FALSE)) {
      gtk_window_fullscreen(window);
    } else {
      gtk_window_unfullscreen(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setBounds") == 0) {
    gdouble x = 0, y = 0, w = 0, h = 0;
    if (get_double_arg(args, "x", &x) && get_double_arg(args, "y", &y)) {
      gtk_window_move(window, static_cast<gint>(std::lround(x)),
                      static_cast<gint>(std::lround(y)));
    }
    if (get_double_arg(args, "width", &w) &&
        get_double_arg(args, "height", &h)) {
      gtk_window_resize(window, static_cast<gint>(std::lround(w)),
                        static_cast<gint>(std::lround(h)));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getBounds") == 0) {
    g_autoptr(FlValue) bounds = get_bounds_map(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(bounds));
  } else if (strcmp(method, "setSize") == 0) {
    gdouble w = 800, h = 600;
    get_double_arg(args, "width", &w);
    get_double_arg(args, "height", &h);
    gtk_window_resize(window, static_cast<gint>(std::lround(w)),
                      static_cast<gint>(std::lround(h)));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setPosition") == 0) {
    gdouble x = 0, y = 0;
    get_double_arg(args, "x", &x);
    get_double_arg(args, "y", &y);
    gtk_window_move(window, static_cast<gint>(std::lround(x)),
                    static_cast<gint>(std::lround(y)));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "center") == 0) {
    gdouble w = 0, h = 0;
    gint target_w = 0;
    gint target_h = 0;
    if (get_double_arg(args, "width", &w) &&
        get_double_arg(args, "height", &h)) {
      target_w = static_cast<gint>(std::lround(w));
      target_h = static_cast<gint>(std::lround(h));
    }
    const bool applied = center_window(window, target_w, target_h);
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

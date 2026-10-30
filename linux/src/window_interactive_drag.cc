#include "plugin_internal.h"

#include <cmath>

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

void xue_hua_plugin_schedule_synthetic_release(
    XueHuaNaviteWindowManagerPlugin* self) {
  if (self->synthetic_release_idle_id != 0) {
    return;
  }
  if (!self->move_drag_active && !self->resize_drag_active) {
    return;
  }
  self->synthetic_release_idle_id =
      g_idle_add(on_synthetic_release_idle, self);
}

void xue_hua_plugin_cancel_synthetic_release(
    XueHuaNaviteWindowManagerPlugin* self) {
  if (self->synthetic_release_idle_id != 0) {
    g_source_remove(self->synthetic_release_idle_id);
    self->synthetic_release_idle_id = 0;
  }
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
  if (!xue_hua_display_is_wayland(display)) {
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

bool xue_hua_plugin_async_drag_already_synced(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  return xue_hua_display_is_wayland(display) && self->sync_press_time != 0 &&
         self->sync_press_time == self->last_mouse_time;
}

void xue_hua_plugin_upsert_interactive_region(
    XueHuaNaviteWindowManagerPlugin* self,
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
    xue_hua_plugin_try_hook_window(self);
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

void xue_hua_plugin_install_button_press_hook(
    XueHuaNaviteWindowManagerPlugin* self) {
  if (self->button_press_hook_id == 0) {
    self->button_press_signal_id =
        g_signal_lookup("button-press-event", GTK_TYPE_WIDGET);
    if (self->button_press_signal_id != 0) {
      self->button_press_hook_id = g_signal_add_emission_hook(
          self->button_press_signal_id, 0, on_button_press_emission_hook,
          self, nullptr);
    } else {
      g_warning(
          "xue_hua_navite_window_manager: button-press-event signal missing; "
          "Wayland sync drag will not work.");
    }
  }
}

void xue_hua_plugin_remove_button_press_hook(
    XueHuaNaviteWindowManagerPlugin* self) {
  if (self->button_press_hook_id != 0 && self->button_press_signal_id != 0) {
    g_signal_remove_emission_hook(self->button_press_signal_id,
                                  self->button_press_hook_id);
    self->button_press_hook_id = 0;
  }
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

bool xue_hua_plugin_prepare_move_drag(XueHuaNaviteWindowManagerPlugin* self,
                                      GtkWindow* window,
                                      guint32* timestamp_out,
                                      gint* root_x_out,
                                      gint* root_y_out,
                                      gint* button_out) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
  const XueHuaMoveDragPlan plan = xue_hua_plan_move_drag(
      self->has_button_event != FALSE, self->last_mouse_time,
      gtk_get_current_event_time(), xue_hua_display_is_wayland(display) != FALSE);
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
    return false;
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
  return true;
}

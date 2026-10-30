#include "plugin_internal.h"

XueHuaClampedSize xue_hua_plugin_clamp_to_stored_limits(
    XueHuaNaviteWindowManagerPlugin* self,
    int width,
    int height) {
  return xue_hua_clamp_window_size(width, height, self->min_width,
                                   self->min_height, self->max_width,
                                   self->max_height);
}

void xue_hua_plugin_apply_geometry_hints(
    XueHuaNaviteWindowManagerPlugin* self,
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

void xue_hua_plugin_apply_background_color(
    XueHuaNaviteWindowManagerPlugin* self,
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

void xue_hua_plugin_apply_title_bar_style(GtkWindow* window,
                                          const gchar* style) {
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

void xue_hua_plugin_show_main_window(
    XueHuaNaviteWindowManagerPlugin* self,
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

void xue_hua_plugin_clear_pending_restore_geometry(
    XueHuaNaviteWindowManagerPlugin* self) {
  self->pending_restore_geometry = FALSE;
  self->pending_has_size = FALSE;
  self->pending_center = FALSE;
  self->pending_has_pos = FALSE;
}

void xue_hua_plugin_record_pending_restore_geometry(
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
bool xue_hua_plugin_restore_window_for_geometry(
    XueHuaNaviteWindowManagerPlugin* self,
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
      xue_hua_plugin_emit_window_event(self, window, "exitedFullScreen");
    }
  }
  if (plan.unmaximize) {
    gtk_window_unmaximize(window);
    if (self->is_maximized) {
      self->is_maximized = FALSE;
      xue_hua_plugin_emit_window_event(self, window, "unmaximized");
    }
  }
  if (plan.deiconify) {
    gtk_window_deiconify(window);
    if (self->is_minimized) {
      self->is_minimized = FALSE;
      xue_hua_plugin_emit_window_event(self, window, "restored");
    }
  }
  return gtk_widget_get_mapped(GTK_WIDGET(window)) != FALSE;
}

// Returns whether the position change is known to be applied. Resizing still
// runs on every backend; only the move is backend-specific.
bool xue_hua_plugin_center_window(GtkWindow* window, gint width, gint height) {
  if (width > 0 && height > 0) {
    gtk_window_resize(window, width, height);
    gtk_window_set_default_size(window, width, height);
  }

  GtkWidget* widget = GTK_WIDGET(window);
  GdkDisplay* display = gtk_widget_get_display(widget);
  const bool wayland = xue_hua_display_is_wayland(display) != FALSE;
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

FlValue* xue_hua_plugin_get_bounds_map(GtkWindow* window) {
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

FlValue* xue_hua_plugin_build_window_state_snapshot(
    XueHuaNaviteWindowManagerPlugin* self,
    GtkWindow* window) {
  FlValue* map = xue_hua_plugin_get_bounds_map(window);
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
          xue_hua_display_is_wayland(display) != FALSE)));
  return map;
}

#include "plugin_internal.h"

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
    disconnect_signal_handlers_with_data(window, "delete-event",
                                         GTK_TYPE_WIDGET, view);
    disconnect_signal_handlers_with_data(window, "close-request",
                                         GTK_TYPE_WINDOW, view);
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
    xue_hua_plugin_emit_window_event(self, GTK_WINDOW(widget),
                                     "closeRequested");
    return TRUE;
  }
  return FALSE;
}

// GTK 4 renamed delete-event to close-request (no GdkEvent argument). GTK 3
// does not have the signal; it is connected only when g_signal_lookup finds it.
static gboolean on_window_close_request(GtkWindow* window,
                                        gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (xue_hua_should_intercept_close(self->prevent_close != FALSE,
                                     self->is_force_closing != FALSE)) {
    xue_hua_plugin_emit_window_event(self, window, "closeRequested");
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
  xue_hua_plugin_emit_window_event(self, window, "resized");
  xue_hua_plugin_emit_window_event(self, window, "moved");
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
      xue_hua_plugin_emit_window_event(
          self, window, self->is_maximized ? "maximized" : "unmaximized");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
    const gboolean now_minimized =
        (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
    if (self->is_minimized != now_minimized) {
      self->is_minimized = now_minimized;
      xue_hua_plugin_emit_window_event(
          self, window, self->is_minimized ? "minimized" : "restored");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    const gboolean now_fullscreen =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    if (self->is_fullscreen != now_fullscreen) {
      self->is_fullscreen = now_fullscreen;
      xue_hua_plugin_emit_window_event(
          self, window,
          self->is_fullscreen ? "enteredFullScreen" : "exitedFullScreen");
    }
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
    const gboolean now_focused =
        (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) != 0;
    if (self->is_focused != now_focused) {
      self->is_focused = now_focused;
      xue_hua_plugin_emit_window_event(self, window,
                                       self->is_focused ? "focused" : "blurred");
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
      xue_hua_plugin_center_window(
          window, self->pending_has_size ? self->pending_width : 0,
          self->pending_has_size ? self->pending_height : 0);
    } else if (self->pending_has_pos) {
      gtk_window_move(window, self->pending_x, self->pending_y);
    }
  }
  return FALSE;
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
  xue_hua_plugin_schedule_synthetic_release(self);
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

void xue_hua_plugin_unhook_window(XueHuaNaviteWindowManagerPlugin* self) {
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
    xue_hua_plugin_unhook_window(self);
    self->hooked_window = window;
    gtk_widget_add_events(GTK_WIDGET(window),
                          GDK_STRUCTURE_MASK | GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK);
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
    self->destroy_id = g_signal_connect(window, "destroy",
                                        G_CALLBACK(on_window_destroy), self);
  }
  // Realize can attach the embedder handler after our own connect.
  suppress_embedder_close_handlers(self, window);
}

void xue_hua_plugin_try_hook_window(XueHuaNaviteWindowManagerPlugin* self) {
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
  xue_hua_plugin_try_hook_window(
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data));
}

static void on_view_hierarchy_changed(GtkWidget* widget,
                                      GtkWidget* previous_toplevel,
                                      gpointer user_data) {
  (void)widget;
  (void)previous_toplevel;
  xue_hua_plugin_try_hook_window(
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data));
}

void xue_hua_plugin_watch_view(XueHuaNaviteWindowManagerPlugin* self) {
  if (self->view_signals_installed) {
    xue_hua_plugin_try_hook_window(self);
    return;
  }
  g_autoptr(FlView) view = FL_VIEW(g_weak_ref_get(&self->view));
  if (view == nullptr) {
    return;
  }
  // after: FlView's realize handler is what connects the embedder close hook.
  self->realize_id = g_signal_connect_after(
      view, "realize", G_CALLBACK(on_view_realize), self);
  self->hierarchy_id = g_signal_connect(
      view, "hierarchy-changed", G_CALLBACK(on_view_hierarchy_changed), self);
  self->view_signals_installed = TRUE;
  xue_hua_plugin_try_hook_window(self);
}

void xue_hua_plugin_disconnect_view_signals(
    XueHuaNaviteWindowManagerPlugin* self) {
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

GtkWindow* xue_hua_plugin_get_main_window(
    XueHuaNaviteWindowManagerPlugin* self) {
  xue_hua_plugin_try_hook_window(self);
  if (self->hooked_window != nullptr && GTK_IS_WINDOW(self->hooked_window)) {
    return self->hooked_window;
  }
  return nullptr;
}

void xue_hua_plugin_emit_window_event(XueHuaNaviteWindowManagerPlugin* self,
                                      GtkWindow* window,
                                      const gchar* type) {
  if (!self->event_listening || self->event_channel == nullptr) {
    return;
  }
  g_autoptr(FlValue) payload = xue_hua_plugin_get_bounds_map(window);
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

#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cmath>
#include <cstring>

#include "xue_hua_navite_window_manager_plugin_private.h"

#define XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST(                    \
      (obj), xue_hua_navite_window_manager_plugin_get_type(), \
      XueHuaNaviteWindowManagerPlugin))

struct _XueHuaNaviteWindowManagerPlugin {
  GObject parent_instance;

  FlPluginRegistrar* registrar;
  FlMethodChannel* method_channel;
  FlEventChannel* event_channel;
  GtkWindow* hooked_window;

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

  gint last_mouse_root_x;
  gint last_mouse_root_y;
  guint32 last_mouse_button;
  guint32 last_mouse_time;
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

// GTK Signal Callbacks
static gboolean on_window_delete_event(GtkWidget* widget,
                                       GdkEvent* event,
                                       gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  if (!self->is_force_closing && self->prevent_close) {
    emit_window_event(self, GTK_WINDOW(widget), "closeRequested");
    return TRUE;  // Intercept close
  }
  return FALSE;
}

static gboolean on_window_configure_event(GtkWidget* widget,
                                          GdkEventConfigure* event,
                                          gpointer user_data) {
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

static gboolean on_button_press_event(GtkWidget* widget,
                                      GdkEventButton* event,
                                      gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  self->last_mouse_root_x = static_cast<gint>(event->x_root);
  self->last_mouse_root_y = static_cast<gint>(event->y_root);
  self->last_mouse_button = event->button;
  self->last_mouse_time = event->time;
  return FALSE;
}

static GtkWindow* get_main_window(XueHuaNaviteWindowManagerPlugin* self) {
  if (self->hooked_window != nullptr && GTK_IS_WINDOW(self->hooked_window)) {
    return self->hooked_window;
  }
  if (self->registrar == nullptr) {
    return nullptr;
  }
  FlView* view = fl_plugin_registrar_get_view(self->registrar);
  if (view == nullptr) {
    return nullptr;
  }
  GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));
  if (toplevel == nullptr || !GTK_IS_WINDOW(toplevel)) {
    return nullptr;
  }

  GtkWindow* window = GTK_WINDOW(toplevel);
  self->hooked_window = window;

  gtk_widget_add_events(toplevel, GDK_STRUCTURE_MASK | GDK_BUTTON_PRESS_MASK |
                                      GDK_POINTER_MOTION_MASK);
  g_signal_connect(toplevel, "delete-event",
                   G_CALLBACK(on_window_delete_event), self);
  g_signal_connect(toplevel, "configure-event",
                   G_CALLBACK(on_window_configure_event), self);
  g_signal_connect(toplevel, "window-state-event",
                   G_CALLBACK(on_window_state_event), self);
  g_signal_connect(GTK_WIDGET(view), "button-press-event",
                   G_CALLBACK(on_button_press_event), self);

  return window;
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
  const gdouble a = ((argb >> 24) & 0xFF) / 255.0;
  const gdouble r = ((argb >> 16) & 0xFF) / 255.0;
  const gdouble g = ((argb >> 8) & 0xFF) / 255.0;
  const gdouble b = (argb & 0xFF) / 255.0;

  GdkRGBA rgba{r, g, b, a};
  if (self->registrar != nullptr) {
    FlView* view = fl_plugin_registrar_get_view(self->registrar);
    if (view != nullptr) {
      fl_view_set_background_color(view, &rgba);
    }
  }
}

static void apply_title_bar_style(GtkWindow* window, const gchar* style) {
  if (g_strcmp0(style, "hidden") == 0 || g_strcmp0(style, "immersive") == 0) {
    gtk_window_set_decorated(window, FALSE);
  } else {
    gtk_window_set_decorated(window, TRUE);
  }
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
    if (get_double_arg(args, "width", &width) &&
        get_double_arg(args, "height", &height)) {
      gtk_window_resize(window, static_cast<gint>(std::lround(width)),
                        static_cast<gint>(std::lround(height)));
    }

    if (get_bool_arg(args, "center", FALSE)) {
      gtk_window_set_position(window, GTK_WIN_POS_CENTER);
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
    gtk_window_close(window);
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
    if (get_double_arg(args, "width", &w) &&
        get_double_arg(args, "height", &h)) {
      gtk_window_resize(window, static_cast<gint>(std::lround(w)),
                        static_cast<gint>(std::lround(h)));
    }
    gtk_window_set_position(window, GTK_WIN_POS_CENTER);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
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
    gtk_window_begin_move_drag(
        window, self->last_mouse_button ? self->last_mouse_button : 1,
        self->last_mouse_root_x, self->last_mouse_root_y,
        self->last_mouse_time ? self->last_mouse_time : GDK_CURRENT_TIME);
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
      gtk_window_begin_resize_drag(
          window, gdk_edge,
          self->last_mouse_button ? self->last_mouse_button : 1,
          self->last_mouse_root_x, self->last_mouse_root_y,
          self->last_mouse_time ? self->last_mouse_time : GDK_CURRENT_TIME);
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
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  self->event_listening = TRUE;
  return nullptr;
}

static FlMethodErrorResponse* event_cancel_cb(FlEventChannel* channel,
                                              FlValue* args,
                                              gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  self->event_listening = FALSE;
  return nullptr;
}

static void xue_hua_navite_window_manager_plugin_dispose(GObject* object) {
  XueHuaNaviteWindowManagerPlugin* self =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(object);
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
  self->is_resizable = TRUE;
  self->last_mouse_button = 1;
}

static void method_call_cb(FlMethodChannel* channel,
                           FlMethodCall* method_call,
                           gpointer user_data) {
  XueHuaNaviteWindowManagerPlugin* plugin =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(user_data);
  xue_hua_navite_window_manager_plugin_handle_method_call(plugin, method_call);
}

void xue_hua_navite_window_manager_plugin_register_with_registrar(
    FlPluginRegistrar* registrar) {
  XueHuaNaviteWindowManagerPlugin* plugin =
      XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN(g_object_new(
          xue_hua_navite_window_manager_plugin_get_type(), nullptr));

  plugin->registrar = registrar;

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

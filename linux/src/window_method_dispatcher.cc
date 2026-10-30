#include <cmath>
#include <cstring>

#include "fl_value_utils.h"
#include "plugin_internal.h"
#include "window_logic.h"
#include "xue_hua_navite_window_manager_plugin_private.h"

void xue_hua_navite_window_manager_plugin_handle_method_call(
    XueHuaNaviteWindowManagerPlugin* self,
    FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response = nullptr;
  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  if (strcmp(method, "ensureInitialized") == 0) {
    xue_hua_plugin_get_main_window(self);
    response = xue_hua_navite_window_manager_handle_ensure_initialized();
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "getLaunchAtStartupState") == 0) {
    response =
        xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
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
    if (!xue_hua_get_int_arg(args, "id", &id)) {
      response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "BAD_ARGS", "setLinuxInteractiveRegion requires an integer id.",
          nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    const bool present = xue_hua_get_bool_arg(args, "present", FALSE) != FALSE;
    XueHuaInteractiveRegion region{};
    region.id = static_cast<int>(id);
    region.kind = XUE_HUA_INTERACTIVE_MOVE;
    if (present) {
      FlValue* kind_val = xue_hua_lookup_map_value(args, "kind");
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
      xue_hua_get_double_arg(args, "x", &x);
      xue_hua_get_double_arg(args, "y", &y);
      xue_hua_get_double_arg(args, "width", &w);
      xue_hua_get_double_arg(args, "height", &h);
      region.x = x;
      region.y = y;
      region.width = w;
      region.height = h;
    }
    xue_hua_plugin_upsert_interactive_region(self, region, present);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  GtkWindow* window = xue_hua_plugin_get_main_window(self);
  if (window == nullptr) {
    response = FL_METHOD_RESPONSE(fl_method_error_response_new(
        "NO_WINDOW", "Unable to resolve main GtkWindow.", nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "configure") == 0) {
    FlValue* bg_val = xue_hua_lookup_map_value(args, "backgroundColor");
    if (bg_val != nullptr && fl_value_get_type(bg_val) == FL_VALUE_TYPE_INT) {
      xue_hua_plugin_apply_background_color(self, window,
                                            fl_value_get_int(bg_val));
    }

    FlValue* title_val = xue_hua_lookup_map_value(args, "title");
    if (title_val != nullptr &&
        fl_value_get_type(title_val) == FL_VALUE_TYPE_STRING) {
      gtk_window_set_title(window, fl_value_get_string(title_val));
    }

    if (xue_hua_has_key_arg(args, "preventClose")) {
      self->prevent_close = xue_hua_get_bool_arg(args, "preventClose", FALSE);
    }
    if (xue_hua_has_key_arg(args, "alwaysOnTop")) {
      self->is_always_on_top = xue_hua_get_bool_arg(args, "alwaysOnTop", FALSE);
      gtk_window_set_keep_above(window, self->is_always_on_top);
    }
    if (xue_hua_has_key_arg(args, "resizable")) {
      self->is_resizable = xue_hua_get_bool_arg(args, "resizable", TRUE);
      gtk_window_set_resizable(window, self->is_resizable);
    }

    FlValue* style_val = xue_hua_lookup_map_value(args, "titleBarStyle");
    if (style_val != nullptr &&
        fl_value_get_type(style_val) == FL_VALUE_TYPE_STRING) {
      xue_hua_plugin_apply_title_bar_style(window,
                                           fl_value_get_string(style_val));
    }

    gdouble min_w = 0, min_h = 0, max_w = 0, max_h = 0;
    if (xue_hua_get_double_arg(args, "minWidth", &min_w)) {
      self->min_width = static_cast<gint>(std::lround(min_w));
    }
    if (xue_hua_get_double_arg(args, "minHeight", &min_h)) {
      self->min_height = static_cast<gint>(std::lround(min_h));
    }
    if (xue_hua_get_double_arg(args, "maxWidth", &max_w)) {
      self->max_width = static_cast<gint>(std::lround(max_w));
    }
    if (xue_hua_get_double_arg(args, "maxHeight", &max_h)) {
      self->max_height = static_cast<gint>(std::lround(max_h));
    }
    xue_hua_plugin_apply_geometry_hints(self, window);

    gdouble width = 0, height = 0;
    const bool has_size = (xue_hua_get_double_arg(args, "width", &width) &&
                           xue_hua_get_double_arg(args, "height", &height));
    const bool center = xue_hua_get_bool_arg(args, "center", FALSE) != FALSE;
    gdouble x = 0, y = 0;
    const bool has_pos = !center && xue_hua_get_double_arg(args, "x", &x) &&
                         xue_hua_get_double_arg(args, "y", &y);
    const bool has_geometry_request = has_size || center || has_pos;

    const bool async_restore = xue_hua_plugin_restore_window_for_geometry(
        self, window, has_geometry_request);

    gint target_w = 0;
    gint target_h = 0;
    if (has_size) {
      const XueHuaClampedSize clamped = xue_hua_plugin_clamp_to_stored_limits(
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
      xue_hua_plugin_center_window(window, target_w, target_h);
    } else if (has_pos) {
      gtk_window_move(window, target_x, target_y);
    }

    if (async_restore) {
      xue_hua_plugin_record_pending_restore_geometry(
          self, has_size, target_w, target_h, center, has_pos, target_x,
          target_y);
    } else if (has_geometry_request) {
      xue_hua_plugin_clear_pending_restore_geometry(self);
    }

    if (xue_hua_get_bool_arg(args, "showWhenReady", FALSE)) {
      xue_hua_plugin_show_main_window(self, window, true);
    }

    g_autoptr(FlValue) snapshot =
        xue_hua_plugin_build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "markReadyToShow") == 0) {
    xue_hua_plugin_show_main_window(self, window, true);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getState") == 0) {
    g_autoptr(FlValue) snapshot =
        xue_hua_plugin_build_window_state_snapshot(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(snapshot));
  } else if (strcmp(method, "show") == 0) {
    xue_hua_plugin_show_main_window(
        self, window, xue_hua_get_bool_arg(args, "focus", TRUE) != FALSE);
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
      xue_hua_plugin_emit_window_event(self, window, "closeRequested");
    } else {
      gtk_window_close(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "destroy") == 0) {
    self->is_force_closing = TRUE;
    gtk_widget_destroy(GTK_WIDGET(window));
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "maximize") == 0) {
    xue_hua_plugin_clear_pending_restore_geometry(self);
    if (self->is_fullscreen) {
      gtk_window_unfullscreen(window);
    }
    gtk_window_maximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "unmaximize") == 0) {
    xue_hua_plugin_clear_pending_restore_geometry(self);
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "minimize") == 0) {
    xue_hua_plugin_clear_pending_restore_geometry(self);
    gtk_window_iconify(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "restore") == 0) {
    xue_hua_plugin_clear_pending_restore_geometry(self);
    gtk_window_unfullscreen(window);
    gtk_window_deiconify(window);
    gtk_window_unmaximize(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setFullScreen") == 0) {
    xue_hua_plugin_clear_pending_restore_geometry(self);
    if (xue_hua_get_bool_arg(args, "isFullScreen", FALSE)) {
      gtk_window_fullscreen(window);
    } else {
      gtk_window_unfullscreen(window);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setBounds") == 0) {
    gdouble x = 0, y = 0, w = 0, h = 0;
    const bool has_pos =
        xue_hua_get_double_arg(args, "x", &x) &&
        xue_hua_get_double_arg(args, "y", &y);
    const bool has_size =
        xue_hua_get_double_arg(args, "width", &w) &&
        xue_hua_get_double_arg(args, "height", &h);
    const bool has_geometry_request = has_pos || has_size;
    const bool async_restore = xue_hua_plugin_restore_window_for_geometry(
        self, window, has_geometry_request);

    gint target_w = 0;
    gint target_h = 0;
    if (has_size) {
      const XueHuaClampedSize clamped = xue_hua_plugin_clamp_to_stored_limits(
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
      xue_hua_plugin_record_pending_restore_geometry(
          self, has_size, target_w, target_h, false, has_pos, target_x,
          target_y);
    } else if (has_geometry_request) {
      xue_hua_plugin_clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "getBounds") == 0) {
    g_autoptr(FlValue) bounds = xue_hua_plugin_get_bounds_map(window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(bounds));
  } else if (strcmp(method, "setSize") == 0) {
    const bool async_restore =
        xue_hua_plugin_restore_window_for_geometry(self, window, true);
    gdouble w = 800, h = 600;
    xue_hua_get_double_arg(args, "width", &w);
    xue_hua_get_double_arg(args, "height", &h);
    const XueHuaClampedSize clamped = xue_hua_plugin_clamp_to_stored_limits(
        self, static_cast<gint>(std::lround(w)),
        static_cast<gint>(std::lround(h)));
    gtk_window_set_default_size(window, clamped.width, clamped.height);
    gtk_window_resize(window, clamped.width, clamped.height);
    if (async_restore) {
      xue_hua_plugin_record_pending_restore_geometry(
          self, true, clamped.width, clamped.height, false, false, 0, 0);
    } else {
      xue_hua_plugin_clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setPosition") == 0) {
    const bool async_restore =
        xue_hua_plugin_restore_window_for_geometry(self, window, true);
    gdouble x = 0, y = 0;
    xue_hua_get_double_arg(args, "x", &x);
    xue_hua_get_double_arg(args, "y", &y);
    const gint target_x = static_cast<gint>(std::lround(x));
    const gint target_y = static_cast<gint>(std::lround(y));
    gtk_window_move(window, target_x, target_y);
    if (async_restore) {
      xue_hua_plugin_record_pending_restore_geometry(
          self, false, 0, 0, false, true, target_x, target_y);
    } else {
      xue_hua_plugin_clear_pending_restore_geometry(self);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "center") == 0) {
    const bool async_restore =
        xue_hua_plugin_restore_window_for_geometry(self, window, true);
    gdouble w = 0, h = 0;
    gint target_w = 0;
    gint target_h = 0;
    const bool has_size =
        xue_hua_get_double_arg(args, "width", &w) &&
        xue_hua_get_double_arg(args, "height", &h);
    if (has_size) {
      const XueHuaClampedSize clamped = xue_hua_plugin_clamp_to_stored_limits(
          self, static_cast<gint>(std::lround(w)),
          static_cast<gint>(std::lround(h)));
      target_w = clamped.width;
      target_h = clamped.height;
    }
    const bool applied = xue_hua_plugin_center_window(window, target_w, target_h);
    if (async_restore) {
      xue_hua_plugin_record_pending_restore_geometry(
          self, has_size, target_w, target_h, true, false, 0, 0);
    } else {
      xue_hua_plugin_clear_pending_restore_geometry(self);
    }
    g_autoptr(FlValue) applied_value = fl_value_new_bool(applied);
    response =
        FL_METHOD_RESPONSE(fl_method_success_response_new(applied_value));
  } else if (strcmp(method, "setMinimumSize") == 0) {
    gdouble w = 0, h = 0;
    xue_hua_get_double_arg(args, "width", &w);
    xue_hua_get_double_arg(args, "height", &h);
    self->min_width = static_cast<gint>(std::lround(w));
    self->min_height = static_cast<gint>(std::lround(h));
    xue_hua_plugin_apply_geometry_hints(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setMaximumSize") == 0) {
    gdouble w = 0, h = 0;
    xue_hua_get_double_arg(args, "width", &w);
    xue_hua_get_double_arg(args, "height", &h);
    self->max_width = static_cast<gint>(std::lround(w));
    self->max_height = static_cast<gint>(std::lround(h));
    xue_hua_plugin_apply_geometry_hints(self, window);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setResizable") == 0) {
    self->is_resizable = xue_hua_get_bool_arg(args, "resizable", TRUE);
    gtk_window_set_resizable(window, self->is_resizable);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setAlwaysOnTop") == 0) {
    self->is_always_on_top = xue_hua_get_bool_arg(args, "alwaysOnTop", FALSE);
    gtk_window_set_keep_above(window, self->is_always_on_top);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setPreventClose") == 0) {
    self->prevent_close = xue_hua_get_bool_arg(args, "preventClose", FALSE);
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setTitle") == 0) {
    FlValue* title_val = xue_hua_lookup_map_value(args, "title");
    if (title_val != nullptr &&
        fl_value_get_type(title_val) == FL_VALUE_TYPE_STRING) {
      gtk_window_set_title(window, fl_value_get_string(title_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setTitleBarStyle") == 0) {
    FlValue* style_val = xue_hua_lookup_map_value(args, "titleBarStyle");
    if (style_val != nullptr &&
        fl_value_get_type(style_val) == FL_VALUE_TYPE_STRING) {
      xue_hua_plugin_apply_title_bar_style(window,
                                           fl_value_get_string(style_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setBackgroundColor") == 0) {
    FlValue* bg_val = xue_hua_lookup_map_value(args, "backgroundColor");
    if (bg_val != nullptr && fl_value_get_type(bg_val) == FL_VALUE_TYPE_INT) {
      xue_hua_plugin_apply_background_color(self, window,
                                            fl_value_get_int(bg_val));
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "setOpacity") == 0) {
    gdouble opacity = 1.0;
    if (xue_hua_get_double_arg(args, "opacity", &opacity)) {
      gtk_widget_set_opacity(GTK_WIDGET(window), opacity);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "startDragging") == 0) {
    guint32 timestamp = 0;
    gint root_x = 0;
    gint root_y = 0;
    gint button = 1;
    // Wayland DragArea already called begin_move_drag inside the press hook.
    if (!xue_hua_plugin_async_drag_already_synced(self, window) &&
        xue_hua_plugin_prepare_move_drag(self, window, &timestamp, &root_x,
                                         &root_y, &button)) {
      self->move_drag_active = TRUE;
      gtk_window_begin_move_drag(window, button, root_x, root_y, timestamp);
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
  } else if (strcmp(method, "startResizing") == 0) {
    FlValue* edge_val = xue_hua_lookup_map_value(args, "edge");
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
      if (!xue_hua_plugin_async_drag_already_synced(self, window) &&
          xue_hua_plugin_prepare_move_drag(self, window, &timestamp, &root_x,
                                           &root_y, &button)) {
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

#include "xue_hua_window_logic.h"

#include <cstring>

XueHuaCenteredOrigin xue_hua_compute_centered_origin(int area_x,
                                                     int area_y,
                                                     int area_width,
                                                     int area_height,
                                                     int window_width,
                                                     int window_height) {
  XueHuaCenteredOrigin origin;
  origin.x = area_x + (area_width - window_width) / 2;
  origin.y = area_y + (area_height - window_height) / 2;
  return origin;
}

bool xue_hua_should_intercept_close(bool prevent_close, bool force_closing) {
  return prevent_close && !force_closing;
}

XueHuaMoveDragPlan xue_hua_plan_move_drag(bool has_button_event,
                                          std::uint32_t button_event_time,
                                          std::uint32_t current_event_time,
                                          bool wayland) {
  XueHuaMoveDragPlan plan;
  plan.attempt = false;
  plan.timestamp = 0;

  if (has_button_event && button_event_time != 0) {
    plan.attempt = true;
    plan.timestamp = button_event_time;
    return plan;
  }
  if (current_event_time != 0) {
    plan.attempt = true;
    plan.timestamp = current_event_time;
    return plan;
  }
  if (!wayland) {
    plan.attempt = true;
    plan.timestamp = 0;
    return plan;
  }
  return plan;
}

namespace {

double region_area(const XueHuaInteractiveRegion& region) {
  return region.width * region.height;
}

bool is_resize_kind(int kind) {
  return kind >= XUE_HUA_INTERACTIVE_RESIZE_NORTH &&
         kind <= XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST;
}

}  // namespace

bool xue_hua_interactive_kind_from_wire(const char* wire, int* kind_out) {
  if (wire == nullptr || kind_out == nullptr) {
    return false;
  }
  if (std::strcmp(wire, "move") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_MOVE;
    return true;
  }
  if (std::strcmp(wire, "top") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_NORTH;
    return true;
  }
  if (std::strcmp(wire, "bottom") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_SOUTH;
    return true;
  }
  if (std::strcmp(wire, "left") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_WEST;
    return true;
  }
  if (std::strcmp(wire, "right") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_EAST;
    return true;
  }
  if (std::strcmp(wire, "topLeft") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_NORTH_WEST;
    return true;
  }
  if (std::strcmp(wire, "topRight") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_NORTH_EAST;
    return true;
  }
  if (std::strcmp(wire, "bottomLeft") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_SOUTH_WEST;
    return true;
  }
  if (std::strcmp(wire, "bottomRight") == 0) {
    *kind_out = XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST;
    return true;
  }
  return false;
}

double xue_hua_view_local(double event_coord,
                          int event_origin,
                          int view_origin) {
  return event_coord + static_cast<double>(event_origin - view_origin);
}

bool xue_hua_region_contains(const XueHuaInteractiveRegion& region,
                             double x,
                             double y) {
  if (region.width <= 0.0 || region.height <= 0.0) {
    return false;
  }
  return x >= region.x && y >= region.y && x < region.x + region.width &&
         y < region.y + region.height;
}

XueHuaPressPlan xue_hua_plan_interactive_press(
    bool wayland,
    bool primary_button,
    bool double_click,
    double x,
    double y,
    const XueHuaInteractiveRegion* regions,
    int region_count) {
  XueHuaPressPlan plan;
  plan.action = XUE_HUA_PRESS_IGNORE;
  plan.kind = XUE_HUA_INTERACTIVE_MOVE;
  if (!wayland || !primary_button || double_click || regions == nullptr ||
      region_count <= 0) {
    return plan;
  }

  const XueHuaInteractiveRegion* best_resize = nullptr;
  const XueHuaInteractiveRegion* best_move = nullptr;
  for (int i = 0; i < region_count; i++) {
    const XueHuaInteractiveRegion& region = regions[i];
    if (!xue_hua_region_contains(region, x, y)) {
      continue;
    }
    if (region.kind == XUE_HUA_INTERACTIVE_MOVE) {
      if (best_move == nullptr || region_area(region) < region_area(*best_move)) {
        best_move = &region;
      }
    } else if (is_resize_kind(region.kind)) {
      if (best_resize == nullptr ||
          region_area(region) < region_area(*best_resize)) {
        best_resize = &region;
      }
    }
  }

  if (best_resize != nullptr) {
    plan.action = XUE_HUA_PRESS_RESIZE;
    plan.kind = best_resize->kind;
    return plan;
  }
  if (best_move != nullptr) {
    plan.action = XUE_HUA_PRESS_MOVE;
    plan.kind = best_move->kind;
    return plan;
  }
  return plan;
}

XueHuaCenterAction xue_hua_plan_center(bool mapped, bool wayland) {
  if (!mapped) {
    return XUE_HUA_CENTER_MAP_HINT;
  }
  if (wayland) {
    return XUE_HUA_CENTER_WAYLAND_IGNORED;
  }
  return XUE_HUA_CENTER_MOVE;
}

bool xue_hua_position_is_authoritative(bool wayland) {
  return !wayland;
}

bool xue_hua_center_position_applied(XueHuaCenterAction action, bool wayland) {
  if (wayland) {
    return false;
  }
  return action == XUE_HUA_CENTER_MAP_HINT || action == XUE_HUA_CENTER_MOVE;
}

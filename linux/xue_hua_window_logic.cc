#include "xue_hua_window_logic.h"

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

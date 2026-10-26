#ifndef XUE_HUA_WINDOW_LOGIC_H_
#define XUE_HUA_WINDOW_LOGIC_H_

#include <cstdint>

// Pure helpers shared by the Linux plugin and unit tests. They do not touch
// GTK, so they run without a display.

struct XueHuaCenteredOrigin {
  int x;
  int y;
};

// Centers `window_width` x `window_height` inside a monitor work area.
// Integer division matches the Windows work-area centering path.
XueHuaCenteredOrigin xue_hua_compute_centered_origin(int area_x,
                                                     int area_y,
                                                     int area_width,
                                                     int area_height,
                                                     int window_width,
                                                     int window_height);

// True when a close request must be reported to Dart and the window kept.
bool xue_hua_should_intercept_close(bool prevent_close, bool force_closing);

struct XueHuaMoveDragPlan {
  // When false, the caller must not call gtk_window_begin_move_drag.
  bool attempt;
  // Gdk event time. 0 is GDK_CURRENT_TIME, only used on X11 as a fallback.
  std::uint32_t timestamp;
};

// Picks the button-press serial GTK/Wayland need for a programmatic move.
// `button_event_time` / `current_event_time` of 0 means "no real timestamp".
// Wayland compositors reject CurrentTime, so a missing serial skips the drag
// instead of calling begin_move_drag with a timestamp the compositor ignores.
XueHuaMoveDragPlan xue_hua_plan_move_drag(bool has_button_event,
                                          std::uint32_t button_event_time,
                                          std::uint32_t current_event_time,
                                          bool wayland);

#endif  // XUE_HUA_WINDOW_LOGIC_H_

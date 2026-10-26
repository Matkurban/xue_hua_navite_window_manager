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
//
// This plan is the async fallback (method channel after the press). On
// Wayland, GDK itself ignores the timestamp and sends the seat's implicit
// grab serial. Compositors often accept that serial only while still
// dispatching the button press, so DragArea uses
// xue_hua_plan_interactive_press() and calls begin_move_drag inside the
// button-press-event emission hook. X11 keeps this fallback, including
// CurrentTime.
XueHuaMoveDragPlan xue_hua_plan_move_drag(bool has_button_event,
                                          std::uint32_t button_event_time,
                                          std::uint32_t current_event_time,
                                          bool wayland);

// Hit regions registered by DragArea / WindowResizeEdgeArea before the click.
// Coordinates are Flutter logical pixels, which match GDK window coordinates
// on the Linux embedder (the view multiplies by the GTK scale factor and
// Flutter divides by the same devicePixelRatio).
enum XueHuaInteractiveKind {
  XUE_HUA_INTERACTIVE_MOVE = 0,
  XUE_HUA_INTERACTIVE_RESIZE_NORTH = 1,
  XUE_HUA_INTERACTIVE_RESIZE_SOUTH = 2,
  XUE_HUA_INTERACTIVE_RESIZE_WEST = 3,
  XUE_HUA_INTERACTIVE_RESIZE_EAST = 4,
  XUE_HUA_INTERACTIVE_RESIZE_NORTH_WEST = 5,
  XUE_HUA_INTERACTIVE_RESIZE_NORTH_EAST = 6,
  XUE_HUA_INTERACTIVE_RESIZE_SOUTH_WEST = 7,
  XUE_HUA_INTERACTIVE_RESIZE_SOUTH_EAST = 8,
};

struct XueHuaInteractiveRegion {
  int id;
  int kind;
  double x;
  double y;
  double width;
  double height;
};

enum XueHuaPressAction {
  // Let the async startDragging / startResizing path run (X11, or a miss).
  XUE_HUA_PRESS_IGNORE = 0,
  XUE_HUA_PRESS_MOVE = 1,
  XUE_HUA_PRESS_RESIZE = 2,
};

struct XueHuaPressPlan {
  int action;
  int kind;
};

// Parses "move" and ResizeEdge wire names ("top", "bottomRight", ...).
bool xue_hua_interactive_kind_from_wire(const char* wire, int* kind_out);

// View-local GDK coordinate. event_x is relative to the event window.
// Adding the two window origins converts it into the Flutter view.
double xue_hua_view_local(double event_coord, int event_origin, int view_origin);

bool xue_hua_region_contains(const XueHuaInteractiveRegion& region,
                             double x,
                             double y);

// Wayland-only decision for a primary button press already in the press hook.
// X11, non-primary buttons, and double-clicks return IGNORE so the existing
// async path (and Flutter's double-click) stay responsible.
// Overlapping regions: the smaller area wins. A resize region wins over a
// move region so edges are not swallowed by a full-window drag rect.
XueHuaPressPlan xue_hua_plan_interactive_press(
    bool wayland,
    bool primary_button,
    bool double_click,
    double x,
    double y,
    const XueHuaInteractiveRegion* regions,
    int region_count);

enum XueHuaCenterAction {
  // Window is not mapped. GTK_WIN_POS_CENTER is consulted at map time.
  XUE_HUA_CENTER_MAP_HINT = 0,
  // Mapped X11 window. Move onto the monitor work area.
  XUE_HUA_CENTER_MOVE = 1,
  // Mapped Wayland window. xdg-shell has no absolute move. Do not call
  // gtk_window_move (it is ignored and can cache a position the compositor
  // never applied).
  XUE_HUA_CENTER_WAYLAND_IGNORED = 2,
};

XueHuaCenterAction xue_hua_plan_center(bool mapped, bool wayland);

// False on Wayland: clients cannot read a trustworthy global position, and
// gtk_window_get_position may echo a value the compositor never applied.
bool xue_hua_position_is_authoritative(bool wayland);

// True only when this process can rely on the center request being applied.
// Wayland is always false: the map hint is still issued, but the compositor
// may ignore it, and a mapped window cannot be moved at all.
bool xue_hua_center_position_applied(XueHuaCenterAction action, bool wayland);

#endif  // XUE_HUA_WINDOW_LOGIC_H_

#ifndef XUE_HUA_WINDOW_LOGIC_H_
#define XUE_HUA_WINDOW_LOGIC_H_

#include <cstdint>
#include <string>
#include <vector>

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

struct XueHuaClampedSize {
  int width;
  int height;
};

// Clamps a client size to stored limits. A limit <= 0 leaves that edge open.
// GTK geometry hints do not constrain gtk_window_resize on every window
// manager, so setSize / setBounds / configure clamp before requesting a size.
// When both limits are set and max < min, the minimum wins (same as macOS
// configure and the Windows ClampDipAxis helper).
XueHuaClampedSize xue_hua_clamp_window_size(int width,
                                            int height,
                                            int min_width,
                                            int min_height,
                                            int max_width,
                                            int max_height);

struct XueHuaGeometryRestorePlan {
  bool unfullscreen;
  bool unmaximize;
  bool deiconify;
};

// Plans which modal window states must be cleared before applying explicit
// windowed geometry (`configure`, `setBounds`, `setSize`, `setPosition`,
// `center`). On X11 and Wayland, `gtk_window_resize` / `gtk_window_move` do not
// clear `_NET_WM_STATE_MAXIMIZED` / `xdg_toplevel` maximized or fullscreen
// states, causing WMs to ignore the resize or push a maximized surface
// off-screen when centered.
XueHuaGeometryRestorePlan xue_hua_plan_geometry_restore(
    bool has_geometry_request,
    bool is_fullscreen,
    bool is_maximized,
    bool is_minimized);

enum XueHuaLinuxStartupStatus {
  XUE_HUA_LINUX_STARTUP_DISABLED = 0,
  XUE_HUA_LINUX_STARTUP_ENABLED = 1,
  XUE_HUA_LINUX_STARTUP_DISABLED_BY_USER = 2,
};

struct XueHuaLinuxStartupCommand {
  std::string program;
  std::vector<std::string> arguments;
  std::string reported_executable_path;
};

// Produces a safe Freedesktop `.desktop` file stem (without `.desktop`),
// stripping leading dots, whitespace, path separators, and `.desktop` suffix.
std::string xue_hua_sanitize_startup_id(const std::string& raw);

// Sanitizes single-line Desktop Entry `string` / `localestring` values
// (`Name=`), stripping CR/LF/NUL to prevent key injection.
std::string xue_hua_sanitize_desktop_text(const std::string& raw);

// Escapes a single token for a Desktop Entry `Exec=` value (Freedesktop
// Desktop Entry Specification: `%%` field-code escaping + double-quoting and
// `\"`, `` \` ``, `\$`, `\\` escaping when reserved characters are present).
std::string xue_hua_escape_desktop_exec_token(const std::string& token);

// Builds the logical `Exec` command string (`program` + `arguments`) before
// GKeyFile `string` escaping.
std::string xue_hua_build_desktop_exec_line(
    const std::string& program,
    const std::vector<std::string>& arguments);

// Escapes a logical value for raw storage in a Desktop Entry `string` key
// (`\\`, `\n`, `\r`, `\t`, leading `\s`), matching `g_key_file_set_string`.
std::string xue_hua_escape_keyfile_string(const std::string& value);

// Resolves the Linux autostart target command in priority order:
// 1. Explicit `configured_executable_path`
// 2. `$APPIMAGE` (avoids transient `/tmp/.mount_*` FUSE mount paths)
// 3. `flatpak run <$FLATPAK_ID>` when running inside Flatpak
// 4. `/proc/self/exe` symlink target
XueHuaLinuxStartupCommand xue_hua_resolve_linux_startup_command(
    const std::string& configured_executable_path,
    const std::vector<std::string>& configured_arguments,
    const std::string& appimage_env,
    const std::string& flatpak_id_env,
    const std::string& proc_self_exe);

// Generates the complete UTF-8 contents of `~/.config/autostart/<id>.desktop`.
std::string xue_hua_build_autostart_desktop_entry(
    const std::string& app_name,
    const std::string& program,
    const std::vector<std::string>& arguments);

// Evaluates parsed `.desktop` flags into `disabled`, `enabled`, or
// `disabledByUser` (`Hidden=true` or `X-GNOME-Autostart-enabled=false`).
XueHuaLinuxStartupStatus xue_hua_evaluate_desktop_entry_state(
    bool file_exists,
    bool valid_desktop_entry,
    bool has_non_empty_exec,
    bool hidden,
    bool has_gnome_enabled,
    bool gnome_enabled);

#endif  // XUE_HUA_WINDOW_LOGIC_H_

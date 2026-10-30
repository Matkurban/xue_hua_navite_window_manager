#ifndef XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_PRIVATE_H_
#define XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_PRIVATE_H_

#include <flutter_linux/flutter_linux.h>

#include "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin.h"

// Exposes internal helpers for unit tests when no GtkWindow is attached.
FlMethodResponse* xue_hua_navite_window_manager_handle_ensure_initialized();

FlMethodResponse*
xue_hua_navite_window_manager_handle_get_launch_at_startup_state(
    FlValue* args,
    const gchar* autostart_dir_override);

FlMethodResponse* xue_hua_navite_window_manager_handle_set_launch_at_startup(
    FlValue* args,
    const gchar* autostart_dir_override);

FlMethodResponse*
xue_hua_navite_window_manager_handle_open_launch_at_startup_settings();

#endif  // XUE_HUA_NAVITE_WINDOW_MANAGER_PLUGIN_PRIVATE_H_

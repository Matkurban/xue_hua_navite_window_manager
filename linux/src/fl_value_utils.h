#ifndef XUE_HUA_FL_VALUE_UTILS_H_
#define XUE_HUA_FL_VALUE_UTILS_H_

#include <flutter_linux/flutter_linux.h>
#include <gdk/gdk.h>

#include <cstdint>
#include <string>
#include <vector>

FlValue* xue_hua_lookup_map_value(FlValue* args, const gchar* key);

gboolean xue_hua_get_double_arg(FlValue* args,
                                const gchar* key,
                                gdouble* out_val);

gboolean xue_hua_get_bool_arg(FlValue* args,
                              const gchar* key,
                              gboolean default_val);

bool xue_hua_get_int_arg(FlValue* args, const gchar* key, int64_t* out_val);

std::string xue_hua_get_string_arg_or_empty(FlValue* args, const gchar* key);

std::vector<std::string> xue_hua_get_string_list_arg(FlValue* args,
                                                     const gchar* key);

gboolean xue_hua_has_key_arg(FlValue* args, const gchar* key);

gboolean xue_hua_display_is_wayland(GdkDisplay* display);

#endif  // XUE_HUA_FL_VALUE_UTILS_H_

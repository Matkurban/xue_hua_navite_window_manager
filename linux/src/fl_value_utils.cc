#include "fl_value_utils.h"

#include <cmath>
#include <cstring>

FlValue* xue_hua_lookup_map_value(FlValue* args, const gchar* key) {
  if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return nullptr;
  }
  return fl_value_lookup_string(args, key);
}

gboolean xue_hua_get_double_arg(FlValue* args,
                                const gchar* key,
                                gdouble* out_val) {
  FlValue* v = xue_hua_lookup_map_value(args, key);
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

gboolean xue_hua_get_bool_arg(FlValue* args,
                              const gchar* key,
                              gboolean default_val) {
  FlValue* v = xue_hua_lookup_map_value(args, key);
  if (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_BOOL) {
    return fl_value_get_bool(v);
  }
  return default_val;
}

bool xue_hua_get_int_arg(FlValue* args, const gchar* key, int64_t* out_val) {
  FlValue* v = xue_hua_lookup_map_value(args, key);
  if (v == nullptr) {
    return false;
  }
  if (fl_value_get_type(v) == FL_VALUE_TYPE_INT) {
    *out_val = fl_value_get_int(v);
    return true;
  }
  if (fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT) {
    *out_val = static_cast<int64_t>(std::lround(fl_value_get_float(v)));
    return true;
  }
  return false;
}

std::string xue_hua_get_string_arg_or_empty(FlValue* args, const gchar* key) {
  FlValue* v = xue_hua_lookup_map_value(args, key);
  if (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_STRING) {
    const gchar* s = fl_value_get_string(v);
    if (s != nullptr) {
      return std::string(s);
    }
  }
  return std::string();
}

std::vector<std::string> xue_hua_get_string_list_arg(FlValue* args,
                                                     const gchar* key) {
  std::vector<std::string> out;
  FlValue* v = xue_hua_lookup_map_value(args, key);
  if (v == nullptr || fl_value_get_type(v) != FL_VALUE_TYPE_LIST) {
    return out;
  }
  const size_t len = fl_value_get_length(v);
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    FlValue* item = fl_value_get_list_value(v, i);
    if (item != nullptr && fl_value_get_type(item) == FL_VALUE_TYPE_STRING) {
      const gchar* s = fl_value_get_string(item);
      if (s != nullptr) {
        out.push_back(std::string(s));
      }
    }
  }
  return out;
}

gboolean xue_hua_has_key_arg(FlValue* args, const gchar* key) {
  return xue_hua_lookup_map_value(args, key) != nullptr;
}

gboolean xue_hua_display_is_wayland(GdkDisplay* display) {
  if (display == nullptr) {
    return FALSE;
  }
  // "wayland-0" when GTK is on the Wayland backend. An XWayland session
  // reports an X11 display name and can use the X11 drag fallback.
  const gchar* name = gdk_display_get_name(display);
  return name != nullptr && g_str_has_prefix(name, "wayland");
}

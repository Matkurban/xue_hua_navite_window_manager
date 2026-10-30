# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "xue_hua_navite_window_manager")
project(${PROJECT_NAME} LANGUAGES CXX)

# Enforce C++17 standard for modern Win32 / DWM plugin development.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "xue_hua_navite_window_manager_plugin")

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "src/launch_at_startup_logic.h"
  "src/launch_at_startup_windows.cpp"
  "src/method_call_utils.h"
  "src/method_call_utils.cpp"
  "src/window_frame_controller.cpp"
  "src/window_geometry_logic.h"
  "src/window_message_handler.cpp"
  "src/window_method_dispatcher.cpp"
  "src/xue_hua_navite_window_manager_plugin.h"
  "src/xue_hua_navite_window_manager_plugin.cpp"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/xue_hua_navite_window_manager/xue_hua_navite_window_manager_plugin_c_api.h"
  "src/xue_hua_navite_window_manager_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_compile_definitions(${PLUGIN_NAME} PRIVATE UNICODE _UNICODE NOMINMAX)

# Source include directories and library dependencies.
# Links dwmapi (DWM rounded corners & shadows) and comctl32 (SetWindowSubclass).
target_include_directories(${PLUGIN_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PLUGIN_NAME} PRIVATE
  flutter
  flutter_wrapper_plugin
  advapi32
  shell32
  dwmapi
  comctl32
  uxtheme
)

# List of absolute paths to libraries that should be bundled with the plugin.
set(xue_hua_navite_window_manager_bundled_libraries
  ""
  PARENT_SCOPE
)

# === Tests ===
if (${include_${PROJECT_NAME}_tests})
set(TEST_RUNNER "${PROJECT_NAME}_test")
enable_testing()

include(FetchContent)
# release-1.11.0 sets cmake_minimum_required() below 3.5. CMake 4 removed
# compatibility with that, so configuring the example fails. v1.15.2 requires
# CMake 3.13 (Flutter Windows already requires 3.14) and still builds as C++14.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(${TEST_RUNNER}
  test/window_geometry_logic_test.cpp
  test/launch_at_startup_windows_test.cpp
  test/window_plugin_smoke_test.cpp
  ${PLUGIN_SOURCES}
)
apply_standard_settings(${TEST_RUNNER})
target_compile_definitions(${TEST_RUNNER} PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(${TEST_RUNNER} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(${TEST_RUNNER} PRIVATE
  flutter_wrapper_plugin
  advapi32
  shell32
  dwmapi
  comctl32
  uxtheme
  gtest_main
  gmock
)
add_custom_command(TARGET ${TEST_RUNNER} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${FLUTTER_LIBRARY}" $<TARGET_FILE_DIR:${TEST_RUNNER}>
)

include(GoogleTest)
gtest_discover_tests(${TEST_RUNNER})
endif()

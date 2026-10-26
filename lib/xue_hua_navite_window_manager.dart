/// High-performance cross-platform desktop window management plugin for Flutter
/// (macOS, Windows, and Linux).
///
/// Features:
/// - **Zero-Flash First Frame**: Synchronizes native window background color and
///   defers window display until the first Flutter frame is rendered.
/// - **Atomic Window Configuration**: Single-channel `NativeWindowController.configure(WindowConfig)`
///   applies size, min/max constraints, center/position, title bar style, and
///   macOS traffic light offsets in one atomic native pass.
/// - **Deep Custom Frame & Native Feel**:
///   - Windows: Win32 `SetWindowSubclass` with `WM_NCCALCSIZE`, `WM_NCHITTEST`
///     (including `HTMAXBUTTON` for Windows 11 Snap Layouts), `WM_GETMINMAXINFO`,
///     and DWM rounded corners (`DWMWCP_ROUND`) + native shadow.
///   - macOS: Full-size content view, transparent title bar, and persistent
///     traffic lights `(x, y)` offset controller across resize and full-screen transitions.
///   - Linux: GTK3 frameless / header bar configuration, geometry hints, and edge resizing.
library;

export 'src/native_window_controller.dart';
export 'src/widgets/drag_area.dart';
export 'src/window_config.dart';
export 'src/window_enums.dart';
export 'src/window_listener.dart';
export 'xue_hua_navite_window_manager_method_channel.dart';
export 'xue_hua_navite_window_manager_platform_interface.dart';

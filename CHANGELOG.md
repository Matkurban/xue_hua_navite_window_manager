## 1.0.0

* Initial production release of `xue_hua_navite_window_manager`.
* **Zero-Flash First Frame**: Eliminates startup black/white flash on macOS, Windows, and Linux by synchronizing native window backing color and deferring presentation until the first Flutter frame renders.
* **Atomic Window Configuration**: Single-pass `NativeWindowController.configure(WindowConfig)` sets size, min/max constraints, center/position, title bar style, background color, and traffic light offsets without visual jumping or multi-channel overhead.
* **Deep Custom Frame & Native Feel**:
  * Windows: Win32 `SetWindowSubclass` handling `WM_NCCALCSIZE`, `WM_NCHITTEST` (with 6-8px resize borders and `HTMAXBUTTON` support for Windows 11 Snap Layouts), `WM_GETMINMAXINFO`, `WM_ERASEBKGND`, and `DwmSetWindowAttribute` for native DWM shadow and rounded corners (`DWMWCP_ROUND`).
  * macOS: Full-size content view with transparent titlebar, custom `(x, y)` offset controller for traffic light buttons (`closeButton`, `miniaturizeButton`, `zoomButton`) across resize and fullscreen transitions, and Swift Package Manager (SPM) support.
  * Linux: GTK3 header bar and frameless CSD geometry management, edge resizing, and window state lifecycle tracking.
* **High-Performance Dragging, Resizing & Event Stream**: Native `startDragging()`, `startResizing(ResizeEdge)`, `DragArea`, `WindowCaption`, `SnapLayoutButton`, and real-time `EventChannel` state notifications.

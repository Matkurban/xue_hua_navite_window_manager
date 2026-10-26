# xue_hua_navite_window_manager_example

Full-featured desktop studio example demonstrating the `xue_hua_navite_window_manager` plugin across **macOS**, **Windows**, and **Linux**:

- **Zero-Flash First Frame** & atomic single-channel `NativeWindowController.configure(WindowConfig)` initialization
- **Custom Frameless Title Bar** (`WindowCaption`, `DragArea`, `SnapLayoutButton`)
- **Windows 11 Snap Layouts** (`HTMAXBUTTON` hit-test integration) & DWM native shadow / rounded corners
- **macOS Traffic Lights Offset Controller** (`setTrafficLightsOffset`, `setTrafficLightsVisible`)
- **Close Interception** (`preventClose: true` + confirmation dialog + `destroy()`)
- **Real-time Event Stream Telemetry** (`WindowListener` & `EventChannel`)

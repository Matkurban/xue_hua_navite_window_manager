# xue_hua_navite_window_manager_example

Full-featured desktop studio example demonstrating the `xue_hua_navite_window_manager` plugin across **macOS**, **Windows**, and **Linux**:

- **Zero-Flash First Frame** & atomic single-channel `NativeWindowController.configure(WindowConfig)` initialization
- **Custom Frameless Title Bar** (`WindowCaption`, `DragArea`, `SnapLayoutButton`)
- **Windows 11 Snap Layouts** (`HTMAXBUTTON` hit-test integration) & DWM native shadow / rounded corners
- **macOS Traffic Lights Offset Controller** (`setTrafficLightsOffset`, `setTrafficLightsVisible`)
- **Close Interception** (`preventClose: true` + confirmation dialog + `destroy()`)
- **Real-time Event Stream Telemetry** (`WindowListener` & `EventChannel`)

## Tests

`integration_test/` drives this app's real native window through the whole public API. Run it with
`flutter test integration_test -d macos` (or `-d windows`, `-d linux`) from this directory. See the
Testing section of the package README for what is covered, how Linux window managers are handled,
and why there is a single `*_test.dart` file.

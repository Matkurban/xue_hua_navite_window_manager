## Unreleased

## 1.0.5

* **CocoaPods on macOS:** the podspec could not be used. It began with `//` comment lines, which are not Ruby, so CocoaPods rejected it with `Invalid podspec file: syntax error`. Once it parsed, every CocoaPods build also printed `no rule to process file ... PrivacyInfo.xcprivacy`, because the `source_files` glob compiled the privacy manifest as a source file. `source_files` now lists only the Swift sources; the manifest still ships through `resource_bundles`. Swift Package Manager builds were never affected.
* **Zero-warning macOS build:** the example's Debug build no longer prints Xcode's "Run script build phase will be run during every build" warning for the `Flutter Assemble` phase, which Flutter's own template still triggers. The `RunnerTests` bundle now targets macOS 14.0, because Xcode 26's XCTest is built for it and linking at 12.0 printed an `ld` warning. The app and the plugin still target macOS 12.0. `tool/check_macos_native_warnings.sh` fails on any compiler, linker, SwiftPM, CocoaPods, or `xcodebuild` warning and runs in CI.
* **Example Studio:** the atomic preset buttons no longer call `setState` after the widget was disposed.
* **Integration tests:** `example/integration_test` now covers the public API against the real native window on macOS, Windows, and Linux: state and `configure`, geometry, show and hide, maximize, minimize, fullscreen, behavior flags, the intercepted close, the event stream, the widgets, and the example Studio app. The suites run in one app launch through `plugin_integration_test.dart`.
* **Unit tests:** the models, the wire payload of every platform channel call, the controller, and the `DragArea` and `WindowResizeEdgeArea` pointer behavior are covered by `flutter test`.
* **CI:** GitHub Actions runs analyze, unit tests, the macOS warning gate, the macOS XCTest unit tests, and the integration tests on macOS, Windows, and Linux.

## 1.0.4

* **Min/max on programmatic resize:** `setSize`, `setBounds`, and `center(size:)` clamp to the stored minimum and maximum on macOS, Windows, and Linux. AppKit `setFrame` and Win32 `SetWindowPos` ignore those limits, so `setSize(500, 400)` after `setMinimumSize(850, 560)` previously kept 500×400. Linux `configure` now clamps the same way macOS and Windows already did. A non-positive limit leaves that edge open; if both are set and the maximum is below the minimum, the minimum wins.
* **Caption tests on macOS:** `WindowCaption` selects the macOS layout from Flutter's `defaultTargetPlatform` instead of `Platform.isMacOS`. `window_caption_test.dart` forces the Windows caption, so `flutter test` passes on a macOS host. macOS still hides the Windows-style minimize, maximize, and close buttons.
* **Example `hide()` on macOS:** the example `AppDelegate` returns false from `applicationShouldTerminateAfterLastWindowClosed`. `hide()` orders the window out, and the Flutter template's `true` quits the process, so `show()` never runs. Hosts that hide the window need the same override (see the README).

## 1.0.3

* Shorten the package description so it stays within the 60–180 character range pub.dev and search engines display.

## 1.0.2

* **Material import:** `WindowCaption` now imports `package:material_ui/material_ui.dart`. `package:flutter/material.dart` is deprecated on Flutter 3.50 and produced an analyzer warning.
* **Narrow client areas:** `WindowCaption` clips an oversized title and hides or scales the trailing controls instead of painting a RenderFlex overflow when the window is only a couple hundred pixels wide (for example during a minimize animation). The example Studio cards scroll or ellipsize the same way.
* **Windows caption maximize:** a left click on the `WindowCaption` / `SnapLayoutButton` maximize control toggles maximize again. `WM_NCHITTEST` still returns `HTMAXBUTTON` so Windows 11 Snap Layouts can open on hover, but that non-client hit never reaches Flutter's `onTap`, and `WM_NCLBUTTONUP` was not a reliable release signal after the non-client press was swallowed. The plugin now arms the press, captures the mouse, and posts `snapButtonClicked` when the button is released inside the control. `SnapLayoutButton` still performs the default `toggleMaximize()` from that event.

## 1.0.1

* **Wayland drag**: `DragArea` and `WindowResizeEdgeArea` register their layout rects, and the Linux GDK filter calls `gtk_window_begin_move_drag` / `gtk_window_begin_resize_drag` inside the button-press event. GTK3 Wayland ignores the timestamp argument and sends the seat's implicit-grab serial, which compositors generally accept only during that dispatch. X11 still uses the async method-channel path, including the `GDK_CURRENT_TIME` fallback.
* **Wayland center**: an unmapped window still gets `GTK_WIN_POS_CENTER` before the first map (a hint the compositor may ignore). A mapped Wayland window is not moved — `xdg-shell` does not allow client absolute positions — and `center()` returns false instead of reporting a position GTK only cached. `WindowStateSnapshot.isPositionAuthoritative` is false on Wayland.
* A bare `startDragging()` / `startResizing()` call that arrives after the press is still best-effort on Wayland and is skipped when no button serial was captured.

## 1.0.0

* Initial production release of `xue_hua_navite_window_manager`.
* **Zero-Flash First Frame**: Eliminates startup black/white flash on macOS, Windows, and Linux by synchronizing native window backing color and deferring presentation until the first Flutter frame renders.
* **Atomic Window Configuration**: Single-pass `NativeWindowController.configure(WindowConfig)` sets size, min/max constraints, center/position, title bar style, background color, and traffic light offsets without visual jumping or multi-channel overhead.
* **Deep Custom Frame & Native Feel**:
  * Windows: Win32 `SetWindowSubclass` handling `WM_NCCALCSIZE`, `WM_NCHITTEST` (with 6-8px resize borders and `HTMAXBUTTON` support for Windows 11 Snap Layouts), `WM_GETMINMAXINFO`, `WM_ERASEBKGND`, and `DwmSetWindowAttribute` for native DWM shadow and rounded corners (`DWMWCP_ROUND`).
  * macOS: Full-size content view with transparent titlebar, custom `(x, y)` offset controller for traffic light buttons (`closeButton`, `miniaturizeButton`, `zoomButton`) across resize and fullscreen transitions, and Swift Package Manager (SPM) support.
  * Linux: GTK3 header bar and frameless CSD geometry management, edge resizing, and window state lifecycle tracking.
* **High-Performance Dragging, Resizing & Event Stream**: Native `startDragging()`, `startResizing(ResizeEdge)`, `DragArea`, `WindowCaption`, `SnapLayoutButton`, and real-time `EventChannel` state notifications.

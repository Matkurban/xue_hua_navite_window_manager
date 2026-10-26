---
name: xue-hua-navite-window-manager
description: Use when the user is working with xue_hua_navite_window_manager APIs (XueHuaNaviteWindowManager) to implement zero-flash desktop window startup, atomic single-channel window configuration (WindowConfig), custom frameless title bars with Windows 11 DWM rounded corners/shadow and Snap Layouts (HTMAXBUTTON), macOS traffic light buttons (x, y) offset positioning, native window dragging/resizing, and lifecycle event listeners across macOS, Windows, and Linux.
---

# xue-hua-navite-window-manager

Provide idiomatic integration patterns, native Runner setup, platform difference handling, and best practices for the `xue_hua_navite_window_manager` Flutter desktop plugin across macOS, Windows, and Linux.

## When to Use

- Eliminating startup black/white window flashes (`XueHuaNaviteWindowManager.configure`, `WindowConfig.showWhenReady`, `WindowConfig.backgroundColor`)
- Configuring initial window geometry (`size`, `minSize`, `maxSize`, `center`, `position`), title bar style (`TitleBarStyle.normal`, `hidden`, `immersive`), and behavior (`preventClose`, `alwaysOnTop`, `resizable`) in a single atomic call via `XueHuaNaviteWindowManager.configure` or `XueHuaNaviteWindowManager.instance.configureWindow`
- Repositioning or toggling visibility of macOS traffic light buttons (`closeButton`, `miniaturizeButton`, `zoomButton`) via `trafficLightsOffset` and `XueHuaNaviteWindowManager.instance.setTrafficLightsOffset`
- Building custom frameless title bars (`WindowCaption`, `DragArea`, `SnapLayoutButton`) that retain Windows 11 native DWM rounded corners (`DWMWCP_ROUND`), system drop shadow, and Windows 11 Snap Layouts (`HTMAXBUTTON`)
- Intercepting window close requests (`preventClose: true` + `WindowListener.onWindowCloseRequested`) to display unsaved-changes confirmation dialogs or hide to the system tray
- Initiating OS-level window dragging (`XueHuaNaviteWindowManager.instance.startDragging()`) and 8-direction edge resizing (`XueHuaNaviteWindowManager.instance.startResizing(ResizeEdge)`, `WindowResizeEdgeArea`)

## Guidelines

1. **Always prefer atomic `XueHuaNaviteWindowManager.configure(WindowConfig)` during startup** over calling `setSize()`, `setMinimumSize()`, `center()`, and `setTitleBarStyle()` separately. Calling individual methods sequentially causes multiple platform channel round-trips and visible window jumping.
2. **Synchronize `backgroundColor` with your Flutter Scaffold background** in `WindowConfig(backgroundColor: ...)` so native `WM_ERASEBKGND` (Windows), `NSWindow.backgroundColor` + `CALayer` (macOS), and `fl_view_set_background_color` (Linux) match your app theme during launch and fast edge resizing.
3. **Use `SnapLayoutButton` or `WindowCaption` on Windows 11** when `titleBarStyle` is `TitleBarStyle.hidden` or `TitleBarStyle.immersive` so the custom maximize/restore button registers its bounding box with native `WM_NCHITTEST` (`HTMAXBUTTON`) and triggers the native Windows 11 Snap Layouts flyout.
4. **Always pair `preventClose: true` with `XueHuaNaviteWindowManager.instance.destroy()`**: When `preventClose` is enabled, `close()` and the OS close button emit `onWindowCloseRequested()`. Call `destroy()` (not `close()`) when the user confirms exiting.
5. **Remove `WindowListener` in `dispose()`**: Always call `XueHuaNaviteWindowManager.instance.removeListener(this)` when disposing a stateful widget that mixes in `WindowListener`.
6. **Linux Wayland drag and center**: Use `DragArea` / `WindowResizeEdgeArea` (they register hit rects so the GDK filter can call `gtk_window_begin_move_drag` during the button press). Do not expect `center()` or `setPosition()` to move an already-mapped Wayland window. `center()` returns false in that case, and `WindowStateSnapshot.isPositionAuthoritative` is false. X11 keeps the async drag path and real centering.

## Examples

### 1. Zero-Flash Atomic Initialization in `main.dart`

```dart
import 'package:flutter/material.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await XueHuaNaviteWindowManager.configure(
    const WindowConfig(
      size: Size(1280, 820),
      minSize: Size(860, 580),
      center: true,
      title: 'My Desktop App',
      titleBarStyle: TitleBarStyle.hidden,
      trafficLightsOffset: Offset(16, 14),
      backgroundColor: Color(0xFF141622),
      preventClose: true,
      showWhenReady: true,
    ),
  );

  runApp(const MyApp());
}
```

### 2. Custom Title Bar (`WindowCaption` / `DragArea` / `SnapLayoutButton`) and Close Interception

```dart
class MainShell extends StatefulWidget {
  const MainShell({super.key});

  @override
  State<MainShell> createState() => _MainShellState();
}

class _MainShellState extends State<MainShell> with WindowListener {
  @override
  void initState() {
    super.initState();
    XueHuaNaviteWindowManager.instance.addListener(this);
  }

  @override
  void dispose() {
    XueHuaNaviteWindowManager.instance.removeListener(this);
    super.dispose();
  }

  @override
  void onWindowCloseRequested() async {
    final bool? confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Exit Application?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Exit'),
          ),
        ],
      ),
    );
    if (confirm == true) {
      await XueHuaNaviteWindowManager.instance.destroy();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF141622),
      body: Column(
        children: [
          const WindowCaption(
            height: 42,
            macosTrafficLightsReservedWidth: 84,
            title: Text('My Desktop App'),
          ),
          const Expanded(
            child: Center(child: Text('Workspace Content')),
          ),
        ],
      ),
    );
  }
}
```

## Gotchas

- **macOS (`NSWindow`)**: For zero native frame flash before Flutter initializes, call `XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow(self, backgroundColor: ...)` inside `MainFlutterWindow.awakeFromNib()`. Custom `trafficLightsOffset` is automatically suspended while the window is in native macOS full-screen mode and restored upon exiting full-screen.
- **Windows (`Win32` / `DWM`)**: In `windows/runner/flutter_window.cpp`, remove the default `flutter_controller_->engine()->SetNextFrameCallback([&]() { this->Show(); });` if you want `XueHuaNaviteWindowManager.configure(..., showWhenReady: true)` to have exclusive control over when the window is first shown after applying geometry and DWM attributes.
- **Linux (`GTK3`)**: In `linux/runner/my_application.cc`, avoid calling `gtk_widget_show(GTK_WIDGET(window))` before `XueHuaNaviteWindowManager.configure` runs so the window is revealed atomically at its target size and position.

# xue_hua_navite_window_manager

English | [简体中文](./README_ZH.md)

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/Matkurban/xue_hua_navite_window_manager)
[![Flutter](https://img.shields.io/badge/Flutter-%3E%3D3.47.0-02569B.svg)](https://flutter.dev)
[![Dart](https://img.shields.io/badge/Dart-%5E3.13.0-0175C2.svg)](https://dart.dev)
[![Platforms](https://img.shields.io/badge/Platforms-macOS%20%7C%20Windows%20%7C%20Linux-success.svg)](https://github.com/Matkurban/xue_hua_navite_window_manager)

A production-grade, high-performance cross-platform desktop window management plugin for Flutter (**macOS**, **Windows**, and **Linux**).

Designed from the ground up to solve the core architectural limitations of legacy desktop window plugins:

1. **Zero-Flash First Frame (`Zero-Flash Startup & Anti-Tearing Resize`)**: Synchronizes native window background brushes (`WM_ERASEBKGND` on Windows, `NSWindow.backgroundColor` + `CALayer` on macOS, `fl_view_set_background_color` on Linux) and keeps the window concealed until the first Flutter frame is rasterized and all geometry is applied.
2. **Atomic Batch Configuration (`XueHuaNaviteWindowManager.configure`)**: Consolidates `size`, `minSize`, `maxSize`, `center`/`position`, `title`, `titleBarStyle`, `trafficLightsOffset`, `backgroundColor`, `alwaysOnTop`, `resizable`, and `preventClose` into a **single** native platform call—eliminating multi-channel overhead and visible window jumping on launch.
3. **Deep Custom Frameless Window with Native OS Feel**:
   - **Windows (`C++17` / `Win32 Subclassing` / `DWM`)**: Uses `SetWindowSubclass` to intercept `WM_NCCALCSIZE` (removes standard title bar while keeping native border styles), `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)` + `DwmExtendFrameIntoClientArea` (preserves Windows 11 native rounded corners and drop shadow), `WM_NCHITTEST` (provides DPI-aware 8-direction resize borders and registers custom Dart widgets as `HTMAXBUTTON` to trigger **Windows 11 Snap Layouts**), and `WM_GETMINMAXINFO` (enforces smooth min/max constraints in the native message loop).
   - **macOS (`Swift 5.9+` / `AppKit` / `SPM`)**: Native Swift Package Manager (`Package.swift`) support, `.fullSizeContentView` + transparent title bar, and a persistent **Traffic Lights Offset Controller** that pins `closeButton`, `miniaturizeButton`, and `zoomButton` to custom `(x, y)` coordinates across live resizing and full-screen transitions.
   - **Linux (`C` / `GTK3`)**: Frameless CSD decoration toggling, `GdkGeometry` min/max size hints, and native `gtk_window_begin_move_drag` / `gtk_window_begin_resize_drag`.

---

## Features Matrix

| Feature | macOS (`Swift 5.9+` / `AppKit`) | Windows (`C++17` / `Win32` & `DWM`) | Linux (`C` / `GTK3`) |
| :--- | :---: | :---: | :---: |
| Zero-Flash Startup & Resize Background Sync | ✅ | ✅ | ✅ |
| Atomic Single-Call `configure(WindowConfig)` | ✅ | ✅ | ✅ |
| Custom Frameless Title Bar (`hidden` / `immersive`) | ✅ | ✅ | ✅ |
| Native Drop Shadow & Rounded Corners (`DWMWCP_ROUND`) | ✅ | ✅ | ✅ (WM dependent) |
| Windows 11 Snap Layouts (`HTMAXBUTTON` Hit-Test) | N/A | ✅ | N/A |
| Custom Traffic Lights `(x, y)` Offset & Visibility | ✅ | N/A | N/A |
| Message-Loop `minSize` / `maxSize` Enforcement | ✅ | ✅ (`WM_GETMINMAXINFO`) | ✅ (`GdkGeometry`) |
| Close Interception (`preventClose` + `destroy()`) | ✅ (`windowShouldClose`) | ✅ (`WM_CLOSE`) | ✅ (`delete-event`) |
| Native Dragging (`startDragging` / `DragArea`) | ✅ | ✅ | ✅ |
| 8-Direction Native Edge Resizing (`startResizing`) | ✅ | ✅ | ✅ |

---

## Installation

Add `xue_hua_navite_window_manager` to your `pubspec.yaml`:

```yaml
dependencies:
  xue_hua_navite_window_manager: ^1.0.0
```

### AI Agent Skills Support

This package ships with an official AI Agent Skill following the [Dart Package Skills Specification](https://dart.dev/tools/pub/package-skills). Consumers can install the skill into their project with:

```bash
dart run skills@ get
```

---

## Native Runner Setup (Recommended for Zero-Flash Startup)

To ensure the operating system does not briefly flash an unstyled native window frame before `main()` runs, apply the following one-line adjustments in your host Runner projects:

### 1. macOS (`macos/Runner/MainFlutterWindow.swift`)

Call `XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow` before `super.awakeFromNib()`:

```swift
import Cocoa
import FlutterMacOS
import xue_hua_navite_window_manager

class MainFlutterWindow: NSWindow {
  override func awakeFromNib() {
    // Conceals the window (alphaValue = 0) and sets the backing color before initial display.
    XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow(
      self,
      backgroundColor: NSColor(srgbRed: 0.08, green: 0.09, blue: 0.13, alpha: 1.0)
    )

    let flutterViewController = FlutterViewController()
    let windowFrame = self.frame
    self.contentViewController = flutterViewController
    self.setFrame(windowFrame, display: true)

    RegisterGeneratedPlugins(registry: flutterViewController)

    super.awakeFromNib()
  }
}
```

### 2. Windows (`windows/runner/flutter_window.cpp`)

Remove the default `this->Show()` inside `SetNextFrameCallback` so `XueHuaNaviteWindowManager.configure(..., showWhenReady: true)` reveals the window atomically after applying size, centering, and DWM attributes:

```cpp
RegisterPlugins(flutter_controller_->engine());
SetChildContent(flutter_controller_->view()->GetNativeWindow());

// XueHuaNaviteWindowManager.configure(WindowConfig(..., showWhenReady: true))
// will reveal and focus the window once the first frame and geometry are ready.
flutter_controller_->ForceRedraw();
```

### 3. Linux (`linux/runner/my_application.cc`)

Set the initial `FlView` background color and let `XueHuaNaviteWindowManager.configure` present the top-level `GtkWindow`:

```c
FlView* view = fl_view_new(project);
GdkRGBA background_color;
gdk_rgba_parse(&background_color, "#141622");
fl_view_set_background_color(view, &background_color);
gtk_widget_show(GTK_WIDGET(view));
gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

gtk_widget_realize(GTK_WIDGET(view));
fl_register_plugins(FL_PLUGIN_REGISTRY(view));
gtk_widget_grab_focus(GTK_WIDGET(view));
```

---

## Quick Start & Usage

### 1. Atomic Window Initialization (`main.dart`)

```dart
import 'package:flutter/material.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Single-channel atomic configuration via XueHuaNaviteWindowManager:
  await XueHuaNaviteWindowManager.configure(
    const WindowConfig(
      size: Size(1240, 820),
      minSize: Size(860, 580),
      maxSize: Size(2560, 1600),
      center: true,
      title: 'XueHua Desktop Studio',
      titleBarStyle: TitleBarStyle.hidden,
      trafficLightsOffset: Offset(16, 14),
      trafficLightsVisible: true,
      backgroundColor: Color(0xFF141622),
      alwaysOnTop: false,
      resizable: true,
      preventClose: true,
      hasShadow: true,
      resizeBorderWidth: 6.0,
      showWhenReady: true,
    ),
  );

  runApp(const MyApp());
}
```

### 2. Custom Title Bar (`WindowCaption`, `DragArea`, and `SnapLayoutButton`)

Use `WindowCaption` for an out-of-the-box adaptive title bar (which reserves space for Traffic Lights on macOS and renders Minimize, Windows 11 Snap-Layout-enabled Maximize/Restore, and Close buttons on Windows/Linux):

```dart
Scaffold(
  backgroundColor: const Color(0xFF141622),
  body: Column(
    children: [
      WindowCaption(
        height: 44,
        macosTrafficLightsReservedWidth: 84,
        backgroundColor: const Color(0xFF1A1D2D),
        title: const Text('XueHua Desktop Studio'),
        actions: [
          IconButton(
            icon: const Icon(Icons.fullscreen, size: 18),
            onPressed: () => XueHuaNaviteWindowManager.instance.setFullScreen(true),
          ),
        ],
      ),
      const Expanded(
        child: Center(child: Text('Application Content')),
      ),
    ],
  ),
)
```

Or build your own custom header using `DragArea` and `SnapLayoutButton`:

```dart
Row(
  children: [
    const Expanded(
      child: DragArea(
        doubleClickToMaximize: true,
        child: SizedBox(
          height: 40,
          child: Align(
            alignment: Alignment.centerLeft,
            child: Text('Custom Draggable Header'),
          ),
        ),
      ),
    ),
    // On Windows 11, hovering over SnapLayoutButton triggers the native OS Snap Layouts menu!
    SnapLayoutButton(
      builder: (context, isHovered, isMaximized) {
        return Container(
          width: 46,
          height: 40,
          color: isHovered ? Colors.white12 : Colors.transparent,
          child: Icon(
            isMaximized ? Icons.filter_none : Icons.crop_square,
            size: 16,
          ),
        );
      },
    ),
  ],
)
```

### 3. Listening to Window Events & Intercepting Window Close

```dart
class WorkspacePage extends StatefulWidget {
  const WorkspacePage({super.key});

  @override
  State<WorkspacePage> createState() => _WorkspacePageState();
}

class _WorkspacePageState extends State<WorkspacePage> with WindowListener {
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
  void onWindowResize(Size size) {
    debugPrint('Window resized: $size');
  }

  @override
  void onWindowCloseRequested() async {
    // Triggered when preventClose is true and the user clicks Close / Alt+F4 / Cmd+W.
    final bool? shouldExit = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Confirm Exit'),
        content: const Text('Are you sure you want to close the window?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Exit'),
          ),
        ],
      ),
    );

    if (shouldExit == true) {
      // Call destroy() to bypass preventClose and close immediately.
      await XueHuaNaviteWindowManager.instance.destroy();
    }
  }

  @override
  Widget build(BuildContext context) => const SizedBox.expand();
}
```

### 4. Dynamic macOS Traffic Lights Positioning

```dart
// Move macOS close/minimize/zoom buttons to (x: 20, y: 18) from top-left:
await XueHuaNaviteWindowManager.instance.setTrafficLightsOffset(const Offset(20, 18));

// Hide or show macOS traffic light buttons dynamically:
await XueHuaNaviteWindowManager.instance.setTrafficLightsVisible(false);
```

---

## Important Notes & Best Practices

1. **Always Match `backgroundColor` to Your App Theme**: Passing `backgroundColor` in `WindowConfig` updates the native OS window surface and CoreAnimation/Win32 brush. This prevents black or white triangles/bars from appearing along window edges during fast user resize drags.
2. **Coordinate System (`DIP`)**: All sizes (`Size`), positions (`Offset`), and rectangles (`Rect`) across the Dart API use logical **Device-Independent Pixels (DIP)** with a **top-left screen origin `(0, 0)`**:
   - On **Windows**, the plugin automatically scales between logical DIP and physical device pixels via `GetDpiForWindow(hwnd)`.
   - On **macOS**, the plugin automatically converts between top-left screen coordinates and AppKit's bottom-left screen origin.
3. **`close()` vs `destroy()`**:
   - `close()` simulates a standard user close request (respecting `preventClose: true` and firing `onWindowCloseRequested`).
   - `destroy()` forcefully closes the native window immediately, bypassing `preventClose`.
4. **Windows 11 Snap Layouts (`HTMAXBUTTON`)**: When `SnapLayoutButton` is mounted on Windows, it automatically tracks its own global widget bounds across resizes and reports them to the native `WM_NCHITTEST` handler. Hovering over the button displays the native Windows 11 Snap Layouts flyout while allowing Flutter to render custom hover states via `onSnapButtonHovered`.

---

## Author & License

- **Author**: Matkurban (`3496354336@qq.com`)
- **Repository**: [https://github.com/Matkurban/xue_hua_navite_window_manager](https://github.com/Matkurban/xue_hua_navite_window_manager)
- **License**: [MIT License](./LICENSE)

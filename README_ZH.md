# xue_hua_navite_window_manager

[English](./README.md) | 简体中文

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/Matkurban/xue_hua_navite_window_manager)
[![Flutter](https://img.shields.io/badge/Flutter-%3E%3D3.47.0-02569B.svg)](https://flutter.dev)
[![Dart](https://img.shields.io/badge/Dart-%5E3.13.0-0175C2.svg)](https://dart.dev)
[![Platforms](https://img.shields.io/badge/Platforms-macOS%20%7C%20Windows%20%7C%20Linux-success.svg)](https://github.com/Matkurban/xue_hua_navite_window_manager)

一款专为 Flutter 桌面端（**macOS**、**Windows**、**Linux**）打造的高性能、生产级原生窗口管理插件。

针对现有桌面端窗口管理插件的三大核心痛点进行了底层重构：

1. **零黑屏启动与防拉伸黑边（Zero-Flash First Frame）**：在原生层同步填充窗口底色（Windows 拦截 `WM_ERASEBKGND` 画刷、macOS 同步 `NSWindow.backgroundColor` 与 `FlutterView` 的 `CALayer` 背景色、Linux 同步 `fl_view_set_background_color`），并在 Flutter 首帧渲染完成且窗口尺寸/位置计算完毕后才原子化显示窗口，彻底消除启动闪烁与快速拖拽边缘时的黑边撕裂。
2. **原子化批量配置（`XueHuaNaviteWindowManager.configure`）**：提供 `XueHuaNaviteWindowManager.configure(WindowConfig)`（或 `XueHuaNaviteWindowManager.instance.configureWindow(WindowConfig)`），通过**单次** MethodChannel 通信将 `size`、`minSize`、`maxSize`、`center`/`position`、`title`、`titleBarStyle`、`trafficLightsOffset`、`backgroundColor`、`alwaysOnTop`、`resizable`、`preventClose` 一次性传给原生层并在内存中完成坐标计算，彻底杜绝初始化时的多次重绘与窗口跳动。
3. **深度无边框与原生系统特性保留（Custom Frame & Native Feel）**：
   - **Windows 端（C++17 / Win32 Subclassing / DWM）**：通过 `SetWindowSubclass` 挂载自定义窗口过程，拦截 `WM_NCCALCSIZE` 移除系统标题栏；调用 `DwmExtendFrameIntoClientArea` 与 `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)` 完整保留 **Windows 11 原生圆角与系统阴影**；拦截 `WM_NCHITTEST` 实现 6~8px DPI 自适应八向缩放热区，并支持将 Dart 层的最大化按钮注册为 `HTMAXBUTTON`，完美触发 **Windows 11 Snap Layouts（悬停分屏菜单）**；拦截 `WM_GETMINMAXINFO` 在原生消息循环层强制约束最小/最大尺寸，丝滑无抖动。
   - **macOS 端（Swift 5.9+ / AppKit / SPM）**：原生支持 **Swift Package Manager (`Package.swift`)**，支持 `.fullSizeContentView` 沉浸式无边框标题栏，内置**红绿灯按钮（Traffic Lights）偏移控制器**，支持动态调整 `(x, y)` 偏移量并在窗口缩放、最小化恢复、全屏进出时自动保持位置不乱跳。
   - **Linux 端（C / GTK3）**：支持无边框修饰切换、`GdkGeometry` 最小/最大尺寸约束，以及原生级窗口拖拽与八向边缘拉伸。

---

## 跨平台特性一览

| 特性 | macOS (`Swift 5.9+` / `AppKit`) | Windows (`C++17` / `Win32` & `DWM`) | Linux (`C` / `GTK3`) |
| :--- | :---: | :---: | :---: |
| 零黑屏启动 & 拉伸防黑边底色同步 | ✅ | ✅ | ✅ |
| 单次通信原子化配置 `configure(WindowConfig)` | ✅ | ✅ | ✅ |
| 隐藏/沉浸式自定义标题栏 (`hidden` / `immersive`) | ✅ | ✅ | ✅ |
| 原生 DWM 阴影与 Windows 11 圆角 (`DWMWCP_ROUND`) | ✅ | ✅ | ✅ (取决于桌面环境) |
| Windows 11 Snap Layouts 悬停分屏 (`HTMAXBUTTON`) | N/A | ✅ | N/A |
| macOS 红绿灯按钮 `(x, y)` 坐标偏移与显隐控制 | ✅ | N/A | N/A |
| 原生消息循环级 `minSize` / `maxSize` 约束 | ✅ | ✅ (`WM_GETMINMAXINFO`) | ✅ (`GdkGeometry`) |
| 关闭窗口拦截 (`preventClose` + `destroy()`) | ✅ (`windowShouldClose`) | ✅ (`WM_CLOSE`) | ✅ (`delete-event`) |
| 原生高性能窗口拖拽 (`startDragging` / `DragArea`) | ✅ | ✅ | ✅ |
| 原生八向边缘缩放 (`startResizing`) | ✅ | ✅ | ✅ |

---

## 安装与集成

在项目的 `pubspec.yaml` 中添加依赖：

```yaml
dependencies:
  xue_hua_navite_window_manager: ^1.0.0
```

### AI Agent Skills 支持

本插件遵循 [Dart 官方 Package Skills 规范](https://dart.dev/tools/pub/package-skills) 内置了 `skills/xue-hua-navite-window-manager` 技能包。开发者可通过以下命令一键安装至开发工具中：

```bash
dart run skills@ get
```

---

## 宿主工程（Runner）零黑屏最佳实践配置

为了确保操作系统在启动初期（Dart `main()` 尚未执行前）绝不闪现默认的空白系统窗口，建议在各平台宿主工程中进行以下配置：

### 1. macOS 端 (`macos/Runner/MainFlutterWindow.swift`)

在 `awakeFromNib()` 中调用 `XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow` 预先隐藏窗口并同步背景色：

```swift
import Cocoa
import FlutterMacOS
import xue_hua_navite_window_manager

class MainFlutterWindow: NSWindow {
  override func awakeFromNib() {
    // 在首帧绘制前将 alphaValue 设为 0 并同步背景色，彻底杜绝启动残影
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

### 2. Windows 端 (`windows/runner/flutter_window.cpp`)

移除默认模板中的 `SetNextFrameCallback([&]() { this->Show(); })`，交由 `XueHuaNaviteWindowManager.configure(..., showWhenReady: true)` 在完成尺寸、居中与 DWM 样式设置后统一原子化调用 `ShowWindow`：

```cpp
RegisterPlugins(flutter_controller_->engine());
SetChildContent(flutter_controller_->view()->GetNativeWindow());

// 窗口显示由 XueHuaNaviteWindowManager.configure(WindowConfig(..., showWhenReady: true))
// 在首帧渲染完成且窗口位置/样式全部就绪后一次性触发。
flutter_controller_->ForceRedraw();
```

### 3. Linux 端 (`linux/runner/my_application.cc`)

预设 `FlView` 背景色，并由 `XueHuaNaviteWindowManager.configure` 控制顶层 `GtkWindow` 的首次显示：

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

## 快速上手与核心用法

### 1. 原子化窗口初始化 (`main.dart`)

```dart
import 'package:flutter/material.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // 单次通信完成大小、最小/最大约束、居中、无边框样式、红绿灯偏移与底色配置：
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

### 2. 自定义无边框标题栏 (`WindowCaption`、`DragArea` 与 `SnapLayoutButton`)

直接使用开箱即用的跨平台 `WindowCaption` 组件（在 macOS 上自动预留红绿灯区域，在 Windows/Linux 上提供最小化、支持 Windows 11 Snap Layouts 的最大化/还原按钮与关闭按钮）：

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
        child: Center(child: Text('主工作区内容')),
      ),
    ],
  ),
)
```

也可以使用 `DragArea` 与 `SnapLayoutButton` 完全自定义标题栏：

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
            child: Text('按住此处拖拽窗口（双击最大化/还原）'),
          ),
        ),
      ),
    ),
    // 在 Windows 11 上将鼠标悬停在此组件上即可自动唤起系统原生 Snap Layouts 分屏菜单！
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

### 3. 监听窗口状态变化与拦截关闭事件 (`WindowListener`)

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
    debugPrint('窗口尺寸变化: $size');
  }

  @override
  void onWindowCloseRequested() async {
    // 当开启 preventClose: true 时，点击关闭按钮或按 Alt+F4 / Cmd+W 会触发此回调
    final bool? shouldExit = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('确认退出'),
        content: const Text('当前有未保存的工作，确定要关闭窗口吗？'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('强制退出'),
          ),
        ],
      ),
    );

    if (shouldExit == true) {
      // 调用 destroy() 绕过拦截立即销毁窗口
      await XueHuaNaviteWindowManager.instance.destroy();
    }
  }

  @override
  Widget build(BuildContext context) => const SizedBox.expand();
}
```

### 4. 动态调整 macOS 红绿灯按钮位置与显隐

```dart
// 动态将 macOS 左上角红绿灯按钮调整至距离左上角 (x: 20, y: 18) 逻辑像素处：
await XueHuaNaviteWindowManager.instance.setTrafficLightsOffset(const Offset(20, 18));

// 动态隐藏或显示红绿灯按钮：
await XueHuaNaviteWindowManager.instance.setTrafficLightsVisible(false);
```

---

## 注意事项与排坑指南

1. **务必保持 `backgroundColor` 与 Flutter 主题背景色一致**：在 `WindowConfig` 中配置 `backgroundColor` 后，原生层会将窗口画刷与渲染层背景色同步为该颜色，从而彻底解决用户快速拖拽边缘改变窗口大小时产生的黑色撕裂块。
2. **统一的逻辑像素坐标系 (`DIP`)**：Dart 层所有 API（`size`、`position`、`bounds`、`minSize`、`maxSize`）均采用以屏幕左上角为 `(0, 0)` 原点的逻辑像素（DIP）：
   - **Windows 端**：插件内部通过 `GetDpiForWindow(hwnd)` 自动完成逻辑像素与物理像素之间的换算。
   - **macOS 端**：插件内部自动完成左上角屏幕坐标系与 AppKit 左下角原点坐标系之间的转换。
3. **`close()` 与 `destroy()` 的区别**：
   - `close()` 会触发标准关闭流程（若开启了 `preventClose: true`，则会拦截关闭并向 Dart 派发 `onWindowCloseRequested`）。
   - `destroy()` 会绕过拦截直接销毁原生窗口，适合在用户点击确认退出对话框后调用。
4. **Windows 11 Snap Layouts 注意事项**：当页面中使用 `SnapLayoutButton`（或包含它的 `WindowCaption`）时，组件会自动计算自身在窗口内的全局坐标并同步给原生层 `WM_NCHITTEST`。当窗口处于无边框模式（`TitleBarStyle.hidden` 或 `immersive`）时，悬停在最大化按钮上即可无缝唤出 Windows 11 原生分屏布局菜单。

---

## 作者与开源协议

- **作者**：Matkurban (`3496354336@qq.com`)
- **仓库地址**：[https://github.com/Matkurban/xue_hua_navite_window_manager](https://github.com/Matkurban/xue_hua_navite_window_manager)
- **开源协议**：[MIT License](./LICENSE)

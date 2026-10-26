import 'package:flutter/widgets.dart';

import '../linux_interactive_region.dart';
import '../xue_hua_navite_window_manager_controller.dart';

/// A widget that turns its child into a native window dragging region and
/// optionally toggles maximize/unmaximize on double-click.
///
/// On pointer down (primary mouse button), [DragArea] invokes
/// `XueHuaNaviteWindowManager.instance.startDragging()` which delegates directly to
/// the operating system (`window.performDrag(with:)` on macOS,
/// `ReleaseCapture() + SendMessage(WM_NCLBUTTONDOWN, HTCAPTION)` on Windows, and
/// `gtk_window_begin_move_drag` on Linux).
///
/// On Linux the widget also publishes its layout rect. Wayland compositors
/// only accept an interactive move while the button-press serial is current,
/// so the native button-press hook calls `gtk_window_begin_move_drag`
/// synchronously when the press lands in that rect. X11 still uses the async
/// `startDragging` path, including the `GDK_CURRENT_TIME` fallback.
/// Double-click maximize stays in Flutter on every backend.
class DragArea extends StatefulWidget {
  /// Child widget rendered inside the draggable region.
  final Widget child;

  /// Whether double-clicking the drag area toggles window maximization.
  final bool doubleClickToMaximize;

  /// Optional callback invoked when a drag gesture begins.
  final VoidCallback? onDragStart;

  /// Creates a [DragArea].
  const DragArea({
    super.key,
    required this.child,
    this.doubleClickToMaximize = true,
    this.onDragStart,
  });

  @override
  State<DragArea> createState() => _DragAreaState();
}

class _DragAreaState extends State<DragArea> {
  late final LinuxInteractiveRegionBinding _region;

  @override
  void initState() {
    super.initState();
    _region = LinuxInteractiveRegionBinding(
      XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId(),
    );
  }

  @override
  void dispose() {
    _region.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    _region.schedule(context, kind: 'move');
    return GestureDetector(
      behavior: HitTestBehavior.translucent,
      onPanStart: (DragStartDetails details) {
        widget.onDragStart?.call();
        XueHuaNaviteWindowManager.instance.startDragging();
      },
      onDoubleTap: widget.doubleClickToMaximize
          ? () {
              XueHuaNaviteWindowManager.instance.toggleMaximize();
            }
          : null,
      child: widget.child,
    );
  }
}

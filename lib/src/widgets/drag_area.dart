import 'package:flutter/widgets.dart';

import '../xue_hua_navite_window_manager_controller.dart';

/// A widget that turns its child into a native window dragging region and
/// optionally toggles maximize/unmaximize on double-click.
///
/// On pointer down (primary mouse button), [DragArea] invokes
/// `XueHuaNaviteWindowManager.instance.startDragging()` which delegates directly to
/// the operating system (`window.performDrag(with:)` on macOS,
/// `ReleaseCapture() + SendMessage(WM_NCLBUTTONDOWN, HTCAPTION)` on Windows, and
/// `gtk_window_begin_move_drag` on Linux).
class DragArea extends StatelessWidget {
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
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.translucent,
      onPanStart: (DragStartDetails details) {
        onDragStart?.call();
        XueHuaNaviteWindowManager.instance.startDragging();
      },
      onDoubleTap: doubleClickToMaximize
          ? () {
              XueHuaNaviteWindowManager.instance.toggleMaximize();
            }
          : null,
      child: child,
    );
  }
}

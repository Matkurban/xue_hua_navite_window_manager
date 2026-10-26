import 'package:flutter/gestures.dart';
import 'package:flutter/widgets.dart';

import '../linux_interactive_region.dart';
import '../window_enums.dart';
import '../xue_hua_navite_window_manager_controller.dart';

/// Optional Flutter-level invisible resize border overlay for environments where
/// custom edge resizing is desired directly from the widget tree.
class WindowResizeEdgeArea extends StatelessWidget {
  /// Child widget wrapped by the 8-direction resize hit zones.
  final Widget child;

  /// Thickness of the edge hit zone in logical pixels. Defaults to `6.0`.
  final double edgeThickness;

  /// Whether resizing is enabled.
  final bool enabled;

  /// Creates a [WindowResizeEdgeArea].
  const WindowResizeEdgeArea({
    super.key,
    required this.child,
    this.edgeThickness = 6.0,
    this.enabled = true,
  });

  @override
  Widget build(BuildContext context) {
    if (!enabled) return child;
    final double corner = edgeThickness * 2;

    return Stack(
      fit: StackFit.expand,
      children: <Widget>[
        child,
        // Top edge
        Positioned(
          top: 0,
          left: corner,
          right: corner,
          height: edgeThickness,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.top,
            cursor: SystemMouseCursors.resizeUpDown,
          ),
        ),
        // Bottom edge
        Positioned(
          bottom: 0,
          left: corner,
          right: corner,
          height: edgeThickness,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.bottom,
            cursor: SystemMouseCursors.resizeUpDown,
          ),
        ),
        // Left edge
        Positioned(
          top: corner,
          bottom: corner,
          left: 0,
          width: edgeThickness,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.left,
            cursor: SystemMouseCursors.resizeLeftRight,
          ),
        ),
        // Right edge
        Positioned(
          top: corner,
          bottom: corner,
          right: 0,
          width: edgeThickness,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.right,
            cursor: SystemMouseCursors.resizeLeftRight,
          ),
        ),
        // Top-Left corner
        Positioned(
          top: 0,
          left: 0,
          width: corner,
          height: corner,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.topLeft,
            cursor: SystemMouseCursors.resizeUpLeftDownRight,
          ),
        ),
        // Top-Right corner
        Positioned(
          top: 0,
          right: 0,
          width: corner,
          height: corner,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.topRight,
            cursor: SystemMouseCursors.resizeUpRightDownLeft,
          ),
        ),
        // Bottom-Left corner
        Positioned(
          bottom: 0,
          left: 0,
          width: corner,
          height: corner,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.bottomLeft,
            cursor: SystemMouseCursors.resizeUpRightDownLeft,
          ),
        ),
        // Bottom-Right corner
        Positioned(
          bottom: 0,
          right: 0,
          width: corner,
          height: corner,
          child: const _ResizeEdgeHitTarget(
            edge: ResizeEdge.bottomRight,
            cursor: SystemMouseCursors.resizeUpLeftDownRight,
          ),
        ),
      ],
    );
  }
}

class _ResizeEdgeHitTarget extends StatefulWidget {
  final ResizeEdge edge;
  final MouseCursor cursor;

  const _ResizeEdgeHitTarget({required this.edge, required this.cursor});

  @override
  State<_ResizeEdgeHitTarget> createState() => _ResizeEdgeHitTargetState();
}

class _ResizeEdgeHitTargetState extends State<_ResizeEdgeHitTarget> {
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
    // Wayland begins the resize from this rect inside the button-press filter.
    // X11 ignores the rect and keeps the async startResizing path below.
    _region.schedule(context, kind: widget.edge.wireValue);
    return MouseRegion(
      cursor: widget.cursor,
      child: Listener(
        behavior: HitTestBehavior.translucent,
        onPointerDown: (PointerDownEvent event) {
          if (event.buttons == kPrimaryMouseButton) {
            XueHuaNaviteWindowManager.instance.startResizing(widget.edge);
          }
        },
        child: const SizedBox.expand(),
      ),
    );
  }
}

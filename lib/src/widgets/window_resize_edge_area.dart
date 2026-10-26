import 'package:flutter/gestures.dart';
import 'package:flutter/widgets.dart';

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
          child: _buildEdge(ResizeEdge.top, SystemMouseCursors.resizeUpDown),
        ),
        // Bottom edge
        Positioned(
          bottom: 0,
          left: corner,
          right: corner,
          height: edgeThickness,
          child: _buildEdge(ResizeEdge.bottom, SystemMouseCursors.resizeUpDown),
        ),
        // Left edge
        Positioned(
          top: corner,
          bottom: corner,
          left: 0,
          width: edgeThickness,
          child: _buildEdge(
            ResizeEdge.left,
            SystemMouseCursors.resizeLeftRight,
          ),
        ),
        // Right edge
        Positioned(
          top: corner,
          bottom: corner,
          right: 0,
          width: edgeThickness,
          child: _buildEdge(
            ResizeEdge.right,
            SystemMouseCursors.resizeLeftRight,
          ),
        ),
        // Top-Left corner
        Positioned(
          top: 0,
          left: 0,
          width: corner,
          height: corner,
          child: _buildEdge(
            ResizeEdge.topLeft,
            SystemMouseCursors.resizeUpLeftDownRight,
          ),
        ),
        // Top-Right corner
        Positioned(
          top: 0,
          right: 0,
          width: corner,
          height: corner,
          child: _buildEdge(
            ResizeEdge.topRight,
            SystemMouseCursors.resizeUpRightDownLeft,
          ),
        ),
        // Bottom-Left corner
        Positioned(
          bottom: 0,
          left: 0,
          width: corner,
          height: corner,
          child: _buildEdge(
            ResizeEdge.bottomLeft,
            SystemMouseCursors.resizeUpRightDownLeft,
          ),
        ),
        // Bottom-Right corner
        Positioned(
          bottom: 0,
          right: 0,
          width: corner,
          height: corner,
          child: _buildEdge(
            ResizeEdge.bottomRight,
            SystemMouseCursors.resizeUpLeftDownRight,
          ),
        ),
      ],
    );
  }

  Widget _buildEdge(ResizeEdge edge, MouseCursor cursor) {
    return MouseRegion(
      cursor: cursor,
      child: Listener(
        behavior: HitTestBehavior.translucent,
        onPointerDown: (PointerDownEvent event) {
          if (event.buttons == kPrimaryMouseButton) {
            XueHuaNaviteWindowManager.instance.startResizing(edge);
          }
        },
        child: const SizedBox.expand(),
      ),
    );
  }
}

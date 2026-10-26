import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import 'native_window_controller.dart';
import 'window_config.dart';
import 'window_enums.dart';
import 'window_listener.dart';

/// A widget that turns its child into a native window dragging region and
/// optionally toggles maximize/unmaximize on double-click.
///
/// On pointer down (primary mouse button), [DragArea] invokes
/// `NativeWindowController.instance.startDragging()` which delegates directly to
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
        NativeWindowController.instance.startDragging();
      },
      onDoubleTap: doubleClickToMaximize
          ? () {
              NativeWindowController.instance.toggleMaximize();
            }
          : null,
      child: child,
    );
  }
}

/// Builder signature for rendering custom maximize/restore button visuals that
/// respond to native Windows 11 `HTMAXBUTTON` hover states.
typedef SnapLayoutButtonBuilder = Widget Function(
  BuildContext context,
  bool isHovered,
  bool isMaximized,
);

/// A wrapper widget for a custom Maximize/Restore button that registers its
/// bounding box with the Windows native `WM_NCHITTEST` handler as `HTMAXBUTTON`.
///
/// On Windows 11, hovering over this widget automatically displays the native OS
/// Snap Layouts flyout menu without breaking Flutter's custom button styling.
class SnapLayoutButton extends StatefulWidget {
  /// Builder that constructs the button visual given `(context, isHovered, isMaximized)`.
  final SnapLayoutButtonBuilder builder;

  /// Optional override callback when clicked. Defaults to `NativeWindowController.instance.toggleMaximize()`.
  final VoidCallback? onPressed;

  /// Whether Snap Layout `HTMAXBUTTON` registration is active. Defaults to `true`.
  final bool enabled;

  /// Creates a [SnapLayoutButton].
  const SnapLayoutButton({
    super.key,
    required this.builder,
    this.onPressed,
    this.enabled = true,
  });

  @override
  State<SnapLayoutButton> createState() => _SnapLayoutButtonState();
}

class _SnapLayoutButtonState extends State<SnapLayoutButton>
    with WindowListener, WidgetsBindingObserver {
  final GlobalKey _buttonKey = GlobalKey();
  bool _isHovered = false;
  bool _isMaximized = false;
  Rect? _lastReportedRect;

  bool get _isWindows => !kIsWeb && Platform.isWindows;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    NativeWindowController.instance.addListener(this);
    _syncInitialState();
    _scheduleRectUpdate();
  }

  Future<void> _syncInitialState() async {
    final WindowStateSnapshot? cached =
        NativeWindowController.instance.lastSnapshot;
    if (cached != null && mounted) {
      setState(() => _isMaximized = cached.isMaximized);
    }
    try {
      final WindowStateSnapshot fresh = await NativeWindowController.instance
          .getState();
      if (mounted) {
        setState(() => _isMaximized = fresh.isMaximized);
      }
    } catch (_) {
      // Ignore during test or non-desktop environments.
    }
  }

  @override
  void didUpdateWidget(covariant SnapLayoutButton oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.enabled != widget.enabled) {
      if (!widget.enabled && _isWindows) {
        NativeWindowController.instance.setSnapLayoutButtonRect(null);
        _lastReportedRect = null;
      } else {
        _scheduleRectUpdate();
      }
    } else {
      _scheduleRectUpdate();
    }
  }

  @override
  void didChangeMetrics() {
    _scheduleRectUpdate();
  }

  void _scheduleRectUpdate() {
    if (!_isWindows || !widget.enabled) return;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted || !widget.enabled) return;
      final RenderObject? renderObject = _buttonKey.currentContext
          ?.findRenderObject();
      if (renderObject is RenderBox && renderObject.hasSize) {
        final Offset topLeft = renderObject.localToGlobal(Offset.zero);
        final Rect rect = topLeft & renderObject.size;
        if (_lastReportedRect != rect) {
          _lastReportedRect = rect;
          NativeWindowController.instance.setSnapLayoutButtonRect(rect);
        }
      }
    });
  }

  @override
  void onWindowResize(Size size) {
    _scheduleRectUpdate();
  }

  @override
  void onWindowMaximize() {
    if (mounted) {
      setState(() => _isMaximized = true);
      _scheduleRectUpdate();
    }
  }

  @override
  void onWindowUnmaximize() {
    if (mounted) {
      setState(() => _isMaximized = false);
      _scheduleRectUpdate();
    }
  }

  @override
  void onSnapButtonHovered(bool isHovered) {
    if (mounted && _isHovered != isHovered) {
      setState(() => _isHovered = isHovered);
    }
  }

  @override
  void onSnapButtonClicked() {
    if (!mounted) return;
    if (widget.onPressed != null) {
      widget.onPressed!();
    } else {
      NativeWindowController.instance.toggleMaximize();
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    NativeWindowController.instance.removeListener(this);
    if (_isWindows && _lastReportedRect != null) {
      NativeWindowController.instance.setSnapLayoutButtonRect(null);
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    _scheduleRectUpdate();
    return MouseRegion(
      key: _buttonKey,
      onEnter: (_) {
        if (!_isHovered) setState(() => _isHovered = true);
      },
      onExit: (_) {
        if (_isHovered) setState(() => _isHovered = false);
      },
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: () {
          if (widget.onPressed != null) {
            widget.onPressed!();
          } else {
            NativeWindowController.instance.toggleMaximize();
          }
        },
        child: widget.builder(context, _isHovered, _isMaximized),
      ),
    );
  }
}

/// A ready-to-use cross-platform custom title bar widget (`WindowCaption`)
/// that adapts to macOS (reserving space for Traffic Lights) and Windows/Linux
/// (providing Minimize, Snap-Layout-enabled Maximize/Restore, and Close buttons).
class WindowCaption extends StatelessWidget implements PreferredSizeWidget {
  /// Optional leading widget (placed after traffic lights on macOS).
  final Widget? leading;

  /// Title widget displayed in the caption bar.
  final Widget? title;

  /// Additional action widgets placed before the window control buttons.
  final List<Widget> actions;

  /// Height of the custom title bar. Defaults to `40.0`.
  final double height;

  /// Left inset reserved for macOS traffic light buttons. Defaults to `78.0`.
  final double macosTrafficLightsReservedWidth;

  /// Background color of the title bar.
  final Color? backgroundColor;

  /// Foreground icon/text color of the title bar.
  final Color? foregroundColor;

  /// Whether to show the standard Minimize / Maximize / Close buttons on Windows and Linux.
  final bool showWindowControls;

  /// Creates a [WindowCaption].
  const WindowCaption({
    super.key,
    this.leading,
    this.title,
    this.actions = const <Widget>[],
    this.height = 40.0,
    this.macosTrafficLightsReservedWidth = 78.0,
    this.backgroundColor,
    this.foregroundColor,
    this.showWindowControls = true,
  });

  @override
  Size get preferredSize => Size.fromHeight(height);

  @override
  Widget build(BuildContext context) {
    final bool isMacOS = !kIsWeb && Platform.isMacOS;
    final ThemeData theme = Theme.of(context);
    final Color fgColor = foregroundColor ?? theme.colorScheme.onSurface;

    return Container(
      height: height,
      color: backgroundColor ?? Colors.transparent,
      child: Row(
        children: <Widget>[
          if (isMacOS) SizedBox(width: macosTrafficLightsReservedWidth),
          if (leading != null)
            Padding(padding: const EdgeInsets.only(left: 8.0), child: leading!),
          Expanded(
            child: DragArea(
              child: Container(
                height: height,
                alignment: Alignment.centerLeft,
                padding: const EdgeInsets.symmetric(horizontal: 12.0),
                child: DefaultTextStyle(
                  style:
                      theme.textTheme.titleSmall?.copyWith(
                        color: fgColor,
                        fontWeight: FontWeight.w600,
                      ) ??
                      TextStyle(color: fgColor, fontSize: 13),
                  child: title ?? const SizedBox.shrink(),
                ),
              ),
            ),
          ),
          ...actions,
          if (!isMacOS && showWindowControls)
            _WindowCaptionControls(height: height, foregroundColor: fgColor),
        ],
      ),
    );
  }
}

class _WindowCaptionControls extends StatelessWidget {
  final double height;
  final Color foregroundColor;

  const _WindowCaptionControls({
    required this.height,
    required this.foregroundColor,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        _CaptionControlButton(
          height: height,
          icon: Icons.remove,
          foregroundColor: foregroundColor,
          tooltip: 'Minimize',
          onPressed: () => NativeWindowController.instance.minimize(),
        ),
        SnapLayoutButton(
          builder: (BuildContext context, bool isHovered, bool isMaximized) {
            return Container(
              width: 46,
              height: height,
              color: isHovered
                  ? foregroundColor.withValues(alpha: 0.1)
                  : Colors.transparent,
              alignment: Alignment.center,
              child: Icon(
                isMaximized ? Icons.filter_none : Icons.crop_square,
                size: isMaximized ? 14 : 16,
                color: foregroundColor,
              ),
            );
          },
        ),
        _CaptionControlButton(
          height: height,
          icon: Icons.close,
          foregroundColor: foregroundColor,
          hoverColor: const Color(0xFFE81123),
          hoverIconColor: Colors.white,
          tooltip: 'Close',
          onPressed: () => NativeWindowController.instance.close(),
        ),
      ],
    );
  }
}

class _CaptionControlButton extends StatefulWidget {
  final double height;
  final IconData icon;
  final Color foregroundColor;
  final Color? hoverColor;
  final Color? hoverIconColor;
  final String tooltip;
  final VoidCallback onPressed;

  const _CaptionControlButton({
    required this.height,
    required this.icon,
    required this.foregroundColor,
    this.hoverColor,
    this.hoverIconColor,
    required this.tooltip,
    required this.onPressed,
  });

  @override
  State<_CaptionControlButton> createState() => _CaptionControlButtonState();
}

class _CaptionControlButtonState extends State<_CaptionControlButton> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final Color bgColor = _hovered
        ? (widget.hoverColor ?? widget.foregroundColor.withValues(alpha: 0.1))
        : Colors.transparent;
    final Color iconColor = _hovered
        ? (widget.hoverIconColor ?? widget.foregroundColor)
        : widget.foregroundColor;

    return MouseRegion(
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: widget.onPressed,
        child: Container(
          width: 46,
          height: widget.height,
          color: bgColor,
          alignment: Alignment.center,
          child: Icon(widget.icon, size: 16, color: iconColor),
        ),
      ),
    );
  }
}

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
            NativeWindowController.instance.startResizing(edge);
          }
        },
        child: const SizedBox.expand(),
      ),
    );
  }
}

import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:material_ui/material_ui.dart';

import '../xue_hua_navite_window_manager_controller.dart';
import 'drag_area.dart';
import 'snap_layout_button.dart';

/// Width of one minimize / maximize / close slot.
const double _kCaptionControlWidth = 46.0;

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

    return LayoutBuilder(
      builder: (BuildContext context, BoxConstraints constraints) {
        final double barHeight = constraints.maxHeight.isFinite
            ? math.min(height, math.max(0.0, constraints.maxHeight))
            : height;
        if (barHeight <= 0) {
          return const SizedBox.shrink();
        }

        final double width = constraints.maxWidth;
        final bool bounded = width.isFinite;
        final double macInset = isMacOS
            ? (bounded
                  ? math.min(
                      macosTrafficLightsReservedWidth,
                      math.max(0.0, width),
                    )
                  : macosTrafficLightsReservedWidth)
            : 0.0;
        final bool showControls = !isMacOS && showWindowControls;
        final double controlsWidth = showControls
            ? _kCaptionControlWidth * 3
            : 0.0;
        // 48dp is the Material tap target. Hiding actions before that budget
        // is exhausted keeps the caption's own Row from overflowing when the
        // client area collapses (minimize animation, ~159px).
        final double estimatedActions = 48.0 * actions.length;
        final double available = bounded
            ? math.max(0.0, width - macInset)
            : double.infinity;
        final bool showActions =
            actions.isNotEmpty &&
            available >= controlsWidth + estimatedActions + 32.0;
        final bool showLeading =
            leading != null &&
            available >= controlsWidth + estimatedActions + 80.0;
        final bool scaleControls = showControls && available < controlsWidth;

        final Widget controls = _WindowCaptionControls(
          height: barHeight,
          foregroundColor: fgColor,
        );

        return ColoredBox(
          color: backgroundColor ?? Colors.transparent,
          child: SizedBox(
            height: barHeight,
            width: bounded ? width : null,
            child: Row(
              children: <Widget>[
                if (macInset > 0) SizedBox(width: macInset),
                if (showLeading)
                  Padding(
                    padding: const EdgeInsets.only(left: 8.0),
                    child: leading!,
                  ),
                Expanded(
                  child: DragArea(
                    child: ClipRect(
                      child: DefaultTextStyle(
                        style:
                            theme.textTheme.titleSmall?.copyWith(
                              color: fgColor,
                              fontWeight: FontWeight.w600,
                            ) ??
                            TextStyle(color: fgColor, fontSize: 13),
                        child: OverflowBox(
                          alignment: Alignment.centerLeft,
                          minWidth: 0,
                          maxWidth: double.infinity,
                          minHeight: 0,
                          maxHeight: barHeight,
                          child: title ?? const SizedBox.shrink(),
                        ),
                      ),
                    ),
                  ),
                ),
                if (showActions) ...actions,
                if (showControls)
                  scaleControls
                      ? Flexible(
                          child: FittedBox(
                            fit: BoxFit.scaleDown,
                            alignment: Alignment.centerRight,
                            child: controls,
                          ),
                        )
                      : controls,
              ],
            ),
          ),
        );
      },
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
          onPressed: () => XueHuaNaviteWindowManager.instance.minimize(),
        ),
        SnapLayoutButton(
          builder: (BuildContext context, bool isHovered, bool isMaximized) {
            return Container(
              width: _kCaptionControlWidth,
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
          onPressed: () => XueHuaNaviteWindowManager.instance.close(),
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

    return Tooltip(
      message: widget.tooltip,
      child: MouseRegion(
        onEnter: (_) => setState(() => _hovered = true),
        onExit: (_) => setState(() => _hovered = false),
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: widget.onPressed,
          child: Container(
            width: _kCaptionControlWidth,
            height: widget.height,
            color: bgColor,
            alignment: Alignment.center,
            child: Icon(widget.icon, size: 16, color: iconColor),
          ),
        ),
      ),
    );
  }
}

import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import '../xue_hua_navite_window_manager_controller.dart';
import 'drag_area.dart';
import 'snap_layout_button.dart';

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
          onPressed: () => XueHuaNaviteWindowManager.instance.minimize(),
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

import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';

import '../window_listener.dart';
import '../window_state_snapshot.dart';
import '../xue_hua_navite_window_manager_controller.dart';

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

  /// Optional override callback when clicked. Defaults to `XueHuaNaviteWindowManager.instance.toggleMaximize()`.
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
    XueHuaNaviteWindowManager.instance.addListener(this);
    _syncInitialState();
    _scheduleRectUpdate();
  }

  Future<void> _syncInitialState() async {
    final WindowStateSnapshot? cached =
        XueHuaNaviteWindowManager.instance.lastSnapshot;
    if (cached != null && mounted) {
      setState(() => _isMaximized = cached.isMaximized);
    }
    try {
      final WindowStateSnapshot fresh =
          await XueHuaNaviteWindowManager.instance.getState();
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
        XueHuaNaviteWindowManager.instance.setSnapLayoutButtonRect(null);
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
      final RenderObject? renderObject =
          _buttonKey.currentContext?.findRenderObject();
      if (renderObject is RenderBox && renderObject.hasSize) {
        final Offset topLeft = renderObject.localToGlobal(Offset.zero);
        final Rect rect = topLeft & renderObject.size;
        if (_lastReportedRect != rect) {
          _lastReportedRect = rect;
          XueHuaNaviteWindowManager.instance.setSnapLayoutButtonRect(rect);
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
      XueHuaNaviteWindowManager.instance.toggleMaximize();
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    XueHuaNaviteWindowManager.instance.removeListener(this);
    if (_isWindows && _lastReportedRect != null) {
      XueHuaNaviteWindowManager.instance.setSnapLayoutButtonRect(null);
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
            XueHuaNaviteWindowManager.instance.toggleMaximize();
          }
        },
        child: widget.builder(context, _isHovered, _isMaximized),
      ),
    );
  }
}

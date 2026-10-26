import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';

import 'xue_hua_navite_window_manager_controller.dart';

/// Publishes a widget's global rect to the Linux plugin so a later button
/// press can start a move or resize without a method-channel round trip.
///
/// Layout updates happen before the click. The native button-press emission
/// hook hit-tests these rects while the Wayland seat serial is still valid.
class LinuxInteractiveRegionBinding {
  /// Creates a binding for [id] from [XueHuaNaviteWindowManager.allocateLinuxInteractiveRegionId].
  LinuxInteractiveRegionBinding(this.id);

  /// Stable id shared with the native region table.
  final int id;

  Rect? _last;
  bool _scheduled = false;

  /// Schedules one publish after this frame. Safe to call from [build].
  void schedule(BuildContext context, {required String kind}) {
    if (kIsWeb || !Platform.isLinux) {
      return;
    }
    if (_scheduled) {
      return;
    }
    _scheduled = true;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _scheduled = false;
      if (!context.mounted) {
        return;
      }
      _publish(context, kind: kind);
    });
  }

  void _publish(BuildContext context, {required String kind}) {
    final RenderObject? object = context.findRenderObject();
    if (object is! RenderBox || !object.attached || !object.hasSize) {
      return;
    }
    final Rect rect = object.localToGlobal(Offset.zero) & object.size;
    if (rect == _last) {
      return;
    }
    XueHuaNaviteWindowManager.instance
        .setLinuxInteractiveRegion(
          id: id,
          present: true,
          kind: kind,
          rect: rect,
        )
        .then((_) {
          _last = rect;
        }, onError: (Object _) {});
  }

  /// Drops the native region. Failures are ignored so dispose cannot throw
  /// when the plugin is not registered (widget tests).
  void dispose() {
    _last = null;
    if (kIsWeb || !Platform.isLinux) {
      return;
    }
    XueHuaNaviteWindowManager.instance
        .setLinuxInteractiveRegion(id: id, present: false)
        .then((_) {}, onError: (Object _) {});
  }
}

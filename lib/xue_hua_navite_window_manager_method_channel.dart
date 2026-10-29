import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'src/launch_at_startup.dart';
import 'src/window_config.dart';
import 'src/window_enums.dart';
import 'src/window_event.dart';
import 'src/window_state_snapshot.dart';
import 'xue_hua_navite_window_manager_platform_interface.dart';

/// An implementation of [XueHuaNaviteWindowManagerPlatform] that uses method and event channels.
class MethodChannelXueHuaNaviteWindowManager
    extends XueHuaNaviteWindowManagerPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final MethodChannel methodChannel = const MethodChannel(
    'xue_hua_navite_window_manager/methods',
  );

  /// The event channel used to receive real-time window state events from the native platform.
  @visibleForTesting
  final EventChannel eventChannel = const EventChannel(
    'xue_hua_navite_window_manager/events',
  );

  Stream<WindowEvent>? _windowEvents;

  @override
  Stream<WindowEvent> get windowEvents {
    _windowEvents ??= eventChannel
        .receiveBroadcastStream()
        .map((dynamic raw) {
          if (raw is Map) {
            return WindowEvent.fromMap(raw);
          }
          return null;
        })
        .where((WindowEvent? event) => event != null)
        .cast<WindowEvent>();
    return _windowEvents!;
  }

  @override
  Future<void> ensureInitialized() async {
    await methodChannel.invokeMethod<void>('ensureInitialized');
  }

  @override
  Future<WindowStateSnapshot> configure(WindowConfig config) async {
    final Map<Object?, Object?>? result = await methodChannel
        .invokeMapMethod<Object?, Object?>('configure', config.toMap());
    return WindowStateSnapshot.fromMap(result ?? const <Object?, Object?>{});
  }

  @override
  Future<void> markReadyToShow() async {
    await methodChannel.invokeMethod<void>('markReadyToShow');
  }

  @override
  Future<WindowStateSnapshot> getState() async {
    final Map<Object?, Object?>? result = await methodChannel
        .invokeMapMethod<Object?, Object?>('getState');
    return WindowStateSnapshot.fromMap(result ?? const <Object?, Object?>{});
  }

  @override
  Future<void> show({bool focus = true}) async {
    await methodChannel.invokeMethod<void>('show', <String, dynamic>{
      'focus': focus,
    });
  }

  @override
  Future<void> hide() async {
    await methodChannel.invokeMethod<void>('hide');
  }

  @override
  Future<void> focus() async {
    await methodChannel.invokeMethod<void>('focus');
  }

  @override
  Future<void> blur() async {
    await methodChannel.invokeMethod<void>('blur');
  }

  @override
  Future<void> close() async {
    await methodChannel.invokeMethod<void>('close');
  }

  @override
  Future<void> destroy() async {
    await methodChannel.invokeMethod<void>('destroy');
  }

  @override
  Future<void> maximize() async {
    await methodChannel.invokeMethod<void>('maximize');
  }

  @override
  Future<void> unmaximize() async {
    await methodChannel.invokeMethod<void>('unmaximize');
  }

  @override
  Future<void> minimize() async {
    await methodChannel.invokeMethod<void>('minimize');
  }

  @override
  Future<void> restore() async {
    await methodChannel.invokeMethod<void>('restore');
  }

  @override
  Future<void> setFullScreen(bool isFullScreen) async {
    await methodChannel.invokeMethod<void>('setFullScreen', <String, dynamic>{
      'isFullScreen': isFullScreen,
    });
  }

  @override
  Future<void> setBounds(Rect bounds, {bool animate = false}) async {
    await methodChannel.invokeMethod<void>('setBounds', <String, dynamic>{
      'x': bounds.left,
      'y': bounds.top,
      'width': bounds.width,
      'height': bounds.height,
      'animate': animate,
    });
  }

  @override
  Future<Rect> getBounds() async {
    final Map<Object?, Object?>? map = await methodChannel
        .invokeMapMethod<Object?, Object?>('getBounds');
    final double x = (map?['x'] as num?)?.toDouble() ?? 0.0;
    final double y = (map?['y'] as num?)?.toDouble() ?? 0.0;
    final double width = (map?['width'] as num?)?.toDouble() ?? 800.0;
    final double height = (map?['height'] as num?)?.toDouble() ?? 600.0;
    return Rect.fromLTWH(x, y, width, height);
  }

  @override
  Future<void> setSize(Size size, {bool animate = false}) async {
    await methodChannel.invokeMethod<void>('setSize', <String, dynamic>{
      'width': size.width,
      'height': size.height,
      'animate': animate,
    });
  }

  @override
  Future<Size> getSize() async {
    final Rect bounds = await getBounds();
    return bounds.size;
  }

  @override
  Future<void> setPosition(Offset position, {bool animate = false}) async {
    await methodChannel.invokeMethod<void>('setPosition', <String, dynamic>{
      'x': position.dx,
      'y': position.dy,
      'animate': animate,
    });
  }

  @override
  Future<Offset> getPosition() async {
    final Rect bounds = await getBounds();
    return bounds.topLeft;
  }

  @override
  Future<bool> center({Size? size, bool animate = false}) async {
    final Object? raw = await methodChannel.invokeMethod<Object?>(
      'center',
      <String, dynamic>{
        if (size != null) 'width': size.width,
        if (size != null) 'height': size.height,
        'animate': animate,
      },
    );
    // Linux returns a bool. Windows and macOS return null and do apply the move.
    if (raw is bool) {
      return raw;
    }
    return true;
  }

  @override
  Future<void> setMinimumSize(Size size) async {
    await methodChannel.invokeMethod<void>('setMinimumSize', <String, dynamic>{
      'width': size.width,
      'height': size.height,
    });
  }

  @override
  Future<void> setMaximumSize(Size size) async {
    await methodChannel.invokeMethod<void>('setMaximumSize', <String, dynamic>{
      'width': size.width,
      'height': size.height,
    });
  }

  @override
  Future<void> setResizable(bool resizable) async {
    await methodChannel.invokeMethod<void>('setResizable', <String, dynamic>{
      'resizable': resizable,
    });
  }

  @override
  Future<void> setAlwaysOnTop(bool alwaysOnTop) async {
    await methodChannel.invokeMethod<void>('setAlwaysOnTop', <String, dynamic>{
      'alwaysOnTop': alwaysOnTop,
    });
  }

  @override
  Future<void> setPreventClose(bool preventClose) async {
    await methodChannel.invokeMethod<void>('setPreventClose', <String, dynamic>{
      'preventClose': preventClose,
    });
  }

  @override
  Future<void> setTitle(String title) async {
    await methodChannel.invokeMethod<void>('setTitle', <String, dynamic>{
      'title': title,
    });
  }

  @override
  Future<void> setTitleBarStyle(
    TitleBarStyle style, {
    Offset? trafficLightsOffset,
    bool? trafficLightsVisible,
  }) async {
    await methodChannel.invokeMethod<void>(
      'setTitleBarStyle',
      <String, dynamic>{
        'titleBarStyle': style.wireValue,
        if (trafficLightsOffset != null) ...<String, dynamic>{
          'trafficLightsOffsetX': trafficLightsOffset.dx,
          'trafficLightsOffsetY': trafficLightsOffset.dy,
        },
        'trafficLightsVisible': ?trafficLightsVisible,
      },
    );
  }

  @override
  Future<void> setTrafficLightsOffset(Offset offset) async {
    await methodChannel.invokeMethod<void>(
      'setTrafficLightsOffset',
      <String, dynamic>{'x': offset.dx, 'y': offset.dy},
    );
  }

  @override
  Future<void> setTrafficLightsVisible(bool visible) async {
    await methodChannel.invokeMethod<void>(
      'setTrafficLightsVisible',
      <String, dynamic>{'visible': visible},
    );
  }

  @override
  Future<void> setBackgroundColor(Color color) async {
    await methodChannel.invokeMethod<void>(
      'setBackgroundColor',
      <String, dynamic>{'backgroundColor': color.toARGB32()},
    );
  }

  @override
  Future<void> setOpacity(double opacity) async {
    await methodChannel.invokeMethod<void>('setOpacity', <String, dynamic>{
      'opacity': opacity.clamp(0.0, 1.0),
    });
  }

  @override
  Future<void> setHasShadow(bool hasShadow) async {
    await methodChannel.invokeMethod<void>('setHasShadow', <String, dynamic>{
      'hasShadow': hasShadow,
    });
  }

  @override
  Future<void> startDragging() async {
    await methodChannel.invokeMethod<void>('startDragging');
  }

  @override
  Future<void> startResizing(ResizeEdge edge) async {
    await methodChannel.invokeMethod<void>('startResizing', <String, dynamic>{
      'edge': edge.wireValue,
    });
  }

  @override
  Future<void> setSnapLayoutButtonRect(Rect? rect) async {
    await methodChannel.invokeMethod<void>(
      'setSnapLayoutButtonRect',
      rect == null
          ? <String, dynamic>{'enabled': false}
          : <String, dynamic>{
              'enabled': true,
              'x': rect.left,
              'y': rect.top,
              'width': rect.width,
              'height': rect.height,
            },
    );
  }

  @override
  Future<void> setLinuxInteractiveRegion({
    required int id,
    required bool present,
    String kind = 'move',
    Rect? rect,
  }) async {
    await methodChannel.invokeMethod<void>(
      'setLinuxInteractiveRegion',
      <String, dynamic>{
        'id': id,
        'present': present,
        'kind': kind,
        if (rect != null) 'x': rect.left,
        if (rect != null) 'y': rect.top,
        if (rect != null) 'width': rect.width,
        if (rect != null) 'height': rect.height,
      },
    );
  }

  @override
  Future<void> setTitleBarDragRects({
    required List<Rect> dragRects,
    List<Rect> excludeRects = const <Rect>[],
  }) async {
    await methodChannel.invokeMethod<void>(
      'setTitleBarDragRects',
      <String, dynamic>{
        'dragRects': dragRects
            .map(
              (Rect r) => <String, double>{
                'x': r.left,
                'y': r.top,
                'width': r.width,
                'height': r.height,
              },
            )
            .toList(),
        'excludeRects': excludeRects
            .map(
              (Rect r) => <String, double>{
                'x': r.left,
                'y': r.top,
                'width': r.width,
                'height': r.height,
              },
            )
            .toList(),
      },
    );
  }

  @override
  Future<LaunchAtStartupState> getLaunchAtStartupState({
    LaunchAtStartupConfig? config,
  }) async {
    final Map<String, dynamic>? payload = config?.toMap();
    final Map<Object?, Object?>? result = await methodChannel
        .invokeMapMethod<Object?, Object?>(
          'getLaunchAtStartupState',
          (payload == null || payload.isEmpty) ? null : payload,
        );
    return LaunchAtStartupState.fromMap(result ?? const <Object?, Object?>{});
  }

  @override
  Future<LaunchAtStartupState> setLaunchAtStartup(
    bool enabled, {
    LaunchAtStartupConfig? config,
  }) async {
    final Map<Object?, Object?>? result = await methodChannel
        .invokeMapMethod<Object?, Object?>(
          'setLaunchAtStartup',
          <String, dynamic>{'enabled': enabled, ...?config?.toMap()},
        );
    return LaunchAtStartupState.fromMap(result ?? const <Object?, Object?>{});
  }

  @override
  Future<bool> openLaunchAtStartupSettings() async {
    final Object? raw = await methodChannel.invokeMethod<Object?>(
      'openLaunchAtStartupSettings',
    );
    if (raw is bool) {
      return raw;
    }
    return false;
  }
}

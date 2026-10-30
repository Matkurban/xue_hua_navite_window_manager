import 'dart:math' as math;

import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import '../controllers/window_studio_controller.dart';
import '../widgets/cards/atomic_presets_card.dart';
import '../widgets/cards/behavior_appearance_card.dart';
import '../widgets/cards/drag_edge_resize_card.dart';
import '../widgets/cards/launch_at_startup_card.dart';
import '../widgets/cards/status_telemetry_card.dart';
import '../widgets/cards/title_bar_traffic_lights_card.dart';
import '../widgets/close_confirmation_dialog.dart';
import '../widgets/event_stream_panel.dart';
import '../widgets/studio_caption_bar.dart';

/// Interactive workbench demonstrating atomic configuration, custom frameless caption,
/// macOS Traffic Lights offset slider, Windows 11 Snap Layouts (`HTMAXBUTTON`),
/// 8-edge programmatic resizing, Launch at Startup, and real-time `WindowListener` event logs.
///
/// All colors and typography in this dashboard are driven exclusively by the
/// active [ThemeData] (`Theme.of(context).colorScheme` and `Theme.of(context).textTheme`).
class WindowStudioDashboard extends StatefulWidget {
  const WindowStudioDashboard({super.key});

  @override
  State<WindowStudioDashboard> createState() => _WindowStudioDashboardState();
}

class _WindowStudioDashboardState extends State<WindowStudioDashboard> {
  late final WindowStudioController _controller;

  @override
  void initState() {
    super.initState();
    _controller = WindowStudioController(
      onCloseIntercepted: _showCloseConfirmationDialog,
    )..init();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Future<void> _showCloseConfirmationDialog() async {
    if (!mounted) return;
    final bool? shouldDestroy = await CloseConfirmationDialog.show(
      context,
      onMinimizeInstead: _controller.minimize,
    );
    if (shouldDestroy == true) {
      await _controller.forceDestroy();
    }
  }

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final Size viewportSize = MediaQuery.sizeOf(context);
    final double captionHeight = math.min(
      44.0,
      math.max(0.0, viewportSize.height),
    );
    final bool showBody = viewportSize.height >= captionHeight + 72.0;
    final double paneWidth = math.max(
      0.0,
      (viewportSize.width - 1.0) * 7.0 / 11.0,
    );
    final double contentWidth = math.max(paneWidth, 480.0);

    return ListenableBuilder(
      listenable: _controller,
      builder: (BuildContext context, _) {
        final Color activeBgColor =
            _controller.selectedBackgroundColor ??
            theme.scaffoldBackgroundColor;

        return Scaffold(
          backgroundColor: activeBgColor,
          body: WindowResizeEdgeArea(
            enabled:
                _controller.resizable &&
                !_controller.isMaximized &&
                !_controller.isFullScreen,
            child: Column(
              children: <Widget>[
                StudioCaptionBar(
                  height: captionHeight,
                  width: viewportSize.width,
                  trafficLightsVisible: _controller.trafficLightsVisible,
                  trafficLightsX: _controller.trafficLightsX,
                  isFullScreen: _controller.isFullScreen,
                  isFocused: _controller.isFocused,
                  onCenterPressed: _controller.centerDefaultSize,
                  onToggleFullScreenPressed: _controller.toggleFullScreen,
                ),
                if (showBody) Divider(height: 1, color: theme.dividerColor),
                if (showBody)
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: <Widget>[
                        Expanded(
                          flex: 7,
                          child: SingleChildScrollView(
                            scrollDirection: Axis.horizontal,
                            child: SizedBox(
                              width: contentWidth,
                              child: SingleChildScrollView(
                                padding: const EdgeInsets.all(20),
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: <Widget>[
                                    StatusTelemetryCard(
                                      currentBounds: _controller.currentBounds,
                                      isMaximized: _controller.isMaximized,
                                      isMinimized: _controller.isMinimized,
                                      isFullScreen: _controller.isFullScreen,
                                      snapButtonHovered:
                                          _controller.snapButtonHovered,
                                      onSyncState:
                                          _controller.refreshWindowState,
                                    ),
                                    const SizedBox(height: 16),
                                    AtomicPresetsCard(
                                      onApplyStudioDefault:
                                          _controller.applyStudioDefaultPreset,
                                      onApplyCompactImmersive: _controller
                                          .applyCompactImmersivePreset,
                                      onApplySystemTitleBar:
                                          _controller.applySystemTitleBarPreset,
                                    ),
                                    const SizedBox(height: 16),
                                    TitleBarTrafficLightsCard(
                                      titleBarStyle: _controller.titleBarStyle,
                                      trafficLightsX:
                                          _controller.trafficLightsX,
                                      trafficLightsY:
                                          _controller.trafficLightsY,
                                      trafficLightsVisible:
                                          _controller.trafficLightsVisible,
                                      isMacOS: _controller.isMacOS,
                                      onTitleBarStyleChanged:
                                          _controller.updateTitleBarStyle,
                                      onTrafficLightsXChanged:
                                          _controller.updateTrafficLightsX,
                                      onTrafficLightsYChanged:
                                          _controller.updateTrafficLightsY,
                                      onTrafficLightsVisibleChanged: _controller
                                          .updateTrafficLightsVisible,
                                    ),
                                    const SizedBox(height: 16),
                                    BehaviorAppearanceCard(
                                      preventClose: _controller.preventClose,
                                      alwaysOnTop: _controller.alwaysOnTop,
                                      resizable: _controller.resizable,
                                      hasShadow: _controller.hasShadow,
                                      opacity: _controller.opacity,
                                      activeBackgroundColor: activeBgColor,
                                      onPreventCloseChanged:
                                          _controller.updatePreventClose,
                                      onAlwaysOnTopChanged:
                                          _controller.updateAlwaysOnTop,
                                      onResizableChanged:
                                          _controller.updateResizable,
                                      onHasShadowChanged:
                                          _controller.updateHasShadow,
                                      onOpacityChanged:
                                          _controller.updateOpacity,
                                      onBackgroundColorSelected:
                                          _controller.selectBackgroundColor,
                                    ),
                                    const SizedBox(height: 16),
                                    DragEdgeResizeCard(
                                      isMaximized: _controller.isMaximized,
                                      onDragStart:
                                          _controller.onCustomDragAreaStart,
                                      onMinimize: _controller.minimize,
                                      onToggleMaximize:
                                          _controller.toggleMaximize,
                                      onCenter: _controller.centerCurrentSize,
                                      onRequestClose: _controller.requestClose,
                                    ),
                                    const SizedBox(height: 16),
                                    LaunchAtStartupCard(
                                      startupState: _controller.startupState,
                                      passMinimizedArg:
                                          _controller.startupPassMinimizedArg,
                                      isBusy: _controller.startupBusy,
                                      onToggleEnabled:
                                          _controller.toggleLaunchAtStartup,
                                      onPassMinimizedArgChanged: _controller
                                          .setStartupPassMinimizedArg,
                                      onRefresh: _controller
                                          .refreshLaunchAtStartupState,
                                      onOpenSettings: _controller
                                          .openLaunchAtStartupSettings,
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          ),
                        ),
                        VerticalDivider(width: 1, color: theme.dividerColor),
                        Expanded(
                          flex: 4,
                          child: EventStreamPanel(
                            eventLogs: _controller.eventLogs,
                            onClear: _controller.clearLogs,
                          ),
                        ),
                      ],
                    ),
                  ),
              ],
            ),
          ),
        );
      },
    );
  }
}

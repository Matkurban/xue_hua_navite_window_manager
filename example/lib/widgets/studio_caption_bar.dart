import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Custom frameless Studio title bar wrapping [WindowCaption] with focus status
/// telemetry and quick center / fullscreen actions.
class StudioCaptionBar extends StatelessWidget {
  const StudioCaptionBar({
    super.key,
    required this.height,
    required this.width,
    required this.trafficLightsVisible,
    required this.trafficLightsX,
    required this.isFullScreen,
    required this.isFocused,
    required this.onCenterPressed,
    required this.onToggleFullScreenPressed,
  });

  final double height;
  final double? width;
  final bool trafficLightsVisible;
  final double trafficLightsX;
  final bool isFullScreen;
  final bool isFocused;
  final VoidCallback onCenterPressed;
  final VoidCallback onToggleFullScreenPressed;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;
    final TextTheme textTheme = theme.textTheme;

    return SizedBox(
      height: height,
      width: width,
      child: WindowCaption(
        height: height,
        macosTrafficLightsReservedWidth: trafficLightsVisible && !isFullScreen
            ? (trafficLightsX + 68.0)
            : 12.0,
        backgroundColor: colorScheme.surfaceContainer,
        foregroundColor: colorScheme.onSurface,
        title: Row(
          children: <Widget>[
            Icon(
              Icons.desktop_windows_rounded,
              size: 18,
              color: colorScheme.primary,
            ),
            const SizedBox(width: 10),
            Text(
              'XueHua Native Window Manager Studio',
              style: textTheme.titleSmall,
            ),
            const SizedBox(width: 12),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
              decoration: BoxDecoration(
                color: isFocused
                    ? colorScheme.primaryContainer
                    : colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(6),
              ),
              child: Text(
                isFocused ? 'ACTIVE' : 'BLURRED',
                style: textTheme.labelSmall?.copyWith(
                  color: isFocused
                      ? colorScheme.onPrimaryContainer
                      : colorScheme.onSurfaceVariant,
                ),
              ),
            ),
          ],
        ),
        actions: <Widget>[
          IconButton(
            tooltip: 'Center Window (1240x820)',
            icon: Icon(
              Icons.filter_center_focus,
              size: 18,
              color: colorScheme.onSurfaceVariant,
            ),
            onPressed: onCenterPressed,
          ),
          IconButton(
            tooltip: 'Toggle FullScreen',
            icon: Icon(
              isFullScreen
                  ? Icons.fullscreen_exit_rounded
                  : Icons.fullscreen_rounded,
              size: 20,
              color: colorScheme.onSurfaceVariant,
            ),
            onPressed: onToggleFullScreenPressed,
          ),
          const SizedBox(width: 4),
        ],
      ),
    );
  }
}

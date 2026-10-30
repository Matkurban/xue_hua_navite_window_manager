import 'package:material_ui/material_ui.dart';

import '../studio_metric_badge.dart';

/// Displays live window geometry, state flags, Windows 11 Snap hover state,
/// and a manual state synchronization action.
class StatusTelemetryCard extends StatelessWidget {
  const StatusTelemetryCard({
    super.key,
    required this.currentBounds,
    required this.isMaximized,
    required this.isMinimized,
    required this.isFullScreen,
    required this.snapButtonHovered,
    required this.onSyncState,
  });

  final Rect currentBounds;
  final bool isMaximized;
  final bool isMinimized;
  final bool isFullScreen;
  final bool snapButtonHovered;
  final VoidCallback onSyncState;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;
    final TextTheme textTheme = theme.textTheme;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                Flexible(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.centerLeft,
                    child: Icon(Icons.speed_rounded, color: colorScheme.primary),
                  ),
                ),
                const Flexible(child: SizedBox(width: 8)),
                Expanded(
                  flex: 6,
                  child: Text(
                    'Live Window Geometry & State Telemetry',
                    style: textTheme.titleMedium,
                    overflow: TextOverflow.ellipsis,
                    maxLines: 1,
                  ),
                ),
                Flexible(
                  flex: 5,
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.centerRight,
                    child: TextButton.icon(
                      onPressed: onSyncState,
                      icon: const Icon(Icons.refresh, size: 16),
                      label: const Text('Sync State'),
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                StudioMetricBadge(
                  label: 'Size (DIP)',
                  value:
                      '${currentBounds.width.toStringAsFixed(0)} × ${currentBounds.height.toStringAsFixed(0)}',
                ),
                StudioMetricBadge(
                  label: 'Position (DIP)',
                  value:
                      '(${currentBounds.left.toStringAsFixed(0)}, ${currentBounds.top.toStringAsFixed(0)})',
                ),
                StudioMetricBadge(
                  label: 'Maximized',
                  value: isMaximized ? 'YES' : 'NO',
                ),
                StudioMetricBadge(
                  label: 'Minimized',
                  value: isMinimized ? 'YES' : 'NO',
                ),
                StudioMetricBadge(
                  label: 'FullScreen',
                  value: isFullScreen ? 'YES' : 'NO',
                ),
                StudioMetricBadge(
                  label: 'Snap Hover',
                  value: snapButtonHovered ? 'HTMAXBUTTON' : 'IDLE',
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

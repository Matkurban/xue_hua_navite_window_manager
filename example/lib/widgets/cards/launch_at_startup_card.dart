import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

import '../studio_metric_badge.dart';

/// Section 5: Interactive launch-at-startup (login auto-start) telemetry and
/// registration controls.
class LaunchAtStartupCard extends StatelessWidget {
  const LaunchAtStartupCard({
    super.key,
    required this.startupState,
    required this.passMinimizedArg,
    required this.isBusy,
    required this.onToggleEnabled,
    required this.onPassMinimizedArgChanged,
    required this.onRefresh,
    required this.onOpenSettings,
  });

  final LaunchAtStartupState startupState;
  final bool passMinimizedArg;
  final bool isBusy;
  final ValueChanged<bool> onToggleEnabled;
  final ValueChanged<bool> onPassMinimizedArgChanged;
  final VoidCallback onRefresh;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;
    final TextTheme textTheme = theme.textTheme;
    final String? id = startupState.identifier;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              '5. Launch at Startup (Login Auto-Start)',
              style: textTheme.titleMedium,
            ),
            const SizedBox(height: 6),
            Text(
              'Registers or removes user-login auto-start via SMAppService / LaunchAgents (macOS), '
              'HKCU CurrentVersion\\Run & StartupApproved / MSIX StartupTask (Windows), '
              'and XDG ~/.config/autostart (Linux).',
              style: textTheme.bodySmall?.copyWith(
                color: colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                StudioMetricBadge(
                  label: 'Startup Status',
                  value: startupState.status.wireValue,
                ),
                StudioMetricBadge(
                  label: 'Backend',
                  value: startupState.backend.wireValue,
                ),
                StudioMetricBadge(
                  label: 'Identifier',
                  value: (id == null || id.isEmpty) ? '—' : id,
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 8,
              children: <Widget>[
                FilterChip(
                  label: const Text('Enable Launch at Startup'),
                  selected: startupState.isEnabled,
                  onSelected: isBusy || !startupState.isSupported
                      ? null
                      : onToggleEnabled,
                ),
                FilterChip(
                  label: const Text('Pass --minimized Argument'),
                  selected: passMinimizedArg,
                  onSelected: isBusy ? null : onPassMinimizedArgChanged,
                ),
                OutlinedButton.icon(
                  onPressed: isBusy ? null : onRefresh,
                  icon: const Icon(Icons.refresh_rounded, size: 16),
                  label: const Text('Refresh Startup State'),
                ),
                if (startupState.canOpenSettings)
                  OutlinedButton.icon(
                    onPressed: onOpenSettings,
                    icon: const Icon(Icons.open_in_new_rounded, size: 16),
                    label: const Text('Open OS Startup Settings'),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

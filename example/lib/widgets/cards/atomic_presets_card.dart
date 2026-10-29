import 'package:material_ui/material_ui.dart';

/// Section 1: Demonstrates single-channel atomic window configuration presets.
class AtomicPresetsCard extends StatelessWidget {
  const AtomicPresetsCard({
    super.key,
    required this.onApplyStudioDefault,
    required this.onApplyCompactImmersive,
    required this.onApplySystemTitleBar,
  });

  final ValueChanged<Color> onApplyStudioDefault;
  final ValueChanged<Color> onApplyCompactImmersive;
  final VoidCallback onApplySystemTitleBar;

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
            Text(
              '1. Atomic Batch Configuration (Single-Channel Zero-Jump Presets)',
              style: textTheme.titleMedium,
            ),
            const SizedBox(height: 6),
            Text(
              'Applies size, min/max constraints, centering, titleBarStyle, trafficLightsOffset, '
              'and backgroundColor in one atomic native transaction.',
              style: textTheme.bodySmall?.copyWith(
                color: colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                FilledButton.icon(
                  onPressed: () =>
                      onApplyStudioDefault(theme.scaffoldBackgroundColor),
                  icon: const Icon(Icons.bolt_rounded, size: 18),
                  label: const Text('Studio Preset (1280×820 Centered)'),
                ),
                FilledButton.tonalIcon(
                  onPressed: () => onApplyCompactImmersive(
                    colorScheme.surfaceContainerLowest,
                  ),
                  icon: const Icon(Icons.layers_rounded, size: 18),
                  label: const Text('Compact Immersive (980×660)'),
                ),
                OutlinedButton.icon(
                  onPressed: onApplySystemTitleBar,
                  icon: const Icon(Icons.web_asset_rounded, size: 18),
                  label: const Text('System TitleBar (1100×720)'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

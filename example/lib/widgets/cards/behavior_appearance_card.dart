import 'package:material_ui/material_ui.dart';

/// Section 3: Controls window behavior flags, opacity slider, and native
/// anti-tearing background color swatches.
class BehaviorAppearanceCard extends StatelessWidget {
  const BehaviorAppearanceCard({
    super.key,
    required this.preventClose,
    required this.alwaysOnTop,
    required this.resizable,
    required this.hasShadow,
    required this.opacity,
    required this.activeBackgroundColor,
    required this.onPreventCloseChanged,
    required this.onAlwaysOnTopChanged,
    required this.onResizableChanged,
    required this.onHasShadowChanged,
    required this.onOpacityChanged,
    required this.onBackgroundColorSelected,
  });

  final bool preventClose;
  final bool alwaysOnTop;
  final bool resizable;
  final bool hasShadow;
  final double opacity;
  final Color activeBackgroundColor;
  final ValueChanged<bool> onPreventCloseChanged;
  final ValueChanged<bool> onAlwaysOnTopChanged;
  final ValueChanged<bool> onResizableChanged;
  final ValueChanged<bool> onHasShadowChanged;
  final ValueChanged<double> onOpacityChanged;
  final void Function(String label, Color color) onBackgroundColorSelected;

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
              '3. Window Behavior, DWM Shadow & Anti-Tearing Background Sync',
              style: textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 16,
              runSpacing: 4,
              children: <Widget>[
                FilterChip(
                  label: const Text('Prevent Close (Intercept WM_CLOSE)'),
                  selected: preventClose,
                  onSelected: onPreventCloseChanged,
                ),
                FilterChip(
                  label: const Text('Always On Top'),
                  selected: alwaysOnTop,
                  onSelected: onAlwaysOnTopChanged,
                ),
                FilterChip(
                  label: const Text('Resizable'),
                  selected: resizable,
                  onSelected: onResizableChanged,
                ),
                FilterChip(
                  label: const Text('Native Shadow & Rounded Corners'),
                  selected: hasShadow,
                  onSelected: onHasShadowChanged,
                ),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: <Widget>[
                Text('Window Opacity: ', style: textTheme.labelMedium),
                Expanded(
                  child: Slider(
                    value: opacity,
                    min: 0.35,
                    max: 1.0,
                    onChanged: onOpacityChanged,
                  ),
                ),
                Text(
                  '${(opacity * 100).toStringAsFixed(0)}%',
                  style: textTheme.labelMedium,
                ),
              ],
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                _ColorSwatchButton(
                  label: 'Theme Surface',
                  color: theme.scaffoldBackgroundColor,
                  activeBackgroundColor: activeBackgroundColor,
                  onSelected: onBackgroundColorSelected,
                ),
                _ColorSwatchButton(
                  label: 'Surface Container Low',
                  color: colorScheme.surfaceContainerLow,
                  activeBackgroundColor: activeBackgroundColor,
                  onSelected: onBackgroundColorSelected,
                ),
                _ColorSwatchButton(
                  label: 'Surface Container High',
                  color: colorScheme.surfaceContainerHigh,
                  activeBackgroundColor: activeBackgroundColor,
                  onSelected: onBackgroundColorSelected,
                ),
                _ColorSwatchButton(
                  label: 'Secondary Container',
                  color: colorScheme.secondaryContainer,
                  activeBackgroundColor: activeBackgroundColor,
                  onSelected: onBackgroundColorSelected,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _ColorSwatchButton extends StatelessWidget {
  const _ColorSwatchButton({
    required this.label,
    required this.color,
    required this.activeBackgroundColor,
    required this.onSelected,
  });

  final String label;
  final Color color;
  final Color activeBackgroundColor;
  final void Function(String label, Color color) onSelected;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;
    final TextTheme textTheme = theme.textTheme;
    final bool isSelected =
        activeBackgroundColor.toARGB32() == color.toARGB32();

    return OutlinedButton.icon(
      onPressed: () => onSelected(label, color),
      icon: Container(
        width: 14,
        height: 14,
        decoration: BoxDecoration(
          color: color,
          shape: BoxShape.circle,
          border: Border.all(color: colorScheme.outline),
        ),
      ),
      label: Text(
        isSelected ? '$label ✓' : label,
        style: textTheme.labelMedium,
      ),
    );
  }
}

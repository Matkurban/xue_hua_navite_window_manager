import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Section 4: Demonstrates custom [DragArea] and imperative lifecycle buttons.
class DragEdgeResizeCard extends StatelessWidget {
  const DragEdgeResizeCard({
    super.key,
    required this.isMaximized,
    required this.onDragStart,
    required this.onMinimize,
    required this.onToggleMaximize,
    required this.onCenter,
    required this.onRequestClose,
  });

  final bool isMaximized;
  final VoidCallback onDragStart;
  final VoidCallback onMinimize;
  final VoidCallback onToggleMaximize;
  final VoidCallback onCenter;
  final VoidCallback onRequestClose;

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
              '4. Native Drag Area, Lifecycle Actions & 8-Direction Resizing',
              style: textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            DragArea(
              onDragStart: onDragStart,
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: <Color>[
                      colorScheme.primaryContainer,
                      colorScheme.secondaryContainer,
                    ],
                  ),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: colorScheme.primary),
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: <Widget>[
                    Icon(
                      Icons.open_with_rounded,
                      color: colorScheme.onPrimaryContainer,
                    ),
                    const SizedBox(width: 10),
                    Flexible(
                      child: Text(
                        'Custom DragArea Widget — Press & Drag Anywhere Here (Double-Click to Maximize)',
                        style: textTheme.labelLarge?.copyWith(
                          color: colorScheme.onPrimaryContainer,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: <Widget>[
                ElevatedButton.icon(
                  onPressed: onMinimize,
                  icon: const Icon(Icons.minimize, size: 16),
                  label: const Text('Minimize'),
                ),
                ElevatedButton.icon(
                  onPressed: onToggleMaximize,
                  icon: const Icon(Icons.crop_square, size: 16),
                  label: Text(isMaximized ? 'Unmaximize' : 'Maximize'),
                ),
                ElevatedButton.icon(
                  onPressed: onCenter,
                  icon: const Icon(Icons.center_focus_strong, size: 16),
                  label: const Text('Center'),
                ),
                ElevatedButton.icon(
                  onPressed: onRequestClose,
                  icon: const Icon(Icons.close, size: 16),
                  label: const Text('Request Close'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

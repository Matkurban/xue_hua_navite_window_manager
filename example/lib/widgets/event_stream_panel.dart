import 'package:material_ui/material_ui.dart';

/// Right-hand live native `EventChannel` log viewer panel.
class EventStreamPanel extends StatelessWidget {
  const EventStreamPanel({
    super.key,
    required this.eventLogs,
    required this.onClear,
  });

  final List<String> eventLogs;
  final VoidCallback onClear;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;
    final TextTheme textTheme = theme.textTheme;

    return Container(
      color: colorScheme.surfaceContainerLow,
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
                  child: Icon(
                    Icons.terminal_rounded,
                    size: 18,
                    color: colorScheme.primary,
                  ),
                ),
              ),
              const Flexible(child: SizedBox(width: 8)),
              Expanded(
                flex: 7,
                child: Text(
                  'Native EventChannel Stream',
                  style: textTheme.titleSmall,
                  overflow: TextOverflow.ellipsis,
                  maxLines: 1,
                ),
              ),
              Flexible(
                flex: 4,
                child: FittedBox(
                  fit: BoxFit.scaleDown,
                  alignment: Alignment.centerRight,
                  child: TextButton(
                    onPressed: onClear,
                    child: Text('Clear', style: textTheme.labelMedium),
                  ),
                ),
              ),
            ],
          ),
          Divider(color: theme.dividerColor),
          Expanded(
            child: eventLogs.isEmpty
                ? Center(
                    child: Text(
                      'Move, resize, maximize, focus, or click close to inspect real-time native events.',
                      textAlign: TextAlign.center,
                      style: textTheme.bodySmall?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                      ),
                    ),
                  )
                : ListView.builder(
                    itemCount: eventLogs.length,
                    itemBuilder: (BuildContext context, int index) {
                      return Padding(
                        padding: const EdgeInsets.symmetric(vertical: 3),
                        child: Text(
                          eventLogs[index],
                          style: textTheme.bodySmall?.copyWith(
                            color: colorScheme.primary,
                          ),
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

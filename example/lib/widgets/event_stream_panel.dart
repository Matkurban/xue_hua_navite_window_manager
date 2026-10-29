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

    return LayoutBuilder(
      builder: (BuildContext context, BoxConstraints panelConstraints) {
        if (panelConstraints.maxHeight.isFinite &&
            panelConstraints.maxHeight < 72) {
          return const SizedBox.shrink();
        }
        return Container(
          color: colorScheme.surfaceContainerLow,
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              LayoutBuilder(
                builder:
                    (BuildContext context, BoxConstraints headerConstraints) {
                      final Widget clearButton = TextButton(
                        onPressed: onClear,
                        child: Text('Clear', style: textTheme.labelMedium),
                      );
                      if (headerConstraints.maxWidth < 220) {
                        return FittedBox(
                          fit: BoxFit.scaleDown,
                          alignment: Alignment.centerLeft,
                          child: Row(
                            mainAxisSize: MainAxisSize.min,
                            children: <Widget>[
                              Icon(
                                Icons.terminal_rounded,
                                size: 18,
                                color: colorScheme.primary,
                              ),
                              const SizedBox(width: 8),
                              Text(
                                'Native EventChannel Stream',
                                style: textTheme.titleSmall,
                              ),
                              clearButton,
                            ],
                          ),
                        );
                      }
                      return Row(
                        children: <Widget>[
                          Icon(
                            Icons.terminal_rounded,
                            size: 18,
                            color: colorScheme.primary,
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                            child: Text(
                              'Native EventChannel Stream',
                              style: textTheme.titleSmall,
                              overflow: TextOverflow.ellipsis,
                              maxLines: 1,
                            ),
                          ),
                          clearButton,
                        ],
                      );
                    },
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
      },
    );
  }
}

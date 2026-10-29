import 'package:material_ui/material_ui.dart';

/// Confirmation dialog displayed when native close (`WM_CLOSE` /
/// `windowShouldClose` / `delete-event`) is intercepted by `preventClose`.
class CloseConfirmationDialog extends StatelessWidget {
  const CloseConfirmationDialog({super.key, required this.onMinimizeInstead});

  final Future<void> Function() onMinimizeInstead;

  static Future<bool?> show(
    BuildContext context, {
    required Future<void> Function() onMinimizeInstead,
  }) {
    return showDialog<bool>(
      context: context,
      builder: (_) =>
          CloseConfirmationDialog(onMinimizeInstead: onMinimizeInstead),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ThemeData dialogTheme = Theme.of(context);
    final ColorScheme dialogColorScheme = dialogTheme.colorScheme;
    final TextTheme dialogTextTheme = dialogTheme.textTheme;

    return AlertDialog(
      title: Text(
        'Intercepted Window Close',
        style: dialogTextTheme.titleLarge,
      ),
      content: Text(
        'Native WM_CLOSE / windowShouldClose / delete-event was intercepted '
        'because preventClose is enabled. Do you want to force-destroy the window?',
        style: dialogTextTheme.bodyMedium,
      ),
      actions: <Widget>[
        TextButton(
          onPressed: () => Navigator.of(context).pop(false),
          child: const Text('Keep Open'),
        ),
        FilledButton.tonal(
          onPressed: () async {
            Navigator.of(context).pop(false);
            await onMinimizeInstead();
          },
          child: const Text('Minimize Instead'),
        ),
        FilledButton(
          style: FilledButton.styleFrom(
            backgroundColor: dialogColorScheme.error,
            foregroundColor: dialogColorScheme.onError,
          ),
          onPressed: () => Navigator.of(context).pop(true),
          child: const Text('Force Destroy'),
        ),
      ],
    );
  }
}

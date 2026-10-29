import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Section 2: Controls `TitleBarStyle` and macOS traffic light offset / visibility.
class TitleBarTrafficLightsCard extends StatelessWidget {
  const TitleBarTrafficLightsCard({
    super.key,
    required this.titleBarStyle,
    required this.trafficLightsX,
    required this.trafficLightsY,
    required this.trafficLightsVisible,
    required this.isMacOS,
    required this.onTitleBarStyleChanged,
    required this.onTrafficLightsXChanged,
    required this.onTrafficLightsYChanged,
    required this.onTrafficLightsVisibleChanged,
  });

  final TitleBarStyle titleBarStyle;
  final double trafficLightsX;
  final double trafficLightsY;
  final bool trafficLightsVisible;
  final bool isMacOS;
  final ValueChanged<TitleBarStyle> onTitleBarStyleChanged;
  final ValueChanged<double> onTrafficLightsXChanged;
  final ValueChanged<double> onTrafficLightsYChanged;
  final ValueChanged<bool> onTrafficLightsVisibleChanged;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final TextTheme textTheme = theme.textTheme;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              '2. Custom Frame & macOS Traffic Lights Offset Controller',
              style: textTheme.titleMedium,
            ),
            const SizedBox(height: 12),
            SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: Row(
                children: <Widget>[
                  Text('TitleBarStyle: ', style: textTheme.bodyMedium),
                  const SizedBox(width: 12),
                  SegmentedButton<TitleBarStyle>(
                    segments: const <ButtonSegment<TitleBarStyle>>[
                      ButtonSegment<TitleBarStyle>(
                        value: TitleBarStyle.normal,
                        label: Text('Normal'),
                      ),
                      ButtonSegment<TitleBarStyle>(
                        value: TitleBarStyle.hidden,
                        label: Text('Hidden'),
                      ),
                      ButtonSegment<TitleBarStyle>(
                        value: TitleBarStyle.immersive,
                        label: Text('Immersive'),
                      ),
                    ],
                    selected: <TitleBarStyle>{titleBarStyle},
                    onSelectionChanged: (Set<TitleBarStyle> selection) =>
                        onTitleBarStyleChanged(selection.first),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 14),
            Row(
              children: <Widget>[
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        'macOS Traffic Lights X Offset: ${trafficLightsX.toStringAsFixed(0)} pt',
                        style: textTheme.labelMedium,
                      ),
                      Slider(
                        value: trafficLightsX,
                        min: 8.0,
                        max: 64.0,
                        divisions: 56,
                        onChanged: onTrafficLightsXChanged,
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        'macOS Traffic Lights Y Offset: ${trafficLightsY.toStringAsFixed(0)} pt',
                        style: textTheme.labelMedium,
                      ),
                      Slider(
                        value: trafficLightsY,
                        min: 6.0,
                        max: 48.0,
                        divisions: 42,
                        onChanged: onTrafficLightsYChanged,
                      ),
                    ],
                  ),
                ),
              ],
            ),
            if (isMacOS)
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                title: Text(
                  'Show macOS Traffic Light Buttons',
                  style: textTheme.bodyMedium,
                ),
                subtitle: Text(
                  'Dynamically toggle close / miniaturize / zoom button visibility',
                  style: textTheme.bodySmall,
                ),
                value: trafficLightsVisible,
                onChanged: onTrafficLightsVisibleChanged,
              ),
          ],
        ),
      ),
    );
  }
}

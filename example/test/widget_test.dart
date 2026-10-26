import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager_example/main.dart';

void main() {
  testWidgets('Verify WindowManagerExampleApp renders studio dashboard', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(const WindowManagerExampleApp());
    await tester.pump();

    expect(find.text('XueHua Native Window Manager Studio'), findsWidgets);
    expect(
      find.text(
        '1. Atomic Batch Configuration (Single-Channel Zero-Jump Presets)',
      ),
      findsOneWidget,
    );
  });
}

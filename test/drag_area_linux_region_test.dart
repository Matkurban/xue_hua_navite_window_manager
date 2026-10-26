import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('DragArea publishes and clears a Linux move region', (
    WidgetTester tester,
  ) async {
    const MethodChannel channel = MethodChannel(
      'xue_hua_navite_window_manager/methods',
    );
    final List<MethodCall> log = <MethodCall>[];
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
          log.add(call);
          return null;
        });
    addTearDown(() {
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(channel, null);
    });

    await tester.pumpWidget(
      const Center(
        child: SizedBox(
          width: 240,
          height: 36,
          child: DragArea(child: SizedBox.expand()),
        ),
      ),
    );

    final MethodCall present = log.singleWhere(
      (MethodCall call) => call.method == 'setLinuxInteractiveRegion',
    );
    final Map<Object?, Object?> args =
        present.arguments as Map<Object?, Object?>;
    expect(args['present'], isTrue);
    expect(args['kind'], 'move');
    expect(args['width'], 240.0);
    expect(args['height'], 36.0);

    await tester.pumpWidget(const SizedBox.shrink());

    expect(
      log.where((MethodCall call) {
        if (call.method != 'setLinuxInteractiveRegion') {
          return false;
        }
        final Map<Object?, Object?> value =
            call.arguments as Map<Object?, Object?>;
        return value['present'] == false;
      }),
      isNotEmpty,
    );
  }, skip: !Platform.isLinux);
}

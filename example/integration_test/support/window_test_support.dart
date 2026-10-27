// Shared helpers for the window manager integration tests.
//
// Every test in `example/integration_test` drives the one real native window
// of the example app, so the helpers here do three jobs:
//
//  * poll: maximize, minimize, fullscreen and focus are performed by the window
//    system, not by the plugin call itself, so tests wait for the state they
//    expect instead of asserting right after the call;
//  * isolate: [windowTest] puts the window back into a known state before each
//    test and restores it afterwards, so tests do not depend on run order;
//  * protect the test process: [requestCloseSafely] refuses to call `close()`
//    unless `preventClose` is on, and nothing here calls `destroy()`, because
//    either would end the process that is running the tests.
import 'dart:async';
import 'dart:math' as math;
import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Fail, instead of skipping, the tests that need a Linux window manager when
/// none is running. Pass `--dart-define=XUE_HUA_REQUIRE_WINDOW_MANAGER=true`.
const bool kRequireWindowManager = bool.fromEnvironment(
  'XUE_HUA_REQUIRE_WINDOW_MANAGER',
);

/// The singleton under test.
XueHuaNaviteWindowManager get windowManager =>
    XueHuaNaviteWindowManager.instance;

const Duration kPollInterval = Duration(milliseconds: 40);

/// Window system round trips (animations included) settle well inside this.
const Duration kStateTimeout = Duration(seconds: 10);

/// Fullscreen on macOS animates between Spaces and is the slowest transition.
const Duration kFullScreenTimeout = Duration(seconds: 20);

const Duration kTestTimeout = Duration(minutes: 3);

const String kBaselineTitle = 'xue_hua integration test';

/// Larger than any display, so "no maximum" can be restored on every platform.
const Size kUnboundedSize = Size(100000, 100000);

/// Smaller than any size a test asks for.
const Size kTinySize = Size(100, 100);

const double kGeometryTolerance = 2.0;

/// Window sizes in logical pixels that fit on the current display.
///
/// Tests never hard-code a size: a 1024x768 virtual display and a 5K monitor
/// both get windows that fit, and the ratios between the sizes stay fixed.
class TestSizes {
  const TestSizes._(this.screen);

  factory TestSizes.forCurrentDisplay() {
    final Iterable<Display> displays = PlatformDispatcher.instance.displays;
    if (displays.isEmpty) {
      return const TestSizes._(Size(1280, 800));
    }
    final Display display = displays.first;
    return TestSizes._(display.size / display.devicePixelRatio);
  }

  /// Logical size of the first display.
  final Size screen;

  /// Where the baseline window is placed (top-left, logical pixels). Far enough
  /// from the origin to stay clear of a macOS menu bar or a Linux panel.
  Offset get origin => const Offset(60, 80);

  Size get base => _fit(const Size(720, 520), 0.55);
  Size get smaller => _scale(base, 0.8);
  Size get larger => _scale(base, 1.2);

  /// A minimum size below [smaller].
  Size get minLimit => _scale(base, 0.7);

  /// A maximum size above [larger] that still fits on the display.
  Size get maxLimit => _scale(base, 1.4);

  /// Room for the whole example Studio dashboard.
  Size get studio => _fit(const Size(1100, 760), 0.85);

  Size _fit(Size preferred, double fraction) => Size(
    math.min(preferred.width, (screen.width * fraction).floorToDouble()),
    math.min(preferred.height, (screen.height * fraction).floorToDouble()),
  );

  static Size _scale(Size size, double factor) => Size(
    (size.width * factor).floorToDouble(),
    (size.height * factor).floorToDouble(),
  );
}

TestSizes get sizes => TestSizes.forCurrentDisplay();

// MARK: - Waiting

/// Reads the native state until [predicate] holds, or fails with the last state.
Future<WindowStateSnapshot> waitForState(
  bool Function(WindowStateSnapshot state) predicate, {
  required String reason,
  Duration timeout = kStateTimeout,
}) async {
  final Stopwatch watch = Stopwatch()..start();
  WindowStateSnapshot state = await windowManager.getState();
  while (!predicate(state)) {
    if (watch.elapsed > timeout) {
      throw TestFailure(
        'Timed out after ${timeout.inSeconds}s waiting for $reason.\n'
        'Last state: $state',
      );
    }
    await Future<void>.delayed(kPollInterval);
    state = await windowManager.getState();
  }
  return state;
}

/// Like [waitForState] but returns false on timeout. For cleanup and probes.
Future<bool> tryWaitForState(
  bool Function(WindowStateSnapshot state) predicate, {
  Duration timeout = kStateTimeout,
}) async {
  try {
    await waitForState(predicate, reason: 'a best-effort condition', timeout: timeout);
    return true;
  } on TestFailure {
    return false;
  }
}

/// Reads the native bounds until [predicate] holds, or fails with the last value.
Future<Rect> waitForBounds(
  bool Function(Rect bounds) predicate, {
  required String reason,
  Duration timeout = kStateTimeout,
}) async {
  final Stopwatch watch = Stopwatch()..start();
  Rect bounds = await windowManager.getBounds();
  while (!predicate(bounds)) {
    if (watch.elapsed > timeout) {
      throw TestFailure(
        'Timed out after ${timeout.inSeconds}s waiting for $reason.\n'
        'Last bounds: $bounds',
      );
    }
    await Future<void>.delayed(kPollInterval);
    bounds = await windowManager.getBounds();
  }
  return bounds;
}

/// Waits until the window size is [expected], within [tolerance].
Future<Rect> waitForSize(
  Size expected, {
  double tolerance = kGeometryTolerance,
  String? reason,
}) {
  return waitForBounds(
    (Rect bounds) => sizeIsNear(bounds.size, expected, tolerance),
    reason: reason ?? 'the window to be ${_describe(expected)}',
  );
}

/// Waits until the window origin is [expected], within [tolerance].
Future<Rect> waitForPosition(
  Offset expected, {
  double tolerance = kGeometryTolerance,
  String? reason,
}) {
  return waitForBounds(
    (Rect bounds) => offsetIsNear(bounds.topLeft, expected, tolerance),
    reason: reason ?? 'the window to move to $expected',
  );
}

/// Waits until two reads, [settle] apart, return the same bounds. Window
/// systems that apply moves asynchronously report intermediate values.
Future<Rect> waitForStableBounds({
  Duration settle = const Duration(milliseconds: 150),
  Duration timeout = kStateTimeout,
}) async {
  final Stopwatch watch = Stopwatch()..start();
  Rect previous = await windowManager.getBounds();
  while (true) {
    await Future<void>.delayed(settle);
    final Rect current = await windowManager.getBounds();
    if (current == previous) {
      return current;
    }
    if (watch.elapsed > timeout) {
      throw TestFailure('Window bounds kept changing: $previous -> $current');
    }
    previous = current;
  }
}

bool sizeIsNear(Size actual, Size expected, [double tolerance = kGeometryTolerance]) {
  return (actual.width - expected.width).abs() <= tolerance &&
      (actual.height - expected.height).abs() <= tolerance;
}

bool offsetIsNear(Offset actual, Offset expected, [double tolerance = kGeometryTolerance]) {
  return (actual.dx - expected.dx).abs() <= tolerance &&
      (actual.dy - expected.dy).abs() <= tolerance;
}

String _describe(Size size) => '${size.width}x${size.height}';

/// Expects [actual] to be [expected] within [tolerance] logical pixels.
void expectSizeNear(
  Size actual,
  Size expected, {
  double tolerance = kGeometryTolerance,
  String? reason,
}) {
  expect(
    sizeIsNear(actual, expected, tolerance),
    isTrue,
    reason: '${reason ?? 'size'}: expected ~${_describe(expected)} (+/-$tolerance) but was ${_describe(actual)}',
  );
}

/// Expects [actual] to be [expected] within [tolerance] logical pixels.
void expectOffsetNear(
  Offset actual,
  Offset expected, {
  double tolerance = kGeometryTolerance,
  String? reason,
}) {
  expect(
    offsetIsNear(actual, expected, tolerance),
    isTrue,
    reason: '${reason ?? 'position'}: expected ~$expected (+/-$tolerance) but was $actual',
  );
}

// MARK: - Window system capabilities

bool? _windowSystemHonorsStateChanges;

/// Whether maximize, minimize, fullscreen and focus can take effect.
///
/// macOS and Windows always can. On Linux the window manager performs them, and
/// a bare Xvfb (the setup Flutter's documentation uses for CI) has none, so the
/// answer is measured once: maximize the window and see whether it happens.
Future<bool> windowSystemHonorsStateChanges() async {
  if (!isLinux) {
    return true;
  }
  return _windowSystemHonorsStateChanges ??= await _probeWindowManager();
}

Future<bool> _probeWindowManager() async {
  await windowManager.show();
  await windowManager.maximize();
  final bool honored = await tryWaitForState(
    (WindowStateSnapshot state) => state.isMaximized,
    timeout: const Duration(seconds: 5),
  );
  await windowManager.unmaximize();
  await tryWaitForState(
    (WindowStateSnapshot state) => !state.isMaximized,
    timeout: const Duration(seconds: 5),
  );
  return honored;
}

/// Call at the top of a test that needs the window system to act on maximize,
/// minimize, fullscreen or focus. Returns false after marking the test skipped.
Future<bool> ensureWindowSystemHonorsStateChanges() async {
  if (await windowSystemHonorsStateChanges()) {
    return true;
  }
  const String why =
      'This X display has no window manager (bare Xvfb), so maximize, '
      'minimize, fullscreen and focus cannot take effect. Run a window manager '
      '(for example openbox) next to Xvfb, or use a desktop session.';
  if (kRequireWindowManager) {
    throw TestFailure(why);
  }
  markTestSkipped(why);
  return false;
}

/// Wayland clients cannot place or read the absolute window position, so the
/// plugin reports `isPositionAuthoritative == false` there.
Future<bool> positionsAreAuthoritative() async {
  return (await windowManager.getState()).isPositionAuthoritative;
}

// MARK: - Baseline

/// Leaves fullscreen, minimized and maximized states, best effort.
Future<void> _leaveTransientStates() async {
  WindowStateSnapshot state = await windowManager.getState();
  if (state.isFullScreen) {
    await windowManager.setFullScreen(false);
    await tryWaitForState(
      (WindowStateSnapshot s) => !s.isFullScreen,
      timeout: kFullScreenTimeout,
    );
    state = await windowManager.getState();
  }
  if (state.isMinimized) {
    await windowManager.restore();
    await tryWaitForState((WindowStateSnapshot s) => !s.isMinimized);
    state = await windowManager.getState();
  }
  if (state.isMaximized) {
    await windowManager.unmaximize();
    await tryWaitForState((WindowStateSnapshot s) => !s.isMaximized);
  }
}

/// The configuration every test starts from.
WindowConfig baselineConfig() {
  final TestSizes metrics = sizes;
  return WindowConfig(
    size: metrics.base,
    minSize: kTinySize,
    maxSize: kUnboundedSize,
    position: metrics.origin,
    title: kBaselineTitle,
    titleBarStyle: TitleBarStyle.normal,
    trafficLightsOffset: const Offset(16, 14),
    trafficLightsVisible: true,
    backgroundColor: const Color(0xFF202020),
    alwaysOnTop: false,
    resizable: true,
    preventClose: true,
    hasShadow: true,
    // The tests reveal the window explicitly. With showWhenReady the controller
    // would call markReadyToShow after the next frame, which can land in the
    // middle of a later test (for example right after hide()).
    showWhenReady: false,
  );
}

/// Puts the window into the baseline state: visible, normal, unconstrained,
/// `preventClose` on. Returns the resulting snapshot.
Future<WindowStateSnapshot> resetWindow() async {
  await windowManager.ensureInitialized();
  await _leaveTransientStates();

  final TestSizes metrics = sizes;
  await windowManager.configureWindow(baselineConfig());
  await windowManager.setOpacity(1.0);
  await windowManager.show();
  await waitForState(
    (WindowStateSnapshot s) => s.isVisible && !s.isMinimized,
    reason: 'the baseline window to become visible',
  );

  // Showing a window can make the window system place it again. Apply the
  // geometry once more now that it is on screen.
  await windowManager.setBounds(
    Rect.fromLTWH(
      metrics.origin.dx,
      metrics.origin.dy,
      metrics.base.width,
      metrics.base.height,
    ),
  );
  return waitForState(
    (WindowStateSnapshot s) =>
        sizeIsNear(s.bounds.size, metrics.base) &&
        !s.isMaximized &&
        !s.isMinimized &&
        !s.isFullScreen,
    reason: 'the baseline window geometry',
  );
}

/// Best-effort cleanup that runs after every [windowTest], pass or fail.
Future<void> restoreWindowSafely() async {
  try {
    await windowManager.setPreventClose(true);
    await windowManager.setOpacity(1.0);
    await windowManager.setAlwaysOnTop(false);
    await _leaveTransientStates();
    await windowManager.setMinimumSize(kTinySize);
    await windowManager.setMaximumSize(kUnboundedSize);
    await windowManager.show();
  } catch (error) {
    debugPrint('restoreWindowSafely failed: $error');
  }
}

/// A `testWidgets` that starts from [resetWindow] and always restores the
/// window, so a failing test cannot leave the next one minimized or hidden.
void windowTest(
  String description,
  Future<void> Function(WidgetTester tester) body, {
  bool skip = false,
  Timeout timeout = const Timeout(kTestTimeout),
}) {
  testWidgets(
    description,
    (WidgetTester tester) async {
      await resetWindow();
      addTearDown(restoreWindowSafely);
      await body(tester);
    },
    skip: skip,
    timeout: timeout,
  );
}

/// Calls `close()` only when `preventClose` is on. With it off, `close()` would
/// destroy the window and end the process that is running the tests.
Future<void> requestCloseSafely() async {
  final WindowStateSnapshot state = await windowManager.getState();
  if (!state.isPreventClose) {
    throw TestFailure(
      'Refusing to call close(): preventClose is off, so the window and the '
      'test process would be destroyed.',
    );
  }
  await windowManager.close();
}

// MARK: - Events

/// Records native window events from the raw stream and from a [WindowListener].
class WindowEventRecorder with WindowListener {
  WindowEventRecorder._();

  /// Events from `XueHuaNaviteWindowManager.events`, oldest first.
  final List<WindowEvent> events = <WindowEvent>[];

  /// Events delivered through [onWindowEvent].
  final List<WindowEvent> listenerEvents = <WindowEvent>[];

  /// Names of the typed callbacks that ran, oldest first.
  final List<String> callbacks = <String>[];

  /// Sizes passed to [onWindowResize].
  final List<Size> resizeSizes = <Size>[];

  /// Positions passed to [onWindowMove].
  final List<Offset> movePositions = <Offset>[];

  /// Values passed to [onSnapButtonHovered].
  final List<bool> snapHovers = <bool>[];

  StreamSubscription<WindowEvent>? _subscription;

  /// Subscribes to the stream and registers as a listener. Waits briefly so the
  /// native event channel is listening before the test triggers events.
  static Future<WindowEventRecorder> attach() async {
    final WindowEventRecorder recorder = WindowEventRecorder._();
    await windowManager.ensureInitialized();
    recorder._subscription = windowManager.events.listen(recorder.events.add);
    windowManager.addListener(recorder);
    await Future<void>.delayed(const Duration(milliseconds: 200));
    return recorder;
  }

  Future<void> dispose() async {
    windowManager.removeListener(this);
    await _subscription?.cancel();
    _subscription = null;
  }

  void clear() {
    events.clear();
    listenerEvents.clear();
    callbacks.clear();
    resizeSizes.clear();
    movePositions.clear();
    snapHovers.clear();
  }

  Iterable<WindowEventType> get types => events.map((WindowEvent e) => e.type);

  bool has(WindowEventType type) => types.contains(type);

  /// Waits for an event of [type] (optionally matching [where]) on the stream.
  Future<WindowEvent> waitFor(
    WindowEventType type, {
    bool Function(WindowEvent event)? where,
    Duration timeout = kStateTimeout,
  }) async {
    final Stopwatch watch = Stopwatch()..start();
    while (true) {
      for (final WindowEvent event in List<WindowEvent>.of(events)) {
        if (event.type == type && (where == null || where(event))) {
          return event;
        }
      }
      if (watch.elapsed > timeout) {
        throw TestFailure(
          'Timed out after ${timeout.inSeconds}s waiting for a '
          '${type.wireValue} event. Received: '
          '${events.map((WindowEvent e) => e.type.wireValue).toList()}',
        );
      }
      await Future<void>.delayed(const Duration(milliseconds: 25));
    }
  }

  /// Waits until the typed callback [name] has run.
  Future<void> waitForCallback(
    String name, {
    Duration timeout = kStateTimeout,
  }) async {
    final Stopwatch watch = Stopwatch()..start();
    while (!callbacks.contains(name)) {
      if (watch.elapsed > timeout) {
        throw TestFailure(
          'Timed out after ${timeout.inSeconds}s waiting for the $name '
          'callback. Callbacks so far: $callbacks',
        );
      }
      await Future<void>.delayed(const Duration(milliseconds: 25));
    }
  }

  @override
  void onWindowEvent(WindowEvent event) => listenerEvents.add(event);

  @override
  void onWindowResize(Size size) => resizeSizes.add(size);

  @override
  void onWindowMove(Offset position) => movePositions.add(position);

  @override
  void onWindowMaximize() => callbacks.add('maximize');

  @override
  void onWindowUnmaximize() => callbacks.add('unmaximize');

  @override
  void onWindowMinimize() => callbacks.add('minimize');

  @override
  void onWindowRestore() => callbacks.add('restore');

  @override
  void onWindowEnterFullScreen() => callbacks.add('enterFullScreen');

  @override
  void onWindowLeaveFullScreen() => callbacks.add('leaveFullScreen');

  @override
  void onWindowFocus() => callbacks.add('focus');

  @override
  void onWindowBlur() => callbacks.add('blur');

  @override
  void onWindowCloseRequested() => callbacks.add('closeRequested');

  @override
  void onSnapButtonHovered(bool isHovered) => snapHovers.add(isHovered);

  @override
  void onSnapButtonClicked() => callbacks.add('snapButtonClicked');
}

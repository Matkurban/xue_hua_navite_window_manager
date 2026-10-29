import 'dart:collection';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// State and action controller for the Window Studio workbench.
///
/// Subscribes to native [WindowListener] events, synchronizes live window
/// geometry/flags and launch-at-startup state, and exposes semantic commands
/// for all Studio dashboard cards.
class WindowStudioController extends ChangeNotifier with WindowListener {
  WindowStudioController({
    XueHuaNaviteWindowManager? windowManager,
    this.onCloseIntercepted,
  }) : _windowManager = windowManager ?? XueHuaNaviteWindowManager.instance;

  final XueHuaNaviteWindowManager _windowManager;

  /// Invoked when `preventClose` intercepts a native window close request.
  final VoidCallback? onCloseIntercepted;

  bool _disposed = false;

  TitleBarStyle _titleBarStyle = TitleBarStyle.hidden;
  double _trafficLightsX = 16.0;
  double _trafficLightsY = 14.0;
  bool _trafficLightsVisible = true;
  bool _alwaysOnTop = false;
  bool _resizable = true;
  bool _preventClose = true;
  bool _hasShadow = true;
  double _opacity = 1.0;
  Color? _selectedBackgroundColor;

  Rect _currentBounds = const Rect.fromLTWH(0, 0, 1240, 820);
  bool _isMaximized = false;
  bool _isMinimized = false;
  bool _isFullScreen = false;
  bool _isFocused = true;
  bool _snapButtonHovered = false;

  LaunchAtStartupState _startupState = const LaunchAtStartupState(
    isSupported: false,
    isEnabled: false,
    status: LaunchAtStartupStatus.unsupported,
    backend: LaunchAtStartupBackend.none,
    identifier: '',
  );
  bool _startupPassMinimizedArg = false;
  bool _startupBusy = false;

  final List<String> _eventLogs = <String>[];

  // MARK: - Read-Only State Getters

  TitleBarStyle get titleBarStyle => _titleBarStyle;
  double get trafficLightsX => _trafficLightsX;
  double get trafficLightsY => _trafficLightsY;
  bool get trafficLightsVisible => _trafficLightsVisible;
  bool get alwaysOnTop => _alwaysOnTop;
  bool get resizable => _resizable;
  bool get preventClose => _preventClose;
  bool get hasShadow => _hasShadow;
  double get opacity => _opacity;
  Color? get selectedBackgroundColor => _selectedBackgroundColor;

  Rect get currentBounds => _currentBounds;
  bool get isMaximized => _isMaximized;
  bool get isMinimized => _isMinimized;
  bool get isFullScreen => _isFullScreen;
  bool get isFocused => _isFocused;
  bool get snapButtonHovered => _snapButtonHovered;

  LaunchAtStartupState get startupState => _startupState;
  bool get startupPassMinimizedArg => _startupPassMinimizedArg;
  bool get startupBusy => _startupBusy;

  UnmodifiableListView<String> get eventLogs =>
      UnmodifiableListView<String>(_eventLogs);

  bool get isMacOS => !kIsWeb && Platform.isMacOS;

  LaunchAtStartupConfig get currentStartupConfig => LaunchAtStartupConfig(
    arguments: _startupPassMinimizedArg
        ? const <String>['--minimized']
        : const <String>[],
  );

  // MARK: - Lifecycle

  void init() {
    _windowManager.addListener(this);
    refreshWindowState();
    refreshLaunchAtStartupState();
  }

  @override
  void dispose() {
    _disposed = true;
    _windowManager.removeListener(this);
    super.dispose();
  }

  void _notify() {
    if (!_disposed) {
      notifyListeners();
    }
  }

  // MARK: - Telemetry & Logging

  Future<void> refreshWindowState() async {
    try {
      final WindowStateSnapshot snapshot = await _windowManager.getState();
      if (_disposed) return;
      _currentBounds = snapshot.bounds;
      _isMaximized = snapshot.isMaximized;
      _isMinimized = snapshot.isMinimized;
      _isFullScreen = snapshot.isFullScreen;
      _isFocused = snapshot.isFocused;
      _alwaysOnTop = snapshot.isAlwaysOnTop;
      _resizable = snapshot.isResizable;
      _preventClose = snapshot.isPreventClose;
      _notify();
    } catch (_) {
      // Ignored in headless widget tests.
    }
  }

  void appendLog(String message) {
    if (_disposed) return;
    final String timestamp = DateTime.now()
        .toIso8601String()
        .split('T')
        .last
        .substring(0, 12);
    _eventLogs.insert(0, '[$timestamp] $message');
    if (_eventLogs.length > 60) {
      _eventLogs.removeLast();
    }
    _notify();
  }

  void clearLogs() {
    if (_disposed) return;
    _eventLogs.clear();
    _notify();
  }

  // MARK: - 1. Atomic Batch Presets

  Future<void> applyStudioDefaultPreset(Color backgroundColor) async {
    final WindowStateSnapshot snap = await XueHuaNaviteWindowManager.configure(
      WindowConfig(
        size: const Size(1280, 820),
        minSize: const Size(860, 580),
        center: true,
        title: 'Studio Default (1280×820 Centered)',
        titleBarStyle: TitleBarStyle.hidden,
        trafficLightsOffset: const Offset(16, 14),
        trafficLightsVisible: true,
        backgroundColor: backgroundColor,
        preventClose: true,
        showWhenReady: true,
      ),
    );
    if (_disposed) return;
    _selectedBackgroundColor = backgroundColor;
    _titleBarStyle = TitleBarStyle.hidden;
    _trafficLightsX = 16;
    _trafficLightsY = 14;
    _trafficLightsVisible = true;
    _currentBounds = snap.bounds;
    appendLog('Atomic configure -> Studio Default (1280x820)');
  }

  Future<void> applyCompactImmersivePreset(Color backgroundColor) async {
    final WindowStateSnapshot snap = await XueHuaNaviteWindowManager.configure(
      WindowConfig(
        size: const Size(980, 660),
        minSize: const Size(720, 480),
        center: true,
        title: 'Compact Immersive Mode',
        titleBarStyle: TitleBarStyle.immersive,
        trafficLightsOffset: const Offset(22, 18),
        trafficLightsVisible: true,
        backgroundColor: backgroundColor,
        alwaysOnTop: false,
        showWhenReady: true,
      ),
    );
    if (_disposed) return;
    _selectedBackgroundColor = backgroundColor;
    _titleBarStyle = TitleBarStyle.immersive;
    _trafficLightsX = 22;
    _trafficLightsY = 18;
    _currentBounds = snap.bounds;
    appendLog('Atomic configure -> Compact Immersive (980x660)');
  }

  Future<void> applySystemTitleBarPreset() async {
    final WindowStateSnapshot snap = await XueHuaNaviteWindowManager.configure(
      const WindowConfig(
        size: Size(1100, 720),
        center: true,
        title: 'Native System TitleBar',
        titleBarStyle: TitleBarStyle.normal,
        showWhenReady: true,
      ),
    );
    if (_disposed) return;
    _titleBarStyle = TitleBarStyle.normal;
    _currentBounds = snap.bounds;
    appendLog('Atomic configure -> System Normal (1100x720)');
  }

  // MARK: - 2. Title Bar & Traffic Lights

  Future<void> updateTitleBarStyle(TitleBarStyle style) async {
    _titleBarStyle = style;
    _notify();
    await _windowManager.setTitleBarStyle(
      style,
      trafficLightsOffset: Offset(_trafficLightsX, _trafficLightsY),
      trafficLightsVisible: _trafficLightsVisible,
    );
    appendLog('setTitleBarStyle(${style.wireValue})');
  }

  Future<void> updateTrafficLightsX(double value) {
    _trafficLightsX = value;
    _notify();
    return _windowManager.setTrafficLightsOffset(
      Offset(_trafficLightsX, _trafficLightsY),
    );
  }

  Future<void> updateTrafficLightsY(double value) {
    _trafficLightsY = value;
    _notify();
    return _windowManager.setTrafficLightsOffset(
      Offset(_trafficLightsX, _trafficLightsY),
    );
  }

  Future<void> updateTrafficLightsVisible(bool visible) {
    _trafficLightsVisible = visible;
    _notify();
    return _windowManager.setTrafficLightsVisible(visible);
  }

  // MARK: - 3. Behavior & Appearance

  Future<void> updatePreventClose(bool value) async {
    _preventClose = value;
    _notify();
    await _windowManager.setPreventClose(value);
    appendLog('setPreventClose($value)');
  }

  Future<void> updateAlwaysOnTop(bool value) async {
    _alwaysOnTop = value;
    _notify();
    await _windowManager.setAlwaysOnTop(value);
    appendLog('setAlwaysOnTop($value)');
  }

  Future<void> updateResizable(bool value) async {
    _resizable = value;
    _notify();
    await _windowManager.setResizable(value);
    appendLog('setResizable($value)');
  }

  Future<void> updateHasShadow(bool value) async {
    _hasShadow = value;
    _notify();
    await _windowManager.setHasShadow(value);
    appendLog('setHasShadow($value)');
  }

  Future<void> updateOpacity(double value) {
    _opacity = value;
    _notify();
    return _windowManager.setOpacity(value);
  }

  Future<void> selectBackgroundColor(String label, Color color) async {
    _selectedBackgroundColor = color;
    _notify();
    await _windowManager.setBackgroundColor(color);
    appendLog('setBackgroundColor($label)');
  }

  // MARK: - 4. Drag & Lifecycle Actions

  void onCustomDragAreaStart() => appendLog('startDragging() via DragArea');

  Future<void> centerDefaultSize() =>
      _windowManager.center(size: const Size(1240, 820), animate: true);

  Future<void> centerCurrentSize() => _windowManager.center(animate: true);

  Future<void> toggleFullScreen() =>
      _windowManager.setFullScreen(!_isFullScreen);

  Future<void> minimize() => _windowManager.minimize();

  Future<void> toggleMaximize() => _windowManager.toggleMaximize();

  Future<void> requestClose() => _windowManager.close();

  Future<void> forceDestroy() => _windowManager.destroy();

  // MARK: - 5. Launch at Startup

  Future<void> refreshLaunchAtStartupState() async {
    try {
      final LaunchAtStartupState state = await _windowManager
          .getLaunchAtStartupState(config: currentStartupConfig);
      if (_disposed) return;
      _startupState = state;
      _notify();
    } catch (_) {
      // Ignored in headless widget tests.
    }
  }

  Future<void> toggleLaunchAtStartup(bool enabled) async {
    if (_startupBusy) return;
    _startupBusy = true;
    _notify();
    try {
      final LaunchAtStartupState updated = await _windowManager
          .setLaunchAtStartup(enabled, config: currentStartupConfig);
      if (_disposed) return;
      _startupState = updated;
      appendLog(
        'setLaunchAtStartup($enabled) -> ${updated.status.wireValue} (${updated.backend.wireValue})',
      );
    } catch (error) {
      appendLog('setLaunchAtStartup($enabled) error: $error');
    } finally {
      if (!_disposed) {
        _startupBusy = false;
        _notify();
      }
    }
  }

  void setStartupPassMinimizedArg(bool value) {
    _startupPassMinimizedArg = value;
    _notify();
    refreshLaunchAtStartupState();
  }

  Future<void> openLaunchAtStartupSettings() async {
    final bool opened = await _windowManager.openLaunchAtStartupSettings();
    appendLog('openLaunchAtStartupSettings() -> $opened');
  }

  // MARK: - WindowListener Callbacks

  @override
  void onWindowResize(Size size) {
    _currentBounds = Rect.fromLTWH(
      _currentBounds.left,
      _currentBounds.top,
      size.width,
      size.height,
    );
    appendLog(
      'resized -> ${size.width.toStringAsFixed(0)} x ${size.height.toStringAsFixed(0)}',
    );
  }

  @override
  void onWindowMove(Offset position) {
    _currentBounds = Rect.fromLTWH(
      position.dx,
      position.dy,
      _currentBounds.width,
      _currentBounds.height,
    );
    appendLog(
      'moved -> (${position.dx.toStringAsFixed(0)}, ${position.dy.toStringAsFixed(0)})',
    );
  }

  @override
  void onWindowMaximize() {
    _isMaximized = true;
    appendLog('maximized');
  }

  @override
  void onWindowUnmaximize() {
    _isMaximized = false;
    appendLog('unmaximized');
  }

  @override
  void onWindowMinimize() {
    _isMinimized = true;
    appendLog('minimized');
  }

  @override
  void onWindowRestore() {
    _isMinimized = false;
    appendLog('restored');
  }

  @override
  void onWindowEnterFullScreen() {
    _isFullScreen = true;
    appendLog('enteredFullScreen');
  }

  @override
  void onWindowLeaveFullScreen() {
    _isFullScreen = false;
    appendLog('exitedFullScreen');
  }

  @override
  void onWindowFocus() {
    _isFocused = true;
    appendLog('focused');
  }

  @override
  void onWindowBlur() {
    _isFocused = false;
    appendLog('blurred');
  }

  @override
  void onSnapButtonHovered(bool isHovered) {
    _snapButtonHovered = isHovered;
    appendLog(isHovered ? 'snapButtonHovered (HTMAXBUTTON)' : 'snapButtonLeft');
  }

  @override
  void onSnapButtonClicked() {
    appendLog('snapButtonClicked (HTMAXBUTTON)');
  }

  @override
  void onWindowCloseRequested() {
    appendLog('closeRequested (intercepted by preventClose)');
    onCloseIntercepted?.call();
  }
}

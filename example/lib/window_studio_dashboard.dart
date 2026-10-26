import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:material_ui/material_ui.dart';
import 'package:xue_hua_navite_window_manager/xue_hua_navite_window_manager.dart';

/// Interactive workbench demonstrating atomic configuration, custom frameless caption,
/// macOS Traffic Lights offset slider, Windows 11 Snap Layouts (`HTMAXBUTTON`),
/// 8-edge programmatic resizing, and real-time `WindowListener` event logs.
class WindowStudioDashboard extends StatefulWidget {
  const WindowStudioDashboard({super.key});

  @override
  State<WindowStudioDashboard> createState() => _WindowStudioDashboardState();
}

class _WindowStudioDashboardState extends State<WindowStudioDashboard>
    with WindowListener {
  final XueHuaNaviteWindowManager _windowManager =
      XueHuaNaviteWindowManager.instance;

  TitleBarStyle _titleBarStyle = TitleBarStyle.hidden;
  double _trafficLightsX = 16.0;
  double _trafficLightsY = 14.0;
  bool _trafficLightsVisible = true;
  bool _alwaysOnTop = false;
  bool _resizable = true;
  bool _preventClose = true;
  bool _hasShadow = true;
  double _opacity = 1.0;
  Color _bgColor = const Color(0xFF141622);

  Rect _currentBounds = const Rect.fromLTWH(0, 0, 1240, 820);
  bool _isMaximized = false;
  bool _isMinimized = false;
  bool _isFullScreen = false;
  bool _isFocused = true;
  bool _snapButtonHovered = false;

  final List<String> _eventLogs = <String>[];

  bool get _isMacOS => !kIsWeb && Platform.isMacOS;

  @override
  void initState() {
    super.initState();
    _windowManager.addListener(this);
    _refreshWindowState();
  }

  @override
  void dispose() {
    _windowManager.removeListener(this);
    super.dispose();
  }

  Future<void> _refreshWindowState() async {
    try {
      final WindowStateSnapshot snapshot = await _windowManager.getState();
      if (!mounted) return;
      setState(() {
        _currentBounds = snapshot.bounds;
        _isMaximized = snapshot.isMaximized;
        _isMinimized = snapshot.isMinimized;
        _isFullScreen = snapshot.isFullScreen;
        _isFocused = snapshot.isFocused;
        _alwaysOnTop = snapshot.isAlwaysOnTop;
        _resizable = snapshot.isResizable;
        _preventClose = snapshot.isPreventClose;
      });
    } catch (_) {
      // Ignored in widget tests.
    }
  }

  void _appendLog(String message) {
    if (!mounted) return;
    final String timestamp = DateTime.now()
        .toIso8601String()
        .split('T')
        .last
        .substring(0, 12);
    setState(() {
      _eventLogs.insert(0, '[$timestamp] $message');
      if (_eventLogs.length > 60) {
        _eventLogs.removeLast();
      }
    });
  }

  // MARK: - WindowListener Callbacks

  @override
  void onWindowResize(Size size) {
    setState(() {
      _currentBounds = Rect.fromLTWH(
        _currentBounds.left,
        _currentBounds.top,
        size.width,
        size.height,
      );
    });
    _appendLog(
      'resized -> ${size.width.toStringAsFixed(0)} x ${size.height.toStringAsFixed(0)}',
    );
  }

  @override
  void onWindowMove(Offset position) {
    setState(() {
      _currentBounds = Rect.fromLTWH(
        position.dx,
        position.dy,
        _currentBounds.width,
        _currentBounds.height,
      );
    });
    _appendLog(
      'moved -> (${position.dx.toStringAsFixed(0)}, ${position.dy.toStringAsFixed(0)})',
    );
  }

  @override
  void onWindowMaximize() {
    setState(() => _isMaximized = true);
    _appendLog('maximized');
  }

  @override
  void onWindowUnmaximize() {
    setState(() => _isMaximized = false);
    _appendLog('unmaximized');
  }

  @override
  void onWindowMinimize() {
    setState(() => _isMinimized = true);
    _appendLog('minimized');
  }

  @override
  void onWindowRestore() {
    setState(() => _isMinimized = false);
    _appendLog('restored');
  }

  @override
  void onWindowEnterFullScreen() {
    setState(() => _isFullScreen = true);
    _appendLog('enteredFullScreen');
  }

  @override
  void onWindowLeaveFullScreen() {
    setState(() => _isFullScreen = false);
    _appendLog('exitedFullScreen');
  }

  @override
  void onWindowFocus() {
    setState(() => _isFocused = true);
    _appendLog('focused');
  }

  @override
  void onWindowBlur() {
    setState(() => _isFocused = false);
    _appendLog('blurred');
  }

  @override
  void onSnapButtonHovered(bool isHovered) {
    setState(() => _snapButtonHovered = isHovered);
    _appendLog(
      isHovered ? 'snapButtonHovered (HTMAXBUTTON)' : 'snapButtonLeft',
    );
  }

  @override
  void onSnapButtonClicked() {
    _appendLog('snapButtonClicked (HTMAXBUTTON)');
  }

  @override
  void onWindowCloseRequested() {
    _appendLog('closeRequested (intercepted by preventClose)');
    _showCloseConfirmationDialog();
  }

  Future<void> _showCloseConfirmationDialog() async {
    if (!mounted) return;
    final bool? shouldDestroy = await showDialog<bool>(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Intercepted Window Close'),
          content: const Text(
            'Native WM_CLOSE / windowShouldClose / delete-event was intercepted '
            'because preventClose is enabled. Do you want to force-destroy the window?',
          ),
          actions: <Widget>[
            TextButton(
              onPressed: () => Navigator.of(context).pop(false),
              child: const Text('Keep Open'),
            ),
            FilledButton.tonal(
              onPressed: () async {
                Navigator.of(context).pop(false);
                await _windowManager.minimize();
              },
              child: const Text('Minimize Instead'),
            ),
            FilledButton(
              style: FilledButton.styleFrom(
                backgroundColor: const Color(0xFFEF4444),
              ),
              onPressed: () => Navigator.of(context).pop(true),
              child: const Text('Force Destroy'),
            ),
          ],
        );
      },
    );

    if (shouldDestroy == true) {
      await _windowManager.destroy();
    }
  }

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    return Scaffold(
      backgroundColor: _bgColor,
      body: WindowResizeEdgeArea(
        enabled: _resizable && !_isMaximized && !_isFullScreen,
        child: Column(
          children: <Widget>[
            // Custom Frameless Title Bar with DragArea & Windows 11 SnapLayoutButton
            WindowCaption(
              height: 44,
              macosTrafficLightsReservedWidth:
                  _trafficLightsVisible && !_isFullScreen
                  ? (_trafficLightsX + 68.0)
                  : 12.0,
              backgroundColor: theme.scaffoldBackgroundColor,
              foregroundColor: theme.colorScheme.secondary,
              title: Row(
                children: <Widget>[
                  const Icon(
                    Icons.desktop_windows_rounded,
                    size: 18,
                    color: Color(0xFF818CF8),
                  ),
                  const SizedBox(width: 10),
                  const Text('XueHua Native Window Manager Studio'),
                  const SizedBox(width: 12),
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 2,
                    ),
                    decoration: BoxDecoration(
                      color: _isFocused
                          ? const Color(0xFF6366F1).withValues(alpha: 0.2)
                          : Colors.white10,
                      borderRadius: BorderRadius.circular(6),
                    ),
                    child: Text(
                      _isFocused ? 'ACTIVE' : 'BLURRED',
                      style: TextStyle(
                        fontSize: 10,
                        fontWeight: FontWeight.w700,
                        color: _isFocused
                            ? const Color(0xFFA5B4FC)
                            : Colors.white54,
                      ),
                    ),
                  ),
                ],
              ),
              actions: <Widget>[
                IconButton(
                  tooltip: 'Center Window (1240x820)',
                  icon: const Icon(Icons.filter_center_focus, size: 18),
                  onPressed: () => _windowManager.center(
                    size: const Size(1240, 820),
                    animate: true,
                  ),
                ),
                IconButton(
                  tooltip: 'Toggle FullScreen',
                  icon: Icon(
                    _isFullScreen
                        ? Icons.fullscreen_exit_rounded
                        : Icons.fullscreen_rounded,
                    size: 20,
                  ),
                  onPressed: () => _windowManager.setFullScreen(!_isFullScreen),
                ),
                const SizedBox(width: 4),
              ],
            ),
            const Divider(height: 1, color: Colors.white12),
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: <Widget>[
                  Expanded(
                    flex: 7,
                    child: SingleChildScrollView(
                      padding: const EdgeInsets.all(20),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: <Widget>[
                          _buildStatusTelemetryCard(),
                          const SizedBox(height: 16),
                          _buildAtomicPresetsCard(),
                          const SizedBox(height: 16),
                          _buildTitleBarAndTrafficLightsCard(),
                          const SizedBox(height: 16),
                          _buildBehaviorAndAppearanceCard(),
                          const SizedBox(height: 16),
                          _buildDragAndEdgeResizeCard(),
                        ],
                      ),
                    ),
                  ),
                  const VerticalDivider(width: 1, color: Colors.white12),
                  Expanded(flex: 4, child: _buildEventStreamPanel()),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusTelemetryCard() {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                const Icon(Icons.speed_rounded, color: Color(0xFF818CF8)),
                const SizedBox(width: 8),
                const Text(
                  'Live Window Geometry & State Telemetry',
                  style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
                ),
                const Spacer(),
                TextButton.icon(
                  onPressed: _refreshWindowState,
                  icon: const Icon(Icons.refresh, size: 16),
                  label: const Text('Sync State'),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                _buildMetricBadge(
                  'Size (DIP)',
                  '${_currentBounds.width.toStringAsFixed(0)} × ${_currentBounds.height.toStringAsFixed(0)}',
                ),
                _buildMetricBadge(
                  'Position (DIP)',
                  '(${_currentBounds.left.toStringAsFixed(0)}, ${_currentBounds.top.toStringAsFixed(0)})',
                ),
                _buildMetricBadge('Maximized', _isMaximized ? 'YES' : 'NO'),
                _buildMetricBadge('Minimized', _isMinimized ? 'YES' : 'NO'),
                _buildMetricBadge('FullScreen', _isFullScreen ? 'YES' : 'NO'),
                _buildMetricBadge(
                  'Snap Hover',
                  _snapButtonHovered ? 'HTMAXBUTTON' : 'IDLE',
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildMetricBadge(String label, String value) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: Colors.black26,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.white10),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Text(
            label,
            style: const TextStyle(fontSize: 11, color: Colors.white54),
          ),
          const SizedBox(height: 2),
          Text(
            value,
            style: const TextStyle(
              fontSize: 13,
              fontWeight: FontWeight.w600,
              color: Colors.white,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildAtomicPresetsCard() {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            const Text(
              '1. Atomic Batch Configuration (Single-Channel Zero-Jump Presets)',
              style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 6),
            const Text(
              'Applies size, min/max constraints, centering, titleBarStyle, trafficLightsOffset, '
              'and backgroundColor in one atomic native transaction.',
              style: TextStyle(fontSize: 12, color: Colors.white70),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                FilledButton.icon(
                  onPressed: () async {
                    const Color bg = Color(0xFF141622);
                    final WindowStateSnapshot snap =
                        await XueHuaNaviteWindowManager.configure(
                          const WindowConfig(
                            size: Size(1280, 820),
                            minSize: Size(860, 580),
                            center: true,
                            title: 'Studio Default (1280×820 Centered)',
                            titleBarStyle: TitleBarStyle.hidden,
                            trafficLightsOffset: Offset(16, 14),
                            trafficLightsVisible: true,
                            backgroundColor: bg,
                            preventClose: true,
                            showWhenReady: true,
                          ),
                        );
                    setState(() {
                      _bgColor = bg;
                      _titleBarStyle = TitleBarStyle.hidden;
                      _trafficLightsX = 16;
                      _trafficLightsY = 14;
                      _trafficLightsVisible = true;
                      _currentBounds = snap.bounds;
                    });
                    _appendLog('Atomic configure -> Studio Default (1280x820)');
                  },
                  icon: const Icon(Icons.bolt_rounded, size: 18),
                  label: const Text('Studio Preset (1280×820 Centered)'),
                ),
                FilledButton.tonalIcon(
                  onPressed: () async {
                    const Color bg = Color(0xFF0F172A);
                    final WindowStateSnapshot snap =
                        await XueHuaNaviteWindowManager.configure(
                          const WindowConfig(
                            size: Size(980, 660),
                            minSize: Size(720, 480),
                            center: true,
                            title: 'Compact Immersive Mode',
                            titleBarStyle: TitleBarStyle.immersive,
                            trafficLightsOffset: Offset(22, 18),
                            trafficLightsVisible: true,
                            backgroundColor: bg,
                            alwaysOnTop: false,
                            showWhenReady: true,
                          ),
                        );
                    setState(() {
                      _bgColor = bg;
                      _titleBarStyle = TitleBarStyle.immersive;
                      _trafficLightsX = 22;
                      _trafficLightsY = 18;
                      _currentBounds = snap.bounds;
                    });
                    _appendLog(
                      'Atomic configure -> Compact Immersive (980x660)',
                    );
                  },
                  icon: const Icon(Icons.layers_rounded, size: 18),
                  label: const Text('Compact Immersive (980×660)'),
                ),
                OutlinedButton.icon(
                  onPressed: () async {
                    final WindowStateSnapshot snap =
                        await XueHuaNaviteWindowManager.configure(
                          const WindowConfig(
                            size: Size(1100, 720),
                            center: true,
                            title: 'Native System TitleBar',
                            titleBarStyle: TitleBarStyle.normal,
                            showWhenReady: true,
                          ),
                        );
                    setState(() {
                      _titleBarStyle = TitleBarStyle.normal;
                      _currentBounds = snap.bounds;
                    });
                    _appendLog('Atomic configure -> System Normal (1100x720)');
                  },
                  icon: const Icon(Icons.web_asset_rounded, size: 18),
                  label: const Text('System TitleBar (1100×720)'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTitleBarAndTrafficLightsCard() {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            const Text(
              '2. Custom Frame & macOS Traffic Lights Offset Controller',
              style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            Row(
              children: <Widget>[
                const Text('TitleBarStyle: '),
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
                  selected: <TitleBarStyle>{_titleBarStyle},
                  onSelectionChanged: (Set<TitleBarStyle> selection) {
                    final TitleBarStyle style = selection.first;
                    setState(() => _titleBarStyle = style);
                    _windowManager.setTitleBarStyle(
                      style,
                      trafficLightsOffset: Offset(
                        _trafficLightsX,
                        _trafficLightsY,
                      ),
                      trafficLightsVisible: _trafficLightsVisible,
                    );
                    _appendLog('setTitleBarStyle(${style.wireValue})');
                  },
                ),
              ],
            ),
            const SizedBox(height: 14),
            Row(
              children: <Widget>[
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Text(
                        'macOS Traffic Lights X Offset: ${_trafficLightsX.toStringAsFixed(0)} pt',
                        style: const TextStyle(fontSize: 12),
                      ),
                      Slider(
                        value: _trafficLightsX,
                        min: 8.0,
                        max: 64.0,
                        divisions: 56,
                        onChanged: (double value) {
                          setState(() => _trafficLightsX = value);
                          _windowManager.setTrafficLightsOffset(
                            Offset(_trafficLightsX, _trafficLightsY),
                          );
                        },
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
                        'macOS Traffic Lights Y Offset: ${_trafficLightsY.toStringAsFixed(0)} pt',
                        style: const TextStyle(fontSize: 12),
                      ),
                      Slider(
                        value: _trafficLightsY,
                        min: 6.0,
                        max: 48.0,
                        divisions: 42,
                        onChanged: (double value) {
                          setState(() => _trafficLightsY = value);
                          _windowManager.setTrafficLightsOffset(
                            Offset(_trafficLightsX, _trafficLightsY),
                          );
                        },
                      ),
                    ],
                  ),
                ),
              ],
            ),
            if (_isMacOS)
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Show macOS Traffic Light Buttons'),
                subtitle: const Text(
                  'Dynamically toggle close / miniaturize / zoom button visibility',
                ),
                value: _trafficLightsVisible,
                onChanged: (bool value) {
                  setState(() => _trafficLightsVisible = value);
                  _windowManager.setTrafficLightsVisible(value);
                },
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildBehaviorAndAppearanceCard() {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            const Text(
              '3. Window Behavior, DWM Shadow & Anti-Tearing Background Sync',
              style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 16,
              runSpacing: 4,
              children: <Widget>[
                FilterChip(
                  label: const Text('Prevent Close (Intercept WM_CLOSE)'),
                  selected: _preventClose,
                  onSelected: (bool v) {
                    setState(() => _preventClose = v);
                    _windowManager.setPreventClose(v);
                    _appendLog('setPreventClose($v)');
                  },
                ),
                FilterChip(
                  label: const Text('Always On Top'),
                  selected: _alwaysOnTop,
                  onSelected: (bool v) {
                    setState(() => _alwaysOnTop = v);
                    _windowManager.setAlwaysOnTop(v);
                    _appendLog('setAlwaysOnTop($v)');
                  },
                ),
                FilterChip(
                  label: const Text('Resizable'),
                  selected: _resizable,
                  onSelected: (bool v) {
                    setState(() => _resizable = v);
                    _windowManager.setResizable(v);
                    _appendLog('setResizable($v)');
                  },
                ),
                FilterChip(
                  label: const Text('Native Shadow & Rounded Corners'),
                  selected: _hasShadow,
                  onSelected: (bool v) {
                    setState(() => _hasShadow = v);
                    _windowManager.setHasShadow(v);
                    _appendLog('setHasShadow($v)');
                  },
                ),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              children: <Widget>[
                const Text('Window Opacity: ', style: TextStyle(fontSize: 12)),
                Expanded(
                  child: Slider(
                    value: _opacity,
                    min: 0.35,
                    max: 1.0,
                    onChanged: (double value) {
                      setState(() => _opacity = value);
                      _windowManager.setOpacity(value);
                    },
                  ),
                ),
                Text('${(_opacity * 100).toStringAsFixed(0)}%'),
              ],
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              children: <Widget>[
                _buildColorSwatchButton(
                  'Midnight Indigo',
                  const Color(0xFF141622),
                ),
                _buildColorSwatchButton('Deep Slate', const Color(0xFF0F172A)),
                _buildColorSwatchButton(
                  'Obsidian Carbon',
                  const Color(0xFF18181B),
                ),
                _buildColorSwatchButton('Forest Pine', const Color(0xFF062019)),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildColorSwatchButton(String label, Color color) {
    final bool isSelected = _bgColor.toARGB32() == color.toARGB32();
    return OutlinedButton.icon(
      onPressed: () {
        setState(() => _bgColor = color);
        _windowManager.setBackgroundColor(color);
        _appendLog('setBackgroundColor($label)');
      },
      icon: Container(
        width: 14,
        height: 14,
        decoration: BoxDecoration(
          color: color,
          shape: BoxShape.circle,
          border: Border.all(color: Colors.white54),
        ),
      ),
      label: Text(
        isSelected ? '$label ✓' : label,
        style: const TextStyle(fontSize: 12),
      ),
    );
  }

  Widget _buildDragAndEdgeResizeCard() {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            const Text(
              '4. Native Drag Area, Lifecycle Actions & 8-Direction Resizing',
              style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            DragArea(
              onDragStart: () => _appendLog('startDragging() via DragArea'),
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    colors: <Color>[Color(0xFF312E81), Color(0xFF1E1B4B)],
                  ),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: const Color(0xFF6366F1)),
                ),
                child: const Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: <Widget>[
                    Icon(Icons.open_with_rounded, color: Colors.white),
                    SizedBox(width: 10),
                    Text(
                      'Custom DragArea Widget — Press & Drag Anywhere Here (Double-Click to Maximize)',
                      style: TextStyle(fontWeight: FontWeight.w600),
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
                  onPressed: () => _windowManager.minimize(),
                  icon: const Icon(Icons.minimize, size: 16),
                  label: const Text('Minimize'),
                ),
                ElevatedButton.icon(
                  onPressed: () => _windowManager.toggleMaximize(),
                  icon: const Icon(Icons.crop_square, size: 16),
                  label: Text(_isMaximized ? 'Unmaximize' : 'Maximize'),
                ),
                ElevatedButton.icon(
                  onPressed: () => _windowManager.center(animate: true),
                  icon: const Icon(Icons.center_focus_strong, size: 16),
                  label: const Text('Center'),
                ),
                ElevatedButton.icon(
                  onPressed: () => _windowManager.close(),
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

  Widget _buildEventStreamPanel() {
    return Container(
      color: const Color(0xFF10121C),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              const Icon(
                Icons.terminal_rounded,
                size: 18,
                color: Color(0xFF34D399),
              ),
              const SizedBox(width: 8),
              const Text(
                'Native EventChannel Stream',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 14),
              ),
              const Spacer(),
              TextButton(
                onPressed: () => setState(_eventLogs.clear),
                child: const Text('Clear', style: TextStyle(fontSize: 12)),
              ),
            ],
          ),
          const Divider(color: Colors.white12),
          Expanded(
            child: _eventLogs.isEmpty
                ? const Center(
                    child: Text(
                      'Move, resize, maximize, focus, or click close to inspect real-time native events.',
                      textAlign: TextAlign.center,
                      style: TextStyle(color: Colors.white38, fontSize: 12),
                    ),
                  )
                : ListView.builder(
                    itemCount: _eventLogs.length,
                    itemBuilder: (BuildContext context, int index) {
                      return Padding(
                        padding: const EdgeInsets.symmetric(vertical: 3),
                        child: Text(
                          _eventLogs[index],
                          style: const TextStyle(
                            fontFamily: 'monospace',
                            fontSize: 11.5,
                            color: Color(0xFFA7F3D0),
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

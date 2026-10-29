import 'package:flutter/foundation.dart';

/// Registration and authorization status of the application's launch-at-startup
/// (auto-start on user login) entry reported by the operating system.
enum LaunchAtStartupStatus {
  /// The application is registered and approved to launch automatically at user login.
  enabled('enabled'),

  /// The application is not registered to launch at user login.
  disabled('disabled'),

  /// Registration exists, but the operating system requires the user to approve
  /// it in System Settings (for example `SMAppService.Status.requiresApproval`
  /// on macOS 13+).
  requiresApproval('requiresApproval'),

  /// The user explicitly disabled startup in the operating system's settings UI
  /// (for example Windows Task Manager / `StartupApproved\Run`, MSIX
  /// `StartupTaskState.DisabledByUser`, or an XDG autostart `.desktop` entry with
  /// `Hidden=true` or `X-GNOME-Autostart-enabled=false`).
  disabledByUser('disabledByUser'),

  /// Startup is blocked by enterprise or system group policy
  /// (for example Windows `StartupApproved\Run` policy byte `0x09` or MSIX
  /// `StartupTaskState.DisabledByPolicy`).
  disabledByPolicy('disabledByPolicy'),

  /// Launch-at-startup is not supported in the current runtime environment
  /// (for example a sandboxed macOS 12 app where `SMAppService` is unavailable,
  /// or an MSIX package with no `windows.startupTask` declared in its manifest).
  unsupported('unsupported');

  const LaunchAtStartupStatus(this.wireValue);

  /// String value transmitted over the platform channel.
  final String wireValue;

  /// Parses a [LaunchAtStartupStatus] from its platform channel string representation.
  static LaunchAtStartupStatus fromWire(String? value) {
    for (final LaunchAtStartupStatus status in LaunchAtStartupStatus.values) {
      if (status.wireValue == value) {
        return status;
      }
    }
    return LaunchAtStartupStatus.disabled;
  }
}

/// Strategy used on macOS to register the application for launch at user login.
enum MacOSLaunchAtStartupMode {
  /// Automatically uses `SMAppService.mainApp` on macOS 13.0+ (App Sandbox and
  /// Mac App Store compatible). When running outside App Sandbox with non-empty
  /// [LaunchAtStartupConfig.arguments], or on macOS 12 outside App Sandbox,
  /// automatically falls back to a user `LaunchAgent` plist.
  automatic('automatic'),

  /// Forces Apple's `SMAppService.mainApp` API (`macOS 13.0+`).
  ///
  /// Works inside App Sandbox without extra entitlements, and syncs with
  /// `System Settings > General > Login Items`. Does not pass custom CLI flags.
  appService('appService'),

  /// Uses a user `LaunchAgent` property list at
  /// `~/Library/LaunchAgents/<identifier>.plist`.
  ///
  /// Requires an unsandboxed macOS application. Supports custom command-line
  /// [LaunchAtStartupConfig.arguments] and macOS 12.0+.
  launchAgent('launchAgent');

  const MacOSLaunchAtStartupMode(this.wireValue);

  /// String value transmitted over the platform channel.
  final String wireValue;

  /// Parses a [MacOSLaunchAtStartupMode] from its platform channel string representation.
  static MacOSLaunchAtStartupMode fromWire(String? value) {
    for (final MacOSLaunchAtStartupMode mode
        in MacOSLaunchAtStartupMode.values) {
      if (mode.wireValue == value) {
        return mode;
      }
    }
    return MacOSLaunchAtStartupMode.automatic;
  }
}

/// Native operating system mechanism managing the launch-at-startup registration.
enum LaunchAtStartupBackend {
  /// Apple `SMAppService.mainApp` (`ServiceManagement`, macOS 13.0+).
  macosAppService('macosAppService'),

  /// macOS `~/Library/LaunchAgents/<identifier>.plist`.
  macosLaunchAgent('macosLaunchAgent'),

  /// Windows Registry `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
  /// paired with `StartupApproved\Run`.
  windowsRegistry('windowsRegistry'),

  /// Windows AppModel `StartupTask` (`SystemAppData` state for MSIX packages).
  windowsStartupTask('windowsStartupTask'),

  /// Linux Freedesktop XDG Autostart `$XDG_CONFIG_HOME/autostart/<identifier>.desktop`.
  linuxXdgAutostart('linuxXdgAutostart'),

  /// No supported startup backend available on the current platform.
  none('none');

  const LaunchAtStartupBackend(this.wireValue);

  /// String value transmitted over the platform channel.
  final String wireValue;

  /// Parses a [LaunchAtStartupBackend] from its platform channel string representation.
  static LaunchAtStartupBackend fromWire(String? value) {
    for (final LaunchAtStartupBackend backend
        in LaunchAtStartupBackend.values) {
      if (backend.wireValue == value) {
        return backend;
      }
    }
    return LaunchAtStartupBackend.none;
  }
}

/// Optional customization for registering or querying launch-at-startup behavior.
///
/// All fields are optional: when omitted, the native plugin automatically resolves
/// the application bundle identifier, display name, and executable path from the
/// running host process (`Bundle.main` on macOS, `GetModuleFileNameW` / AppModel
/// on Windows, and `$APPIMAGE` / `$FLATPAK_ID` / `/proc/self/exe` on Linux).
@immutable
class LaunchAtStartupConfig {
  /// Human-readable display name written to desktop startup entries (such as
  /// the `Name=` key in Linux `.desktop` files or the fallback Windows `Run`
  /// registry value name).
  final String? appName;

  /// Unique identifier for the startup entry:
  /// - **Windows (`windowsRegistry`)**: Registry value name under `CurrentVersion\Run`.
  /// - **macOS (`macosLaunchAgent`)**: `Label` and `<identifier>.plist` filename.
  /// - **Linux (`linuxXdgAutostart`)**: `<identifier>.desktop` filename stem.
  final String? identifier;

  /// Explicit executable path to launch on user login.
  ///
  /// Defaults to the running application executable (or `$APPIMAGE` on Linux
  /// AppImage builds).
  final String? executablePath;

  /// Command-line arguments appended when launching at login
  /// (for example `const <String>['--autostart', '--minimized']`).
  ///
  /// Supported by `windowsRegistry`, `linuxXdgAutostart`, and unsandboxed
  /// `macosLaunchAgent`. Note that Apple's `SMAppService.mainApp` and Windows
  /// MSIX `StartupTask` launch the packaged entry point without custom CLI flags.
  final List<String> arguments;

  /// Explicit `TaskId` declared in `<desktop:StartupTask TaskId="..." />` inside
  /// `Package.appxmanifest` for MSIX-packaged Windows apps.
  ///
  /// When omitted on an MSIX app, the plugin automatically discovers the first
  /// startup task registered for the current package.
  final String? msixTaskId;

  /// Registration backend preference on macOS. Defaults to
  /// [MacOSLaunchAtStartupMode.automatic].
  final MacOSLaunchAtStartupMode macOSMode;

  /// Creates an immutable [LaunchAtStartupConfig].
  const LaunchAtStartupConfig({
    this.appName,
    this.identifier,
    this.executablePath,
    this.arguments = const <String>[],
    this.msixTaskId,
    this.macOSMode = MacOSLaunchAtStartupMode.automatic,
  });

  /// Creates a copy of this [LaunchAtStartupConfig] with selective overrides.
  LaunchAtStartupConfig copyWith({
    String? appName,
    String? identifier,
    String? executablePath,
    List<String>? arguments,
    String? msixTaskId,
    MacOSLaunchAtStartupMode? macOSMode,
  }) {
    return LaunchAtStartupConfig(
      appName: appName ?? this.appName,
      identifier: identifier ?? this.identifier,
      executablePath: executablePath ?? this.executablePath,
      arguments: arguments ?? this.arguments,
      msixTaskId: msixTaskId ?? this.msixTaskId,
      macOSMode: macOSMode ?? this.macOSMode,
    );
  }

  /// Converts this configuration into a serializable map for the platform channel.
  Map<String, dynamic> toMap() {
    String? nonBlank(String? value) {
      final String? trimmed = value?.trim();
      return (trimmed == null || trimmed.isEmpty) ? null : trimmed;
    }

    final String? cleanAppName = nonBlank(appName);
    final String? cleanId = nonBlank(identifier);
    final String? cleanExe = nonBlank(executablePath);
    final String? cleanMsix = nonBlank(msixTaskId);

    return <String, dynamic>{
      'appName': ?cleanAppName,
      'identifier': ?cleanId,
      'executablePath': ?cleanExe,
      if (arguments.isNotEmpty) 'arguments': List<String>.of(arguments),
      'msixTaskId': ?cleanMsix,
      if (macOSMode != MacOSLaunchAtStartupMode.automatic)
        'macOSMode': macOSMode.wireValue,
    };
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is LaunchAtStartupConfig &&
        other.appName == appName &&
        other.identifier == identifier &&
        other.executablePath == executablePath &&
        listEquals(other.arguments, arguments) &&
        other.msixTaskId == msixTaskId &&
        other.macOSMode == macOSMode;
  }

  @override
  int get hashCode => Object.hash(
    appName,
    identifier,
    executablePath,
    Object.hashAll(arguments),
    msixTaskId,
    macOSMode,
  );

  @override
  String toString() {
    return 'LaunchAtStartupConfig(appName: $appName, identifier: $identifier, '
        'executablePath: $executablePath, arguments: $arguments, '
        'msixTaskId: $msixTaskId, macOSMode: $macOSMode)';
  }
}

/// Snapshot of the application's launch-at-startup registration state.
@immutable
class LaunchAtStartupState {
  /// Whether launch-at-startup is supported in the current OS and packaging context.
  final bool isSupported;

  /// Convenience flag that is `true` when [status] is [LaunchAtStartupStatus.enabled].
  final bool isEnabled;

  /// Detailed registration and approval status reported by the OS.
  final LaunchAtStartupStatus status;

  /// Native OS mechanism backing this startup registration.
  final LaunchAtStartupBackend backend;

  /// Resolved startup identifier (bundle ID, plist label, registry value name,
  /// MSIX task ID, or `.desktop` entry stem).
  final String? identifier;

  /// Resolved command or executable path associated with the startup entry, when available.
  final String? executablePath;

  /// Whether [XueHuaNaviteWindowManager.openLaunchAtStartupSettings] can open a
  /// dedicated system settings page on this platform (`true` on macOS and Windows).
  final bool canOpenSettings;

  /// Creates an immutable [LaunchAtStartupState].
  const LaunchAtStartupState({
    required this.isSupported,
    required this.isEnabled,
    required this.status,
    this.backend = LaunchAtStartupBackend.none,
    this.identifier,
    this.executablePath,
    this.canOpenSettings = false,
  });

  /// Constructs a [LaunchAtStartupState] from a platform channel dictionary.
  factory LaunchAtStartupState.fromMap(Map<Object?, Object?>? map) {
    if (map == null || map.isEmpty) {
      return const LaunchAtStartupState(
        isSupported: false,
        isEnabled: false,
        status: LaunchAtStartupStatus.unsupported,
      );
    }
    final LaunchAtStartupStatus status = LaunchAtStartupStatus.fromWire(
      map['status'] as String?,
    );
    final bool isSupported =
        (map['isSupported'] as bool?) ??
        (status != LaunchAtStartupStatus.unsupported);
    final bool isEnabled =
        (map['isEnabled'] as bool?) ??
        (status == LaunchAtStartupStatus.enabled);
    final String? rawId = map['identifier'] as String?;
    final String? rawExe = map['executablePath'] as String?;

    return LaunchAtStartupState(
      isSupported: isSupported,
      isEnabled: isEnabled,
      status: status,
      backend: LaunchAtStartupBackend.fromWire(map['backend'] as String?),
      identifier: (rawId == null || rawId.isEmpty) ? null : rawId,
      executablePath: (rawExe == null || rawExe.isEmpty) ? null : rawExe,
      canOpenSettings: (map['canOpenSettings'] as bool?) ?? false,
    );
  }

  /// Converts this state into a serializable map.
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      'isSupported': isSupported,
      'isEnabled': isEnabled,
      'status': status.wireValue,
      'backend': backend.wireValue,
      if (identifier != null) 'identifier': identifier,
      if (executablePath != null) 'executablePath': executablePath,
      'canOpenSettings': canOpenSettings,
    };
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is LaunchAtStartupState &&
        other.isSupported == isSupported &&
        other.isEnabled == isEnabled &&
        other.status == status &&
        other.backend == backend &&
        other.identifier == identifier &&
        other.executablePath == executablePath &&
        other.canOpenSettings == canOpenSettings;
  }

  @override
  int get hashCode => Object.hash(
    isSupported,
    isEnabled,
    status,
    backend,
    identifier,
    executablePath,
    canOpenSettings,
  );

  @override
  String toString() {
    return 'LaunchAtStartupState(isSupported: $isSupported, '
        'isEnabled: $isEnabled, status: $status, backend: $backend, '
        'identifier: $identifier, executablePath: $executablePath, '
        'canOpenSettings: $canOpenSettings)';
  }
}

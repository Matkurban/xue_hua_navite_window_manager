import Cocoa
import FlutterMacOS
import ServiceManagement

/// Native macOS launch-at-startup (login item) manager supporting both Apple's
/// modern `SMAppService.mainApp` (`macOS 13.0+`, App Sandbox & Mac App Store
/// compatible) and user `LaunchAgent` property lists (`~/Library/LaunchAgents`,
/// unsandboxed apps with custom CLI arguments or `macOS 12.0`).
enum XueHuaLaunchAtStartup {
    enum BackendKind: String {
        case appService = "macosAppService"
        case launchAgent = "macosLaunchAgent"
        case none
    }

    enum RequestedMode: String {
        case automatic
        case appService
        case launchAgent

        static func fromWire(_ raw: String?) -> RequestedMode {
            guard let raw = raw else { return .automatic }
            return RequestedMode(rawValue: raw) ?? .automatic
        }
    }

    // MARK: - Pure Testable Helpers

    /// Sanitizes an arbitrary identifier or app name into a safe filename/label stem.
    static func sanitizeIdentifier(_ raw: String?) -> String? {
        guard let raw = raw else { return nil }
        var cleaned = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if cleaned.hasSuffix(".plist") {
            cleaned = String(cleaned.dropLast(6))
        }
        let filtered = cleaned.unicodeScalars.filter { scalar in
            let v = scalar.value
            if v < 32 || v == 127 {
                return false
            }
            if scalar == "/" || scalar == "\\" {
                return false
            }
            return true
        }
        var result = String(String.UnicodeScalarView(filtered))
        while result.hasPrefix(".") {
            result.removeFirst()
        }
        result = result.trimmingCharacters(in: .whitespacesAndNewlines)
        return result.isEmpty ? nil : result
    }

    /// Resolves the startup identifier from explicit config or host bundle metadata.
    static func resolveIdentifier(
        explicitIdentifier: String?,
        appName: String?,
        bundleIdentifier: String? = Bundle.main.bundleIdentifier,
        executableURL: URL? = Bundle.main.executableURL
    ) -> String {
        if let id = sanitizeIdentifier(explicitIdentifier) {
            return id
        }
        if let bundleId = sanitizeIdentifier(bundleIdentifier) {
            return bundleId
        }
        if let name = sanitizeIdentifier(appName) {
            return name
        }
        if let exeStem = sanitizeIdentifier(executableURL?.deletingPathExtension().lastPathComponent) {
            return exeStem
        }
        return "com.xuehua.app"
    }

    /// Resolves the target executable path from explicit config or `Bundle.main`.
    static func resolveExecutablePath(
        explicitExecutablePath: String?,
        bundleExecutablePath: String? = Bundle.main.executablePath,
        processArguments: [String] = ProcessInfo.processInfo.arguments
    ) -> String? {
        if let explicit = explicitExecutablePath?.trimmingCharacters(in: .whitespacesAndNewlines),
           !explicit.isEmpty
        {
            return explicit
        }
        if let bundleExe = bundleExecutablePath, !bundleExe.isEmpty {
            return bundleExe
        }
        if let arg0 = processArguments.first, !arg0.isEmpty {
            return arg0
        }
        return nil
    }

    /// Returns `true` when the host process is running inside macOS App Sandbox.
    static func isAppSandboxed(
        environment: [String: String] = ProcessInfo.processInfo.environment
    ) -> Bool {
        if let containerId = environment["APP_SANDBOX_CONTAINER_ID"], !containerId.isEmpty {
            return true
        }
        return false
    }

    /// Returns `true` when `Bundle.main` is a valid `.app` application bundle.
    static func isBundledApplication(
        bundleURL: URL = Bundle.main.bundleURL,
        bundleIdentifier: String? = Bundle.main.bundleIdentifier
    ) -> Bool {
        guard let bundleId = bundleIdentifier, !bundleId.isEmpty else {
            return false
        }
        return bundleURL.pathExtension.lowercased() == "app"
    }

    /// Chooses the native registration backend given OS capabilities, sandbox state, and config.
    static func selectBackend(
        requestedMode: RequestedMode,
        hasCustomArguments: Bool,
        hasExplicitLaunchAgentOverride: Bool,
        isSandboxed: Bool,
        supportsAppService: Bool
    ) -> BackendKind {
        switch requestedMode {
        case .appService:
            return .appService
        case .launchAgent:
            return .launchAgent
        case .automatic:
            if isSandboxed {
                return supportsAppService ? .appService : .none
            }
            if hasCustomArguments || hasExplicitLaunchAgentOverride {
                return .launchAgent
            }
            return supportsAppService ? .appService : .launchAgent
        }
    }

    // MARK: - State Dictionary Builder

    static func makeStateMap(
        isSupported: Bool,
        status: String,
        backend: BackendKind,
        identifier: String?,
        executablePath: String?,
        canOpenSettings: Bool
    ) -> [String: Any] {
        var map: [String: Any] = [
            "isSupported": isSupported,
            "isEnabled": status == "enabled",
            "status": status,
            "backend": backend.rawValue,
            "canOpenSettings": canOpenSettings,
        ]
        if let identifier = identifier {
            map["identifier"] = identifier
        }
        if let executablePath = executablePath {
            map["executablePath"] = executablePath
        }
        return map
    }

    // MARK: - MethodChannel Entry Points

    static func getState(args: [String: Any]) -> [String: Any] {
        let explicitId = args["identifier"] as? String
        let appName = args["appName"] as? String
        let explicitExe = args["executablePath"] as? String
        let arguments = (args["arguments"] as? [String]) ?? []
        let requestedMode = RequestedMode.fromWire(args["macOSMode"] as? String)

        let identifier = resolveIdentifier(explicitIdentifier: explicitId, appName: appName)
        let executablePath = resolveExecutablePath(explicitExecutablePath: explicitExe)
        let sandboxed = isAppSandboxed()
        let hasExplicitOverride =
            (explicitId != nil && !(explicitId?.isEmpty ?? true))
                || (explicitExe != nil && !(explicitExe?.isEmpty ?? true))

        let supportsAppService: Bool
        if #available(macOS 13.0, *) {
            supportsAppService = true
        } else {
            supportsAppService = false
        }

        let backend = selectBackend(
            requestedMode: requestedMode,
            hasCustomArguments: !arguments.isEmpty,
            hasExplicitLaunchAgentOverride: hasExplicitOverride && !sandboxed,
            isSandboxed: sandboxed,
            supportsAppService: supportsAppService
        )

        switch backend {
        case .appService:
            if #available(macOS 13.0, *) {
                return queryAppServiceState(
                    requestedMode: requestedMode,
                    sandboxed: sandboxed,
                    identifier: identifier,
                    executablePath: executablePath
                )
            } else {
                return makeStateMap(
                    isSupported: false,
                    status: "unsupported",
                    backend: .appService,
                    identifier: identifier,
                    executablePath: executablePath,
                    canOpenSettings: true
                )
            }

        case .launchAgent:
            if sandboxed {
                return makeStateMap(
                    isSupported: false,
                    status: "unsupported",
                    backend: .launchAgent,
                    identifier: identifier,
                    executablePath: executablePath,
                    canOpenSettings: true
                )
            }
            return queryLaunchAgentState(
                identifier: identifier,
                fallbackExecutablePath: executablePath
            )

        case .none:
            return makeStateMap(
                isSupported: false,
                status: "unsupported",
                backend: .none,
                identifier: identifier,
                executablePath: executablePath,
                canOpenSettings: true
            )
        }
    }

    static func setEnabled(args: [String: Any], result: @escaping FlutterResult) {
        let enabled = args["enabled"] as? Bool ?? false
        let explicitId = args["identifier"] as? String
        let appName = args["appName"] as? String
        let explicitExe = args["executablePath"] as? String
        let arguments = (args["arguments"] as? [String]) ?? []
        let requestedMode = RequestedMode.fromWire(args["macOSMode"] as? String)

        let identifier = resolveIdentifier(explicitIdentifier: explicitId, appName: appName)
        let executablePath = resolveExecutablePath(explicitExecutablePath: explicitExe)
        let sandboxed = isAppSandboxed()
        let hasExplicitOverride =
            (explicitId != nil && !(explicitId?.isEmpty ?? true))
                || (explicitExe != nil && !(explicitExe?.isEmpty ?? true))

        let supportsAppService: Bool
        if #available(macOS 13.0, *) {
            supportsAppService = true
        } else {
            supportsAppService = false
        }

        let backend = selectBackend(
            requestedMode: requestedMode,
            hasCustomArguments: !arguments.isEmpty,
            hasExplicitLaunchAgentOverride: hasExplicitOverride && !sandboxed,
            isSandboxed: sandboxed,
            supportsAppService: supportsAppService
        )

        switch backend {
        case .appService:
            guard #available(macOS 13.0, *) else {
                result(
                    FlutterError(
                        code: "UNSUPPORTED",
                        message: "SMAppService requires macOS 13.0 or later.",
                        details: nil
                    )
                )
                return
            }
            setAppServiceEnabled(
                enabled,
                requestedMode: requestedMode,
                sandboxed: sandboxed,
                identifier: identifier,
                executablePath: executablePath,
                result: result
            )

        case .launchAgent:
            if sandboxed {
                result(
                    FlutterError(
                        code: "SANDBOX_RESTRICTION",
                        message: "LaunchAgent plist registration requires an unsandboxed macOS app. Use MacOSLaunchAtStartupMode.appService inside App Sandbox.",
                        details: nil
                    )
                )
                return
            }
            do {
                let state = try setLaunchAgentEnabled(
                    enabled,
                    identifier: identifier,
                    executablePath: executablePath,
                    arguments: arguments
                )
                result(state)
            } catch let error as NSError {
                result(
                    FlutterError(
                        code: "LAUNCH_AT_STARTUP_FAILED",
                        message: error.localizedDescription,
                        details: error.code
                    )
                )
            }

        case .none:
            result(
                FlutterError(
                    code: "UNSUPPORTED",
                    message: "Launch at startup is not supported in this macOS sandbox/OS configuration.",
                    details: nil
                )
            )
        }
    }

    static func openSettings() -> Bool {
        if #available(macOS 13.0, *) {
            SMAppService.openSystemSettingsLoginItems()
            return true
        }
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.users") {
            return NSWorkspace.shared.open(url)
        }
        return false
    }
}

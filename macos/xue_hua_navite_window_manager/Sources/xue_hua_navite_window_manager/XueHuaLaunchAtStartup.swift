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
        case none = "none"
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
            if v < 32 || v == 127 { return false }
            if scalar == "/" || scalar == "\\" { return false }
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

    /// Builds a `launchd.plist(5)` dictionary for a GUI user `LaunchAgent`.
    static func buildLaunchAgentPlistDictionary(
        label: String,
        executablePath: String,
        arguments: [String]
    ) -> [String: Any] {
        var programArguments = [executablePath]
        programArguments.append(contentsOf: arguments)
        return [
            "Label": label,
            "ProgramArguments": programArguments,
            "RunAtLoad": true,
            "ProcessType": "Interactive",
        ]
    }

    /// Serializes a `LaunchAgent` dictionary into XML property list bytes.
    static func serializeLaunchAgentPlist(
        label: String,
        executablePath: String,
        arguments: [String]
    ) throws -> Data {
        let plist = buildLaunchAgentPlistDictionary(
            label: label,
            executablePath: executablePath,
            arguments: arguments
        )
        return try PropertyListSerialization.data(
            fromPropertyList: plist,
            format: .xml,
            options: 0
        )
    }

    /// Evaluates raw XML/binary `.plist` bytes from `~/Library/LaunchAgents`.
    static func evaluateLaunchAgentPlistData(
        _ data: Data?
    ) -> (status: String, executablePath: String?) {
        guard let data = data, !data.isEmpty else {
            return ("disabled", nil)
        }
        guard let rawPlist = try? PropertyListSerialization.propertyList(
            from: data,
            options: [],
            format: nil
        ),
            let dict = rawPlist as? [String: Any]
        else {
            return ("disabled", nil)
        }

        let programArgs = dict["ProgramArguments"] as? [String]
        let programPath = programArgs?.first ?? (dict["Program"] as? String)
        guard let resolvedProgram = programPath, !resolvedProgram.isEmpty else {
            return ("disabled", nil)
        }

        if (dict["Disabled"] as? Bool) == true {
            return ("disabledByUser", resolvedProgram)
        }
        if let runAtLoad = dict["RunAtLoad"] as? Bool, !runAtLoad {
            return ("disabled", resolvedProgram)
        }
        return ("enabled", resolvedProgram)
    }

    @available(macOS 13.0, *)
    static func mapAppServiceStatus(
        _ status: SMAppService.Status,
        isBundledApp: Bool
    ) -> (isSupported: Bool, status: String) {
        guard isBundledApp else {
            return (false, "unsupported")
        }
        switch status {
        case .enabled:
            return (true, "enabled")
        case .notRegistered:
            return (true, "disabled")
        case .requiresApproval:
            return (true, "requiresApproval")
        case .notFound:
            // macOS BTM commonly reports .notFound before the very first registration.
            return (true, "disabled")
        @unknown default:
            return (true, "disabled")
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

    // MARK: - LaunchAgent File Operations

    static func defaultLaunchAgentsDirectory() -> URL {
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("LaunchAgents", isDirectory: true)
    }

    static func launchAgentFileURL(
        identifier: String,
        directory: URL? = nil
    ) -> URL {
        let dir = directory ?? defaultLaunchAgentsDirectory()
        return dir.appendingPathComponent("\(identifier).plist", isDirectory: false)
    }

    static func queryLaunchAgentState(
        identifier: String,
        fallbackExecutablePath: String?,
        launchAgentsDirectory: URL? = nil,
        canOpenSettings: Bool = true
    ) -> [String: Any] {
        let fileURL = launchAgentFileURL(identifier: identifier, directory: launchAgentsDirectory)
        let data = try? Data(contentsOf: fileURL)
        let evaluated = evaluateLaunchAgentPlistData(data)
        return makeStateMap(
            isSupported: true,
            status: evaluated.status,
            backend: .launchAgent,
            identifier: identifier,
            executablePath: evaluated.executablePath ?? fallbackExecutablePath,
            canOpenSettings: canOpenSettings
        )
    }

    static func setLaunchAgentEnabled(
        _ enabled: Bool,
        identifier: String,
        executablePath: String?,
        arguments: [String],
        launchAgentsDirectory: URL? = nil,
        canOpenSettings: Bool = true
    ) throws -> [String: Any] {
        let dir = launchAgentsDirectory ?? defaultLaunchAgentsDirectory()
        let fileURL = launchAgentFileURL(identifier: identifier, directory: dir)
        let fm = FileManager.default

        if enabled {
            guard let exe = executablePath, !exe.isEmpty else {
                throw NSError(
                    domain: "XueHuaLaunchAtStartup",
                    code: 2,
                    userInfo: [NSLocalizedDescriptionKey: "Unable to resolve application executable path for LaunchAgent."]
                )
            }
            try fm.createDirectory(at: dir, withIntermediateDirectories: true, attributes: nil)
            let data = try serializeLaunchAgentPlist(
                label: identifier,
                executablePath: exe,
                arguments: arguments
            )
            try data.write(to: fileURL, options: .atomic)
        } else {
            if fm.fileExists(atPath: fileURL.path) {
                try fm.removeItem(at: fileURL)
            }
        }

        return queryLaunchAgentState(
            identifier: identifier,
            fallbackExecutablePath: executablePath,
            launchAgentsDirectory: dir,
            canOpenSettings: canOpenSettings
        )
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
                let bundled = isBundledApplication()
                let mapped = mapAppServiceStatus(SMAppService.mainApp.status, isBundledApp: bundled)
                // In automatic unsandboxed mode, if SMAppService is not registered, also check
                // whether a user LaunchAgent plist already exists for this identifier.
                if requestedMode == .automatic, !sandboxed, mapped.status == "disabled" {
                    let plistURL = launchAgentFileURL(identifier: identifier)
                    if FileManager.default.fileExists(atPath: plistURL.path) {
                        return queryLaunchAgentState(
                            identifier: identifier,
                            fallbackExecutablePath: executablePath
                        )
                    }
                }
                return makeStateMap(
                    isSupported: mapped.isSupported,
                    status: mapped.status,
                    backend: .appService,
                    identifier: Bundle.main.bundleIdentifier ?? identifier,
                    executablePath: Bundle.main.executablePath ?? executablePath,
                    canOpenSettings: true
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
            guard isBundledApplication() else {
                result(
                    FlutterError(
                        code: "UNSUPPORTED",
                        message: "SMAppService.mainApp requires a bundled .app application.",
                        details: nil
                    )
                )
                return
            }

            let service = SMAppService.mainApp
            do {
                if enabled {
                    if service.status != .enabled {
                        try service.register()
                    }
                } else {
                    if service.status == .enabled || service.status == .requiresApproval {
                        try service.unregister()
                    }
                    // In automatic unsandboxed mode, also remove any stale LaunchAgent plist.
                    if requestedMode == .automatic, !sandboxed {
                        let plistURL = launchAgentFileURL(identifier: identifier)
                        if FileManager.default.fileExists(atPath: plistURL.path) {
                            try? FileManager.default.removeItem(at: plistURL)
                        }
                    }
                }
            } catch let error as NSError {
                let currentStatus = service.status
                if enabled, currentStatus == .enabled || currentStatus == .requiresApproval || error.code == Int(kSMErrorAlreadyRegistered) {
                    // Registration took effect or awaits user approval in System Settings.
                } else if enabled, error.code == Int(kSMErrorLaunchDeniedByUser) {
                    result(
                        makeStateMap(
                            isSupported: true,
                            status: "disabledByUser",
                            backend: .appService,
                            identifier: Bundle.main.bundleIdentifier ?? identifier,
                            executablePath: Bundle.main.executablePath ?? executablePath,
                            canOpenSettings: true
                        )
                    )
                    return
                } else if !enabled, currentStatus == .notRegistered || currentStatus == .notFound || error.code == Int(kSMErrorJobNotFound) {
                    // Already unregistered — idempotent success.
                } else {
                    result(
                        FlutterError(
                            code: "LAUNCH_AT_STARTUP_FAILED",
                            message: error.localizedDescription,
                            details: error.code
                        )
                    )
                    return
                }
            }

            let mapped = mapAppServiceStatus(service.status, isBundledApp: true)
            result(
                makeStateMap(
                    isSupported: mapped.isSupported,
                    status: mapped.status,
                    backend: .appService,
                    identifier: Bundle.main.bundleIdentifier ?? identifier,
                    executablePath: Bundle.main.executablePath ?? executablePath,
                    canOpenSettings: true
                )
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

import Foundation
import FlutterMacOS
import ServiceManagement

extension XueHuaLaunchAtStartup {
    // MARK: - SMAppService.mainApp Bridge (`macOS 13.0+`)

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

    @available(macOS 13.0, *)
    static func queryAppServiceState(
        requestedMode: RequestedMode,
        sandboxed: Bool,
        identifier: String,
        executablePath: String?
    ) -> [String: Any] {
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
    }

    @available(macOS 13.0, *)
    static func setAppServiceEnabled(
        _ enabled: Bool,
        requestedMode: RequestedMode,
        sandboxed: Bool,
        identifier: String,
        executablePath: String?,
        result: @escaping FlutterResult
    ) {
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
    }
}

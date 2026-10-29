import Cocoa
import FlutterMacOS
import ServiceManagement
import XCTest
@testable import xue_hua_navite_window_manager

class RunnerTests: XCTestCase {
    func testZeroFlashWindowPreparation() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 600),
            styleMask: [.titled, .closable, .resizable],
            backing: .buffered,
            defer: false
        )
        let color = NSColor(srgbRed: 0.1, green: 0.1, blue: 0.15, alpha: 1.0)
        XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow(window, backgroundColor: color)

        XCTAssertEqual(window.alphaValue, 0.0, accuracy: 0.001)
        XCTAssertTrue(window.isOpaque)
    }

    func testClampedWindowSizeHonorsMinimumAndMaximum() {
        let minSize = CGSize(width: 850, height: 560)
        let maxSize = CGSize(width: 1050, height: 700)

        let below = XueHuaNaviteWindowManagerPlugin.clampedWindowSize(
            width: 500,
            height: 400,
            minSize: minSize,
            maxSize: maxSize
        )
        XCTAssertEqual(below.width, 850, accuracy: 0.001)
        XCTAssertEqual(below.height, 560, accuracy: 0.001)

        let above = XueHuaNaviteWindowManagerPlugin.clampedWindowSize(
            width: 1400,
            height: 900,
            minSize: minSize,
            maxSize: maxSize
        )
        XCTAssertEqual(above.width, 1050, accuracy: 0.001)
        XCTAssertEqual(above.height, 700, accuracy: 0.001)

        let inside = XueHuaNaviteWindowManagerPlugin.clampedWindowSize(
            width: 900,
            height: 600,
            minSize: minSize,
            maxSize: maxSize
        )
        XCTAssertEqual(inside.width, 900, accuracy: 0.001)
        XCTAssertEqual(inside.height, 600, accuracy: 0.001)

        let unbounded = XueHuaNaviteWindowManagerPlugin.clampedWindowSize(
            width: 2000,
            height: 100,
            minSize: .zero,
            maxSize: .zero
        )
        XCTAssertEqual(unbounded.width, 2000, accuracy: 0.001)
        XCTAssertEqual(unbounded.height, 100, accuracy: 0.001)

        // Minimum wins when the stored maximum is below the minimum.
        let conflict = XueHuaNaviteWindowManagerPlugin.clampedWindowSize(
            width: 50,
            height: 40,
            minSize: CGSize(width: 800, height: 600),
            maxSize: CGSize(width: 100, height: 80)
        )
        XCTAssertEqual(conflict.width, 800, accuracy: 0.001)
        XCTAssertEqual(conflict.height, 600, accuracy: 0.001)
    }

    func testLaunchAtStartupIdentifierSanitizationAndResolution() {
        XCTAssertEqual(
            XueHuaLaunchAtStartup.sanitizeIdentifier("  com.xuehua.studio.plist "),
            "com.xuehua.studio"
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.sanitizeIdentifier("../bad\\path/id"),
            "badpathid"
        )
        XCTAssertNil(XueHuaLaunchAtStartup.sanitizeIdentifier("   ...   "))

        XCTAssertEqual(
            XueHuaLaunchAtStartup.resolveIdentifier(
                explicitIdentifier: "com.custom.id",
                appName: "Ignored",
                bundleIdentifier: "com.bundle.id",
                executableURL: URL(fileURLWithPath: "/Applications/App.app/Contents/MacOS/App")
            ),
            "com.custom.id"
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.resolveIdentifier(
                explicitIdentifier: nil,
                appName: "Studio",
                bundleIdentifier: "com.bundle.id",
                executableURL: nil
            ),
            "com.bundle.id"
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.resolveIdentifier(
                explicitIdentifier: nil,
                appName: nil,
                bundleIdentifier: nil,
                executableURL: URL(fileURLWithPath: "/usr/local/bin/my_tool")
            ),
            "my_tool"
        )
    }

    func testLaunchAtStartupBackendSelectionMatrix() {
        XCTAssertTrue(
            XueHuaLaunchAtStartup.isAppSandboxed(
                environment: ["APP_SANDBOX_CONTAINER_ID": "com.xuehua.example"]
            )
        )
        XCTAssertFalse(XueHuaLaunchAtStartup.isAppSandboxed(environment: [:]))

        XCTAssertTrue(
            XueHuaLaunchAtStartup.isBundledApplication(
                bundleURL: URL(fileURLWithPath: "/Applications/Studio.app"),
                bundleIdentifier: "com.xuehua.studio"
            )
        )
        XCTAssertFalse(
            XueHuaLaunchAtStartup.isBundledApplication(
                bundleURL: URL(fileURLWithPath: "/usr/local/bin/studio"),
                bundleIdentifier: "com.xuehua.studio"
            )
        )

        // Sandboxed automatic -> SMAppService on macOS 13+, none on macOS 12.
        XCTAssertEqual(
            XueHuaLaunchAtStartup.selectBackend(
                requestedMode: .automatic,
                hasCustomArguments: true,
                hasExplicitLaunchAgentOverride: false,
                isSandboxed: true,
                supportsAppService: true
            ),
            .appService
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.selectBackend(
                requestedMode: .automatic,
                hasCustomArguments: false,
                hasExplicitLaunchAgentOverride: false,
                isSandboxed: true,
                supportsAppService: false
            ),
            .none
        )

        // Unsandboxed automatic -> SMAppService without custom args on macOS 13+,
        // LaunchAgent when custom CLI arguments are present or on macOS 12.
        XCTAssertEqual(
            XueHuaLaunchAtStartup.selectBackend(
                requestedMode: .automatic,
                hasCustomArguments: false,
                hasExplicitLaunchAgentOverride: false,
                isSandboxed: false,
                supportsAppService: true
            ),
            .appService
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.selectBackend(
                requestedMode: .automatic,
                hasCustomArguments: true,
                hasExplicitLaunchAgentOverride: false,
                isSandboxed: false,
                supportsAppService: true
            ),
            .launchAgent
        )
        XCTAssertEqual(
            XueHuaLaunchAtStartup.selectBackend(
                requestedMode: .automatic,
                hasCustomArguments: false,
                hasExplicitLaunchAgentOverride: false,
                isSandboxed: false,
                supportsAppService: false
            ),
            .launchAgent
        )
    }

    func testLaunchAgentPlistRoundTripAndTempDirectoryLifecycle() throws {
        let tempDir = FileManager.default.temporaryDirectory
            .appendingPathComponent("xue_hua_startup_tests_\(UUID().uuidString)", isDirectory: true)
        defer {
            try? FileManager.default.removeItem(at: tempDir)
        }

        let id = "com.xuehua.unit.launchagent"
        let exe = "/Applications/XueHua Studio.app/Contents/MacOS/XueHua Studio"

        let initial = XueHuaLaunchAtStartup.queryLaunchAgentState(
            identifier: id,
            fallbackExecutablePath: exe,
            launchAgentsDirectory: tempDir
        )
        XCTAssertEqual(initial["isSupported"] as? Bool, true)
        XCTAssertEqual(initial["isEnabled"] as? Bool, false)
        XCTAssertEqual(initial["status"] as? String, "disabled")
        XCTAssertEqual(initial["backend"] as? String, "macosLaunchAgent")

        let enabled = try XueHuaLaunchAtStartup.setLaunchAgentEnabled(
            true,
            identifier: id,
            executablePath: exe,
            arguments: ["--minimized", "--profile=dev"],
            launchAgentsDirectory: tempDir
        )
        XCTAssertEqual(enabled["isEnabled"] as? Bool, true)
        XCTAssertEqual(enabled["status"] as? String, "enabled")
        XCTAssertEqual(enabled["executablePath"] as? String, exe)

        let plistURL = XueHuaLaunchAtStartup.launchAgentFileURL(identifier: id, directory: tempDir)
        XCTAssertTrue(FileManager.default.fileExists(atPath: plistURL.path))

        // Simulate user disabling via `Disabled = true` in the LaunchAgent plist.
        let disabledPlist: [String: Any] = [
            "Label": id,
            "ProgramArguments": [exe],
            "RunAtLoad": true,
            "Disabled": true,
        ]
        let disabledBytes = try PropertyListSerialization.data(
            fromPropertyList: disabledPlist,
            format: .xml,
            options: 0
        )
        let evaluatedDisabled = XueHuaLaunchAtStartup.evaluateLaunchAgentPlistData(disabledBytes)
        XCTAssertEqual(evaluatedDisabled.status, "disabledByUser")
        XCTAssertEqual(evaluatedDisabled.executablePath, exe)

        let removed = try XueHuaLaunchAtStartup.setLaunchAgentEnabled(
            false,
            identifier: id,
            executablePath: exe,
            arguments: [],
            launchAgentsDirectory: tempDir
        )
        XCTAssertEqual(removed["isEnabled"] as? Bool, false)
        XCTAssertEqual(removed["status"] as? String, "disabled")
        XCTAssertFalse(FileManager.default.fileExists(atPath: plistURL.path))
    }

    func testAppServiceStatusMapping() {
        if #available(macOS 13.0, *) {
            let unbundled = XueHuaLaunchAtStartup.mapAppServiceStatus(.enabled, isBundledApp: false)
            XCTAssertFalse(unbundled.isSupported)
            XCTAssertEqual(unbundled.status, "unsupported")

            XCTAssertEqual(
                XueHuaLaunchAtStartup.mapAppServiceStatus(.enabled, isBundledApp: true).status,
                "enabled"
            )
            XCTAssertEqual(
                XueHuaLaunchAtStartup.mapAppServiceStatus(.notRegistered, isBundledApp: true).status,
                "disabled"
            )
            XCTAssertEqual(
                XueHuaLaunchAtStartup.mapAppServiceStatus(.notFound, isBundledApp: true).status,
                "disabled"
            )
            XCTAssertEqual(
                XueHuaLaunchAtStartup.mapAppServiceStatus(.requiresApproval, isBundledApp: true).status,
                "requiresApproval"
            )
        }
    }
}

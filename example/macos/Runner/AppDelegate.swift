import Cocoa
import FlutterMacOS

@main
class AppDelegate: FlutterAppDelegate {
    /// Keep the process alive after `hide()`.
    ///
    /// `hide()` calls `orderOut`. The Flutter macOS template returns true here,
    /// and AppKit then terminates the app once no window is visible, so
    /// `show()` never runs. Hosts that hide the window need the same override.
    override func applicationShouldTerminateAfterLastWindowClosed(_: NSApplication) -> Bool {
        return false
    }

    override func applicationSupportsSecureRestorableState(_: NSApplication) -> Bool {
        return true
    }
}

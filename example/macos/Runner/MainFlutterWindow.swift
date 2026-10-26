import Cocoa
import FlutterMacOS
import xue_hua_navite_window_manager

class MainFlutterWindow: NSWindow {
    override func awakeFromNib() {
        // Conceal the window before initial frame presentation to achieve zero-flash startup.
        XueHuaNaviteWindowManagerPlugin.prepareZeroFlashWindow(
            self,
            backgroundColor: NSColor(srgbRed: 0.09, green: 0.09, blue: 0.12, alpha: 1.0)
        )

        let flutterViewController = FlutterViewController()
        let windowFrame = frame
        contentViewController = flutterViewController
        setFrame(windowFrame, display: true)

        RegisterGeneratedPlugins(registry: flutterViewController)

        super.awakeFromNib()
    }
}

import Cocoa
import FlutterMacOS

extension XueHuaNaviteWindowManagerPlugin {
    // MARK: - TitleBar, Background & Traffic Lights Management

    func applyBackgroundColor(to window: NSWindow, argb: UInt32) {
        let a = CGFloat((argb >> 24) & 0xFF) / 255.0
        let r = CGFloat((argb >> 16) & 0xFF) / 255.0
        let g = CGFloat((argb >> 8) & 0xFF) / 255.0
        let b = CGFloat(argb & 0xFF) / 255.0
        let nsColor = NSColor(srgbRed: r, green: g, blue: b, alpha: a)

        window.backgroundColor = nsColor
        window.isOpaque = a >= 1.0

        // Synchronize the underlying FlutterView's CoreAnimation CALayer background color
        // so rapid edge resizing never exposes a black Metal/OpenGL swapchain border.
        if let contentView = window.contentView {
            contentView.wantsLayer = true
            contentView.layer?.backgroundColor = nsColor.cgColor
        }
        if let flutterView = registrar?.view {
            flutterView.wantsLayer = true
            flutterView.layer?.backgroundColor = nsColor.cgColor
        }
    }

    func applyResizableStyle(to window: NSWindow, resizable: Bool) {
        if resizable {
            window.styleMask.insert(.resizable)
        } else {
            window.styleMask.remove(.resizable)
        }
    }

    func applyTitleBarStyle(to window: NSWindow, style: String) {
        currentTitleBarStyle = style

        switch style {
        case "hidden", "immersive":
            window.styleMask.insert(.titled)
            window.styleMask.insert(.fullSizeContentView)
            window.titlebarAppearsTransparent = true
            window.titleVisibility = .hidden
            window.isMovableByWindowBackground = false
        default:
            window.styleMask.insert(.titled)
            window.styleMask.remove(.fullSizeContentView)
            window.titlebarAppearsTransparent = false
            window.titleVisibility = .visible
            window.isMovableByWindowBackground = false
        }
    }

    /// Repositions the standard macOS traffic light buttons (`closeButton`, `miniaturizeButton`,
    /// `zoomButton`) according to `trafficLightsOffset` and ensures they remain pinned to the
    /// desired `(x, y)` coordinates from the top-left corner of the window across resizes.
    func updateTrafficLightsLayout(in window: NSWindow) {
        let buttons: [NSButton?] = [
            window.standardWindowButton(.closeButton),
            window.standardWindowButton(.miniaturizeButton),
            window.standardWindowButton(.zoomButton),
        ]

        let isFullScreen = window.styleMask.contains(.fullScreen)

        for button in buttons {
            button?.isHidden = !trafficLightsVisible && !isFullScreen
        }

        // Do not override traffic light coordinates while in macOS native full-screen space
        // or when using the standard system title bar without a custom offset.
        guard !isFullScreen, let offset = trafficLightsOffset,
              let closeBtn = buttons[0],
              let titlebarContainer = closeBtn.superview
        else {
            return
        }

        let buttonHeight = closeBtn.frame.height

        // Ensure the titlebar container view is tall enough to accommodate vertical downward offsets
        // so the traffic light buttons remain hit-testable when shifted down.
        if let themeFrame = titlebarContainer.superview {
            let requiredTitlebarHeight = max(28.0, offset.y + buttonHeight + 8.0)
            var containerFrame = titlebarContainer.frame
            containerFrame.size.height = requiredTitlebarHeight
            containerFrame.origin.y = themeFrame.frame.height - requiredTitlebarHeight
            titlebarContainer.frame = containerFrame
        }

        // In the titlebar container coordinate space (bottom-left origin), calculate `y`
        // so that the top of each button is located `offset.y` points below the top edge.
        let targetY = titlebarContainer.frame.height - offset.y - buttonHeight

        for (index, optionalButton) in buttons.enumerated() {
            guard let button = optionalButton else { continue }
            let targetX = offset.x + CGFloat(index) * trafficLightButtonSpacing
            button.setFrameOrigin(CGPoint(x: targetX, y: targetY))
        }
    }

    // MARK: - Native Window Dragging

    func performNativeWindowDrag(window: NSWindow) {
        if let currentEvent = NSApplication.shared.currentEvent {
            window.performDrag(with: currentEvent)
        }
    }
}

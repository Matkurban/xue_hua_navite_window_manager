import Cocoa

extension XueHuaNaviteWindowManagerPlugin {
    // MARK: - Pure Size Clamping & Coordinate Conversion Helpers (Top-Left DIP <-> AppKit Bottom-Left)

    /// Clamps a requested size to the limits `configure` already applies.
    ///
    /// AppKit `setFrame` ignores `minSize` / `maxSize`, so `setSize` and
    /// `setBounds` must clamp before they call it. A non-positive maximum on
    /// an axis means that axis is unbounded. When both limits are set and the
    /// maximum is below the minimum, the minimum wins.
    static func clampedWindowSize(
        width: CGFloat,
        height: CGFloat,
        minSize: CGSize,
        maxSize: CGSize
    ) -> CGSize {
        let maxW = maxSize.width > 0 ? maxSize.width : CGFloat.greatestFiniteMagnitude
        let maxH = maxSize.height > 0 ? maxSize.height : CGFloat.greatestFiniteMagnitude
        return CGSize(
            width: max(minSize.width, min(width, maxW)),
            height: max(minSize.height, min(height, maxH))
        )
    }

    func primaryScreenHeight(for window: NSWindow) -> CGFloat {
        if let primaryScreen = NSScreen.screens.first {
            return primaryScreen.frame.height
        }
        return window.screen?.frame.height ?? 1080.0
    }

    func convertTopLeftToCocoaFrame(
        x: CGFloat,
        y: CGFloat,
        width: CGFloat,
        height: CGFloat,
        referenceWindow: NSWindow
    ) -> NSRect {
        let screenHeight = primaryScreenHeight(for: referenceWindow)
        let cocoaY = screenHeight - y - height
        return NSRect(x: x, y: cocoaY, width: width, height: height)
    }

    func extractTopLeftBoundsMap(from window: NSWindow) -> [String: Double] {
        let screenHeight = primaryScreenHeight(for: window)
        let frame = window.frame
        let topLeftY = screenHeight - frame.origin.y - frame.size.height
        return [
            "x": Double(frame.origin.x),
            "y": Double(topLeftY),
            "width": Double(frame.size.width),
            "height": Double(frame.size.height),
        ]
    }

    func computeCenteredFrame(for window: NSWindow, targetSize: CGSize) -> NSRect {
        let targetScreen = window.screen ?? NSScreen.main ?? NSScreen.screens.first
        guard let visibleFrame = targetScreen?.visibleFrame else {
            return NSRect(origin: window.frame.origin, size: targetSize)
        }
        let centeredX = visibleFrame.origin.x + (visibleFrame.width - targetSize.width) / 2.0
        let centeredY = visibleFrame.origin.y + (visibleFrame.height - targetSize.height) / 2.0
        return NSRect(x: round(centeredX), y: round(centeredY), width: targetSize.width, height: targetSize.height)
    }

    func buildWindowStateSnapshot(window: NSWindow) -> [String: Any] {
        var map: [String: Any] = extractTopLeftBoundsMap(from: window)
        map["isVisible"] = window.isVisible && !isWaitingForFirstFrame
        map["isMaximized"] = window.isZoomed
        map["isMinimized"] = window.isMiniaturized
        map["isFullScreen"] = window.styleMask.contains(.fullScreen)
        map["isFocused"] = window.isKeyWindow
        map["isAlwaysOnTop"] = window.level == .floating
        map["isResizable"] = window.styleMask.contains(.resizable)
        map["isPreventClose"] = isPreventClose
        return map
    }
}

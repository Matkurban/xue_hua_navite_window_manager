import Cocoa

extension XueHuaNaviteWindowManagerPlugin {
    // MARK: - Atomic Window Configuration & Zero-Flash First Frame

    func configureAtomically(window: NSWindow, args: [String: Any]) -> [String: Any] {
        // Conceal the window if it has not yet presented its first Flutter frame so
        // multiple styleMask/frame adjustments never produce intermediate visual jumps.
        let showWhenReady = args["showWhenReady"] as? Bool ?? false
        if !showWhenReady, window.alphaValue > 0.0, !isWaitingForFirstFrame {
            // Hold alpha at 0 until markReadyToShow is invoked after the first Flutter frame.
            isWaitingForFirstFrame = true
            window.alphaValue = 0.0
        }

        // 1. Background Color (prevents black edges during live window resizing and startup).
        if let argb = (args["backgroundColor"] as? NSNumber)?.uint32Value {
            applyBackgroundColor(to: window, argb: argb)
        }

        // 2. Title & Shadow & AlwaysOnTop & PreventClose.
        if let title = args["title"] as? String {
            window.title = title
        }
        if let hasShadow = args["hasShadow"] as? Bool {
            window.hasShadow = hasShadow
        }
        if let alwaysOnTop = args["alwaysOnTop"] as? Bool {
            window.level = alwaysOnTop ? .floating : .normal
        }
        if let preventClose = args["preventClose"] as? Bool {
            isPreventClose = preventClose
        }

        // 3. Resizable StyleMask.
        if let resizable = args["resizable"] as? Bool {
            applyResizableStyle(to: window, resizable: resizable)
        }

        // 4. TitleBarStyle & Traffic Lights Offset.
        if let offsetX = (args["trafficLightsOffsetX"] as? NSNumber)?.doubleValue,
           let offsetY = (args["trafficLightsOffsetY"] as? NSNumber)?.doubleValue
        {
            trafficLightsOffset = CGPoint(x: offsetX, y: offsetY)
        }
        if let visible = args["trafficLightsVisible"] as? Bool {
            trafficLightsVisible = visible
        }
        if let style = args["titleBarStyle"] as? String {
            applyTitleBarStyle(to: window, style: style)
        }

        // 5. Min / Max Constraints.
        var minWidth = window.minSize.width
        var minHeight = window.minSize.height
        if let mw = (args["minWidth"] as? NSNumber)?.doubleValue,
           let mh = (args["minHeight"] as? NSNumber)?.doubleValue
        {
            minWidth = CGFloat(mw)
            minHeight = CGFloat(mh)
            window.minSize = CGSize(width: minWidth, height: minHeight)
        }

        var maxWidth = window.maxSize.width
        var maxHeight = window.maxSize.height
        if let xw = (args["maxWidth"] as? NSNumber)?.doubleValue,
           let xh = (args["maxHeight"] as? NSNumber)?.doubleValue
        {
            maxWidth = CGFloat(xw)
            maxHeight = CGFloat(xh)
            window.maxSize = CGSize(width: maxWidth, height: maxHeight)
        }

        // 6. Compute Target Geometry in Memory (Size -> Clamp to Min/Max -> Center or Position).
        var targetSize = window.frame.size
        if let w = (args["width"] as? NSNumber)?.doubleValue,
           let h = (args["height"] as? NSNumber)?.doubleValue
        {
            targetSize = Self.clampedWindowSize(
                width: CGFloat(w),
                height: CGFloat(h),
                minSize: CGSize(width: minWidth, height: minHeight),
                maxSize: CGSize(width: maxWidth, height: maxHeight)
            )
        }

        let shouldCenter = args["center"] as? Bool ?? false
        var targetFrame: NSRect
        if shouldCenter {
            targetFrame = computeCenteredFrame(for: window, targetSize: targetSize)
        } else if let posX = (args["x"] as? NSNumber)?.doubleValue,
                  let posY = (args["y"] as? NSNumber)?.doubleValue
        {
            targetFrame = convertTopLeftToCocoaFrame(
                x: CGFloat(posX),
                y: CGFloat(posY),
                width: targetSize.width,
                height: targetSize.height,
                referenceWindow: window
            )
        } else {
            // Keep top-left corner anchored while updating size.
            let topY = window.frame.maxY
            targetFrame = NSRect(
                x: window.frame.origin.x,
                y: topY - targetSize.height,
                width: targetSize.width,
                height: targetSize.height
            )
        }

        // 7. Apply Frame in a Single Atomic Call.
        window.setFrame(targetFrame, display: true, animate: false)
        updateTrafficLightsLayout(in: window)

        // 8. Reveal immediately if requested.
        if showWhenReady {
            markWindowReadyToShow(window: window)
        }

        return buildWindowStateSnapshot(window: window)
    }

    func markWindowReadyToShow(window: NSWindow) {
        isWaitingForFirstFrame = false
        window.alphaValue = configuredOpacity
        window.makeKeyAndOrderFront(nil)
        NSApplication.shared.activate(ignoringOtherApps: true)
        updateTrafficLightsLayout(in: window)
    }
}

import Cocoa
import FlutterMacOS

/// Production-grade macOS native window manager plugin (`Swift 5.9+`, SPM & CocoaPods compatible).
///
/// Key capabilities:
/// 1. **Zero-Flash First Frame**: Supports hiding or setting `alphaValue = 0` before the first
///    Flutter frame is rasterized, synchronizing `NSWindow.backgroundColor` and `FlutterView` layer
///    background color to eliminate black/white flashes on launch and during edge resizing.
/// 2. **Atomic Window Configuration**: Processes `WindowConfig` in a single main-thread transaction
///    (applying `styleMask`, `minSize`, `maxSize`, target `size`, centered/custom `origin`,
///    `backgroundColor`, `level`, and traffic light offsets before performing a single `setFrame`).
/// 3. **Custom Traffic Lights Controller**: Dynamically adjusts the `(x, y)` offset of the standard
///    macOS window buttons (`closeButton`, `miniaturizeButton`, `zoomButton`) from the top-left
///    corner of the window, automatically preserving their positions across live resize,
///    style transitions, and full-screen entry/exit.
/// 4. **NSWindowDelegate Lifecycle**: Forwards `windowShouldClose` (when `preventClose` is enabled),
///    `windowDidResize`, `windowDidMove`, `windowDidMiniaturize`, `windowDidDeminiaturize`,
///    `windowDidEnterFullScreen`, `windowDidExitFullScreen`, `windowDidBecomeKey`, and
///    `windowDidResignKey` to Dart over an `EventChannel`.
public final class XueHuaNaviteWindowManagerPlugin: NSObject, FlutterPlugin, FlutterStreamHandler, NSWindowDelegate {
    // MARK: - Properties

    private weak var registrar: FlutterPluginRegistrar?
    private weak var mainWindow: NSWindow?
    private var methodChannel: FlutterMethodChannel?
    private var eventChannel: FlutterEventChannel?
    private var eventSink: FlutterEventSink?

    /// Whether closing the window via the close button or `Cmd+W` is intercepted.
    private var isPreventClose: Bool = false

    /// Whether a force-destroy (`destroy()`) is currently in progress.
    private var isForceClosing: Bool = false

    /// Current title bar style (`normal`, `hidden`, `immersive`).
    private var currentTitleBarStyle: String = "normal"

    /// Custom top-left `(x, y)` offset for the macOS traffic light buttons.
    private var trafficLightsOffset: CGPoint?

    /// Explicit visibility override for traffic light buttons.
    private var trafficLightsVisible: Bool = true

    /// Target window opacity configured by the user (`0.0` ... `1.0`).
    private var configuredOpacity: CGFloat = 1.0

    /// Tracks whether the window is currently concealed waiting for the first Flutter frame.
    private var isWaitingForFirstFrame: Bool = false

    /// Tracks the previous zoomed/maximized state to emit `maximized` / `unmaximized` events.
    private var wasZoomed: Bool = false

    /// Horizontal spacing between consecutive traffic light buttons in points.
    private let trafficLightButtonSpacing: CGFloat = 20.0

    // MARK: - Plugin Registration

    public static func register(with registrar: FlutterPluginRegistrar) {
        let methodChannel = FlutterMethodChannel(
            name: "xue_hua_navite_window_manager/methods",
            binaryMessenger: registrar.messenger
        )
        let eventChannel = FlutterEventChannel(
            name: "xue_hua_navite_window_manager/events",
            binaryMessenger: registrar.messenger
        )

        let instance = XueHuaNaviteWindowManagerPlugin(
            registrar: registrar,
            methodChannel: methodChannel,
            eventChannel: eventChannel
        )
        registrar.addMethodCallDelegate(instance, channel: methodChannel)
        eventChannel.setStreamHandler(instance)
    }

    private init(
        registrar: FlutterPluginRegistrar,
        methodChannel: FlutterMethodChannel,
        eventChannel: FlutterEventChannel
    ) {
        self.registrar = registrar
        self.methodChannel = methodChannel
        self.eventChannel = eventChannel
        super.init()

        DispatchQueue.main.async { [weak self] in
            self?.attachToWindowIfNeeded()
        }
    }

    // MARK: - FlutterStreamHandler

    public func onListen(withArguments _: Any?, eventSink events: @escaping FlutterEventSink) -> FlutterError? {
        eventSink = events
        return nil
    }

    public func onCancel(withArguments _: Any?) -> FlutterError? {
        eventSink = nil
        return nil
    }

    // MARK: - Helper: Prepare Window Before First Frame (Runner Hook)

    /// Optional helper that can be called directly in `MainFlutterWindow.awakeFromNib()`
    /// to conceal the window (`alphaValue = 0.0`) prior to initial AppKit display so there is
    /// zero native frame flicker before `NativeWindowController.configure(...)` runs.
    public static func prepareZeroFlashWindow(_ window: NSWindow, backgroundColor: NSColor? = nil) {
        window.alphaValue = 0.0
        if let bgColor = backgroundColor {
            window.backgroundColor = bgColor
            window.isOpaque = bgColor.alphaComponent >= 1.0
        }
    }

    // MARK: - Window Resolution & Delegate Attachment

    @discardableResult
    private func attachToWindowIfNeeded() -> NSWindow? {
        if let existing = mainWindow {
            if existing.delegate !== self {
                existing.delegate = self
            }
            return existing
        }

        let resolvedWindow: NSWindow? =
            registrar?.view?.window
                ?? NSApplication.shared.mainWindow
                ?? NSApplication.shared.windows.first

        if let window = resolvedWindow {
            mainWindow = window
            wasZoomed = window.isZoomed
            if window.delegate !== self {
                window.delegate = self
            }
        }
        return resolvedWindow
    }

    // MARK: - MethodChannel Dispatch

    public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
        if Thread.isMainThread {
            handleOnMainThread(call, result: result)
        } else {
            DispatchQueue.main.async { [weak self] in
                self?.handleOnMainThread(call, result: result)
            }
        }
    }

    private func handleOnMainThread(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
        guard let window = attachToWindowIfNeeded() else {
            result(
                FlutterError(
                    code: "NO_WINDOW",
                    message: "Unable to resolve main NSWindow instance.",
                    details: nil
                )
            )
            return
        }

        let args = call.arguments as? [String: Any] ?? [:]

        switch call.method {
        case "ensureInitialized":
            result(nil)

        case "configure":
            let snapshot = configureAtomically(window: window, args: args)
            result(snapshot)

        case "markReadyToShow":
            markWindowReadyToShow(window: window)
            result(nil)

        case "getState":
            result(buildWindowStateSnapshot(window: window))

        case "show":
            let focus = args["focus"] as? Bool ?? true
            isWaitingForFirstFrame = false
            window.alphaValue = configuredOpacity
            if focus {
                window.makeKeyAndOrderFront(nil)
                NSApplication.shared.activate(ignoringOtherApps: true)
            } else {
                window.orderFront(nil)
            }
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "hide":
            window.orderOut(nil)
            result(nil)

        case "focus":
            window.makeKeyAndOrderFront(nil)
            NSApplication.shared.activate(ignoringOtherApps: true)
            result(nil)

        case "blur":
            window.orderBack(nil)
            result(nil)

        case "close":
            window.performClose(nil)
            result(nil)

        case "destroy":
            isForceClosing = true
            window.delegate = nil
            window.close()
            result(nil)

        case "maximize":
            if !window.isZoomed {
                window.zoom(nil)
            }
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "unmaximize":
            if window.isZoomed {
                window.zoom(nil)
            }
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "minimize":
            window.miniaturize(nil)
            result(nil)

        case "restore":
            if window.isMiniaturized {
                window.deminiaturize(nil)
            } else if window.isZoomed {
                window.zoom(nil)
            }
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setFullScreen":
            let targetFullScreen = args["isFullScreen"] as? Bool ?? false
            let currentlyFullScreen = window.styleMask.contains(.fullScreen)
            if targetFullScreen != currentlyFullScreen {
                window.toggleFullScreen(nil)
            }
            result(nil)

        case "setBounds":
            let x = (args["x"] as? NSNumber)?.doubleValue ?? Double(window.frame.origin.x)
            let y = (args["y"] as? NSNumber)?.doubleValue ?? 0.0
            let width = (args["width"] as? NSNumber)?.doubleValue ?? Double(window.frame.size.width)
            let height = (args["height"] as? NSNumber)?.doubleValue ?? Double(window.frame.size.height)
            let animate = args["animate"] as? Bool ?? false
            let cocoaFrame = convertTopLeftToCocoaFrame(
                x: CGFloat(x),
                y: CGFloat(y),
                width: CGFloat(width),
                height: CGFloat(height),
                referenceWindow: window
            )
            window.setFrame(cocoaFrame, display: true, animate: animate)
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "getBounds":
            result(extractTopLeftBoundsMap(from: window))

        case "setSize":
            let width = CGFloat((args["width"] as? NSNumber)?.doubleValue ?? Double(window.frame.width))
            let height = CGFloat((args["height"] as? NSNumber)?.doubleValue ?? Double(window.frame.height))
            let animate = args["animate"] as? Bool ?? false
            var frame = window.frame
            // Preserve top-left corner in screen coordinates when resizing.
            let topY = frame.maxY
            frame.size = CGSize(width: width, height: height)
            frame.origin.y = topY - height
            window.setFrame(frame, display: true, animate: animate)
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setPosition":
            let x = CGFloat((args["x"] as? NSNumber)?.doubleValue ?? Double(window.frame.origin.x))
            let y = CGFloat((args["y"] as? NSNumber)?.doubleValue ?? 0.0)
            let animate = args["animate"] as? Bool ?? false
            let frame = convertTopLeftToCocoaFrame(
                x: x,
                y: y,
                width: window.frame.width,
                height: window.frame.height,
                referenceWindow: window
            )
            window.setFrame(frame, display: true, animate: animate)
            result(nil)

        case "center":
            let animate = args["animate"] as? Bool ?? false
            var targetSize = window.frame.size
            if let w = (args["width"] as? NSNumber)?.doubleValue,
               let h = (args["height"] as? NSNumber)?.doubleValue
            {
                targetSize = CGSize(width: w, height: h)
            }
            let centeredFrame = computeCenteredFrame(for: window, targetSize: targetSize)
            window.setFrame(centeredFrame, display: true, animate: animate)
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setMinimumSize":
            if let w = (args["width"] as? NSNumber)?.doubleValue,
               let h = (args["height"] as? NSNumber)?.doubleValue
            {
                window.minSize = CGSize(width: w, height: h)
            }
            result(nil)

        case "setMaximumSize":
            if let w = (args["width"] as? NSNumber)?.doubleValue,
               let h = (args["height"] as? NSNumber)?.doubleValue
            {
                window.maxSize = CGSize(width: w, height: h)
            }
            result(nil)

        case "setResizable":
            let resizable = args["resizable"] as? Bool ?? true
            applyResizableStyle(to: window, resizable: resizable)
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setAlwaysOnTop":
            let alwaysOnTop = args["alwaysOnTop"] as? Bool ?? false
            window.level = alwaysOnTop ? .floating : .normal
            result(nil)

        case "setPreventClose":
            isPreventClose = args["preventClose"] as? Bool ?? false
            result(nil)

        case "setTitle":
            if let title = args["title"] as? String {
                window.title = title
            }
            result(nil)

        case "setTitleBarStyle":
            let style = args["titleBarStyle"] as? String ?? "normal"
            if let offsetX = (args["trafficLightsOffsetX"] as? NSNumber)?.doubleValue,
               let offsetY = (args["trafficLightsOffsetY"] as? NSNumber)?.doubleValue
            {
                trafficLightsOffset = CGPoint(x: offsetX, y: offsetY)
            }
            if let visible = args["trafficLightsVisible"] as? Bool {
                trafficLightsVisible = visible
            } else if style == "immersive", args["trafficLightsVisible"] == nil {
                trafficLightsVisible = true
            }
            applyTitleBarStyle(to: window, style: style)
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setTrafficLightsOffset":
            if let x = (args["x"] as? NSNumber)?.doubleValue,
               let y = (args["y"] as? NSNumber)?.doubleValue
            {
                trafficLightsOffset = CGPoint(x: x, y: y)
                updateTrafficLightsLayout(in: window)
            }
            result(nil)

        case "setTrafficLightsVisible":
            let visible = args["visible"] as? Bool ?? true
            trafficLightsVisible = visible
            updateTrafficLightsLayout(in: window)
            result(nil)

        case "setBackgroundColor":
            if let argb = (args["backgroundColor"] as? NSNumber)?.uint32Value {
                applyBackgroundColor(to: window, argb: argb)
            }
            result(nil)

        case "setOpacity":
            if let opacity = (args["opacity"] as? NSNumber)?.doubleValue {
                configuredOpacity = CGFloat(max(0.0, min(1.0, opacity)))
                if !isWaitingForFirstFrame {
                    window.alphaValue = configuredOpacity
                }
            }
            result(nil)

        case "setHasShadow":
            let hasShadow = args["hasShadow"] as? Bool ?? true
            window.hasShadow = hasShadow
            window.invalidateShadow()
            result(nil)

        case "startDragging":
            performNativeWindowDrag(window: window)
            result(nil)

        case "startResizing":
            // macOS natively handles edge resizing on all window borders when .resizable is in styleMask.
            result(nil)

        case "setSnapLayoutButtonRect", "setTitleBarDragRects":
            // Windows-specific hit-test regions; safely no-op on macOS.
            result(nil)

        default:
            result(FlutterMethodNotImplemented)
        }
    }

    // MARK: - Atomic Window Configuration & Zero-Flash First Frame

    private func configureAtomically(window: NSWindow, args: [String: Any]) -> [String: Any] {
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
            let clampedW = max(minWidth, min(CGFloat(w), maxWidth > 0 ? maxWidth : CGFloat.greatestFiniteMagnitude))
            let clampedH = max(minHeight, min(CGFloat(h), maxHeight > 0 ? maxHeight : CGFloat.greatestFiniteMagnitude))
            targetSize = CGSize(width: clampedW, height: clampedH)
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

    private func markWindowReadyToShow(window: NSWindow) {
        isWaitingForFirstFrame = false
        window.alphaValue = configuredOpacity
        window.makeKeyAndOrderFront(nil)
        NSApplication.shared.activate(ignoringOtherApps: true)
        updateTrafficLightsLayout(in: window)
    }

    // MARK: - TitleBar, Background & Traffic Lights Management

    private func applyBackgroundColor(to window: NSWindow, argb: UInt32) {
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

    private func applyResizableStyle(to window: NSWindow, resizable: Bool) {
        if resizable {
            window.styleMask.insert(.resizable)
        } else {
            window.styleMask.remove(.resizable)
        }
    }

    private func applyTitleBarStyle(to window: NSWindow, style: String) {
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
    private func updateTrafficLightsLayout(in window: NSWindow) {
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

    private func performNativeWindowDrag(window: NSWindow) {
        if let currentEvent = NSApplication.shared.currentEvent {
            window.performDrag(with: currentEvent)
        }
    }

    // MARK: - Coordinate Conversion Helpers (Top-Left DIP <-> AppKit Bottom-Left)

    private func primaryScreenHeight(for window: NSWindow) -> CGFloat {
        if let primaryScreen = NSScreen.screens.first {
            return primaryScreen.frame.height
        }
        return window.screen?.frame.height ?? 1080.0
    }

    private func convertTopLeftToCocoaFrame(
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

    private func extractTopLeftBoundsMap(from window: NSWindow) -> [String: Double] {
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

    private func computeCenteredFrame(for window: NSWindow, targetSize: CGSize) -> NSRect {
        let targetScreen = window.screen ?? NSScreen.main ?? NSScreen.screens.first
        guard let visibleFrame = targetScreen?.visibleFrame else {
            return NSRect(origin: window.frame.origin, size: targetSize)
        }
        let centeredX = visibleFrame.origin.x + (visibleFrame.width - targetSize.width) / 2.0
        let centeredY = visibleFrame.origin.y + (visibleFrame.height - targetSize.height) / 2.0
        return NSRect(x: round(centeredX), y: round(centeredY), width: targetSize.width, height: targetSize.height)
    }

    private func buildWindowStateSnapshot(window: NSWindow) -> [String: Any] {
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

    // MARK: - Event Emission

    private func emitWindowEvent(type: String, window: NSWindow) {
        guard let sink = eventSink else { return }
        var payload: [String: Any] = extractTopLeftBoundsMap(from: window)
        payload["type"] = type
        payload["isMaximized"] = window.isZoomed
        payload["isFullScreen"] = window.styleMask.contains(.fullScreen)
        payload["timestamp"] = Int(Date().timeIntervalSince1970 * 1000)
        sink(payload)
    }

    // MARK: - NSWindowDelegate Implementation

    public func windowShouldClose(_ sender: NSWindow) -> Bool {
        if isForceClosing {
            return true
        }
        if isPreventClose {
            emitWindowEvent(type: "closeRequested", window: sender)
            return false
        }
        return true
    }

    public func windowDidResize(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        updateTrafficLightsLayout(in: window)

        let currentlyZoomed = window.isZoomed
        if currentlyZoomed != wasZoomed {
            wasZoomed = currentlyZoomed
            emitWindowEvent(type: currentlyZoomed ? "maximized" : "unmaximized", window: window)
        }
        emitWindowEvent(type: "resized", window: window)
    }

    public func windowDidMove(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        emitWindowEvent(type: "moved", window: window)
    }

    public func windowDidMiniaturize(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        emitWindowEvent(type: "minimized", window: window)
    }

    public func windowDidDeminiaturize(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        updateTrafficLightsLayout(in: window)
        emitWindowEvent(type: "restored", window: window)
    }

    public func windowDidEnterFullScreen(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        updateTrafficLightsLayout(in: window)
        emitWindowEvent(type: "enteredFullScreen", window: window)
    }

    public func windowDidExitFullScreen(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        // Re-apply custom traffic lights layout after macOS exits native full-screen space.
        DispatchQueue.main.async { [weak self, weak window] in
            guard let self = self, let window = window else { return }
            self.updateTrafficLightsLayout(in: window)
        }
        emitWindowEvent(type: "exitedFullScreen", window: window)
    }

    public func windowDidBecomeKey(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        updateTrafficLightsLayout(in: window)
        emitWindowEvent(type: "focused", window: window)
    }

    public func windowDidResignKey(_ notification: Notification) {
        guard let window = notification.object as? NSWindow else { return }
        emitWindowEvent(type: "blurred", window: window)
    }
}

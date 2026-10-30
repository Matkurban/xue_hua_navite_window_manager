import Cocoa
import FlutterMacOS

extension XueHuaNaviteWindowManagerPlugin {
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

    func handleOnMainThread(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
        let args = call.arguments as? [String: Any] ?? [:]

        // Application-level methods do not require an active NSWindow.
        switch call.method {
        case "ensureInitialized":
            attachToWindowIfNeeded()
            result(nil)
            return

        case "getLaunchAtStartupState":
            result(XueHuaLaunchAtStartup.getState(args: args))
            return

        case "setLaunchAtStartup":
            XueHuaLaunchAtStartup.setEnabled(args: args, result: result)
            return

        case "openLaunchAtStartupSettings":
            result(XueHuaLaunchAtStartup.openSettings())
            return

        case "setSnapLayoutButtonRect", "setTitleBarDragRects", "setLinuxInteractiveRegion":
            result(nil)
            return

        default:
            break
        }

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

        switch call.method {
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
            // orderOut leaves no visible window. Hosts that return true from
            // applicationShouldTerminateAfterLastWindowClosed (the Flutter
            // template) quit here, so a later show() never runs. Return false
            // from that method when hide/show must keep the process alive.
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
            let clamped = Self.clampedWindowSize(
                width: CGFloat(width),
                height: CGFloat(height),
                minSize: window.minSize,
                maxSize: window.maxSize
            )
            let cocoaFrame = convertTopLeftToCocoaFrame(
                x: CGFloat(x),
                y: CGFloat(y),
                width: clamped.width,
                height: clamped.height,
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
            let clamped = Self.clampedWindowSize(
                width: width,
                height: height,
                minSize: window.minSize,
                maxSize: window.maxSize
            )
            var frame = window.frame
            // Preserve top-left corner in screen coordinates when resizing.
            let topY = frame.maxY
            frame.size = clamped
            frame.origin.y = topY - clamped.height
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
                targetSize = Self.clampedWindowSize(
                    width: CGFloat(w),
                    height: CGFloat(h),
                    minSize: window.minSize,
                    maxSize: window.maxSize
                )
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

        default:
            result(FlutterMethodNotImplemented)
        }
    }
}

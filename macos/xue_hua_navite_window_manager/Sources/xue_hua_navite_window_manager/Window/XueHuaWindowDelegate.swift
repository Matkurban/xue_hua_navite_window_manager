import Cocoa

extension XueHuaNaviteWindowManagerPlugin: NSWindowDelegate {
    // MARK: - Event Emission

    func emitWindowEvent(type: String, window: NSWindow) {
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

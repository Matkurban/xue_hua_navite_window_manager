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
public final class XueHuaNaviteWindowManagerPlugin: NSObject, FlutterPlugin, FlutterStreamHandler {
    // MARK: - Properties

    weak var registrar: FlutterPluginRegistrar?
    weak var mainWindow: NSWindow?
    var methodChannel: FlutterMethodChannel?
    var eventChannel: FlutterEventChannel?
    var eventSink: FlutterEventSink?

    /// Whether closing the window via the close button or `Cmd+W` is intercepted.
    var isPreventClose: Bool = false

    /// Whether a force-destroy (`destroy()`) is currently in progress.
    var isForceClosing: Bool = false

    /// Current title bar style (`normal`, `hidden`, `immersive`).
    var currentTitleBarStyle: String = "normal"

    /// Custom top-left `(x, y)` offset for the macOS traffic light buttons.
    var trafficLightsOffset: CGPoint?

    /// Explicit visibility override for traffic light buttons.
    var trafficLightsVisible: Bool = true

    /// Target window opacity configured by the user (`0.0` ... `1.0`).
    var configuredOpacity: CGFloat = 1.0

    /// Tracks whether the window is currently concealed waiting for the first Flutter frame.
    var isWaitingForFirstFrame: Bool = false

    /// Tracks the previous zoomed/maximized state to emit `maximized` / `unmaximized` events.
    var wasZoomed: Bool = false

    /// Horizontal spacing between consecutive traffic light buttons in points.
    let trafficLightButtonSpacing: CGFloat = 20.0

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
    func attachToWindowIfNeeded() -> NSWindow? {
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
}

import Cocoa
import FlutterMacOS
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
}

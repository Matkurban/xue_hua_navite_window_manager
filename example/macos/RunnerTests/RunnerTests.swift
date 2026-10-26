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
}

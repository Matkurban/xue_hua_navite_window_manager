// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "xue_hua_navite_window_manager",
    platforms: [
        .macOS("12.0"),
    ],
    products: [
        .library(name: "xue-hua-navite-window-manager", targets: ["xue_hua_navite_window_manager"]),
    ],
    dependencies: [
        .package(name: "FlutterFramework", path: "../FlutterFramework"),
    ],
    targets: [
        .target(
            name: "xue_hua_navite_window_manager",
            dependencies: [
                .product(name: "FlutterFramework", package: "FlutterFramework"),
            ],
            resources: [
                .process("PrivacyInfo.xcprivacy"),
            ]
        ),
    ]
)

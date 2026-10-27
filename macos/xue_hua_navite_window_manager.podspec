#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint xue_hua_navite_window_manager.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'xue_hua_navite_window_manager'
  s.version          = '1.0.0'
  s.summary          = 'High-performance cross-platform desktop window manager plugin for Flutter.'
  s.description      = <<-DESC
High-performance cross-platform desktop window manager plugin for Flutter (macOS, Windows, Linux) featuring zero-flash startup, atomic window configuration, custom frameless window & DWM shadow/Snap Layouts, and macOS traffic lights positioning.
                       DESC
  s.homepage         = 'https://github.com/Matkurban/xue_hua_navite_window_manager'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Matkurban' => '3496354336@qq.com' }

  s.source           = { :path => '.' }
  s.source_files = 'xue_hua_navite_window_manager/Sources/xue_hua_navite_window_manager/**/*'
  s.resource_bundles = {'xue_hua_navite_window_manager_privacy' => ['xue_hua_navite_window_manager/Sources/xue_hua_navite_window_manager/PrivacyInfo.xcprivacy']}

  s.dependency 'FlutterMacOS'

  s.platform = :osx, '12.0'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.9'
end

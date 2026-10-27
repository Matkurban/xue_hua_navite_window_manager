#!/usr/bin/env bash
#
# Fails on ANY warning from the plugin's macOS native code (macos/) and from the
# example macOS app: swiftc, clang, the linker, asset catalog and xib compilers,
# SwiftPM manifests, the Xcode project, and xcodebuild itself.
#
# Needs a Mac with Xcode and Flutter on PATH. macOS only (no iOS).
#
#   tool/check_macos_native_warnings.sh                  # example plugin podspec pods
#   tool/check_macos_native_warnings.sh example debug    # one check, one mode
#   tool/check_macos_native_warnings.sh self-test        # prove the gate can fail
#
# Checks:
#   example    Clean debug and release builds of the example app, plus the
#              RunnerTests XCTest target. Covers the Runner target, the Xcode
#              project, and the SwiftPM manifests.
#   plugin     The plugin's own Swift package, built as the root package with
#              `swift build`. SwiftPM suppresses warnings from package
#              dependencies, and in the example app the plugin is one, so this
#              is where diagnostics in the plugin's Swift sources show up.
#   podspec    The podspec evaluates without errors or warnings.
#   pods       A scratch app that depends on the plugin through CocoaPods, built
#              in debug and release. CocoaPods does not hide pod warnings. Only
#              diagnostics that name the plugin count here: the fresh app
#              template and CocoaPods itself have their own.
#   self-test  Injects a deliberate warning into a copy of the plugin and
#              requires the plugin check to report it, so the gate cannot go
#              blind without anyone noticing.
#
# Modes (debug, release, profile) apply to the example check. The plugin and
# pods checks build debug and release.
#
# CI runs this same script (.github/workflows/ci.yml).
set -euo pipefail

usage() {
  sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

checks=()
modes=()
for arg in "$@"; do
  case "$arg" in
    example|plugin|podspec|pods|self-test) checks+=("$arg") ;;
    debug|release|profile) modes+=("$arg") ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "error: unknown argument '$arg'." >&2
      usage >&2
      exit 2
      ;;
  esac
done
if [[ ${#checks[@]} -eq 0 ]]; then checks=(example plugin podspec pods); fi
if [[ ${#modes[@]} -eq 0 ]]; then modes=(debug release); fi

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: this check needs macOS and Xcode." >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
example_dir="$repo_root/example"
macos_dir="$example_dir/macos"
plugin_name="xue_hua_navite_window_manager"
plugin_package="$repo_root/macos/$plugin_name"
plugin_source="$plugin_package/Sources/$plugin_name/XueHuaNaviteWindowManagerPlugin.swift"
log_dir="${XUE_HUA_WARNING_LOG_DIR:-$repo_root/build/macos-warnings}"
mkdir -p "$log_dir"

# Matches "file:1:2: warning: ...", "ld: warning: ...", "warning: ..." and
# "xcodebuild: WARNING: ...". Capitalised Flutter tool notices ("Warning: ...")
# are matched too, on purpose: they point at Xcode project settings that need
# attention. "note:" lines are informational and not matched.
warning_pattern='(^|[^[:alnum:]_])warning:'

failed=0

# scan <log> <label> [name]: prints every warning line in <log> and records a
# failure. With [name], only warning lines that mention it count.
scan() {
  local log="$1" label="$2" name="${3:-}"
  if [[ -n "$name" ]]; then
    grep -niE "$warning_pattern" "$log" | grep -i "$name" >"$log.warnings" || true
  else
    grep -niE "$warning_pattern" "$log" >"$log.warnings" || true
  fi
  if [[ -s "$log.warnings" ]]; then
    echo "FAIL ($label): warnings found:" >&2
    cat "$log.warnings" >&2
    failed=1
  else
    echo "OK ($label): no warnings."
  fi
}

# run_logged <log> <command...>: runs the command, tees its output into <log>
# and returns its exit status.
run_logged() {
  local log="$1" status
  shift
  set +e
  "$@" 2>&1 | tee "$log"
  status=${PIPESTATUS[0]}
  set -e
  return "$status"
}

check_example() {
  local mode label log arch
  arch="$(uname -m)"
  for mode in "${modes[@]}"; do
    label="example/$mode"
    log="$log_dir/example-$mode.log"
    echo "==> $label: clean build"

    # Xcode only reports compiler warnings for files it recompiles. A warm
    # build would hide them, so always start from an empty build directory.
    rm -rf "$example_dir/build/macos"
    (cd "$example_dir" && flutter pub get >/dev/null)

    if ! run_logged "$log" bash -c "cd '$example_dir' && flutter build macos --$mode"; then
      echo "FAIL ($label): flutter build macos failed." >&2
      failed=1
      continue
    fi
    scan "$log" "$label"

    if [[ "$mode" == "debug" ]]; then
      # Compiles RunnerTests, the plugin's XCTest target, against the build that
      # was just produced. -quiet prints only warnings and errors. The explicit
      # architecture avoids xcodebuild's "multiple matching destinations" warning.
      label="example/build-for-testing"
      log="$log_dir/example-build-for-testing.log"
      echo "==> $label"
      if ! run_logged "$log" xcodebuild build-for-testing \
        -workspace "$macos_dir/Runner.xcworkspace" \
        -scheme Runner \
        -configuration Debug \
        -derivedDataPath "$example_dir/build/macos" \
        -destination "platform=macOS,arch=$arch" \
        -quiet; then
        echo "FAIL ($label): xcodebuild build-for-testing failed." >&2
        failed=1
      else
        scan "$log" "$label"
      fi
    fi
  done

  echo "==> example: tracked files changed by the build (expect none):"
  git -C "$repo_root" status --short -- macos example/macos
}

# engine_dir: prints the Flutter engine's macOS artifact directory, downloading
# the artifacts first if needed.
engine_dir() {
  local flutter_root
  flutter precache --macos >/dev/null 2>&1 || true
  flutter_root="$(flutter --version --machine | sed -n 's/.*"flutterRoot": *"\([^"]*\)".*/\1/p' | head -1)"
  echo "$flutter_root/bin/cache/artifacts/engine/darwin-x64"
}

# engine_framework_search_path: prints the directory that holds
# FlutterMacOS.framework in the Flutter engine artifacts, for swiftc's -F flag.
# In the example app the same framework is copied next to the build products by
# Flutter's "prepare" step, which is how `import FlutterMacOS` resolves there.
engine_framework_search_path() {
  local xcframework framework
  xcframework="$(engine_dir)/FlutterMacOS.xcframework"
  framework="$(find "$xcframework" -maxdepth 2 -type d -name FlutterMacOS.framework 2>/dev/null | head -1)"
  if [[ -z "$framework" ]]; then
    echo "error: FlutterMacOS.framework not found under $xcframework." >&2
    return 1
  fi
  dirname "$framework"
}

# stage_plugin <dir>: lays out <dir>/<plugin>, a copy of the plugin's Swift
# package, next to <dir>/FlutterFramework, which is where its Package.swift
# looks for it. Flutter generates that package for the example app; it is an
# empty target that only orders the build, so a stand-in with the same product
# name is equivalent here.
stage_plugin() {
  local stage="$1"
  rm -rf "$stage"
  mkdir -p "$stage/FlutterFramework/Sources/FlutterFramework"
  cat >"$stage/FlutterFramework/Package.swift" <<'PACKAGE'
// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "FlutterFramework",
    products: [.library(name: "FlutterFramework", targets: ["FlutterFramework"])],
    targets: [.target(name: "FlutterFramework")]
)
PACKAGE
  : >"$stage/FlutterFramework/Sources/FlutterFramework/FlutterFramework.swift"
  cp -R "$plugin_package" "$stage/$plugin_name"
  rm -rf "$stage/$plugin_name/.build" "$stage/$plugin_name/.swiftpm"
}

# build_plugin <stage> <log-prefix>: swift build in every configuration. The
# plugin is the root package, so SwiftPM reports its warnings. Returns 1 when a
# build fails; warnings are left in "<log-prefix>-<config>.log" for the caller.
build_plugin() {
  local stage="$1" prefix="$2" config frameworks
  frameworks="$(engine_framework_search_path)" || { failed=1; return 1; }
  for config in debug release; do
    echo "==> plugin/$config: swift build"
    if ! run_logged "$prefix-$config.log" \
      swift build -c "$config" --package-path "$stage/$plugin_name" \
      -Xswiftc -F -Xswiftc "$frameworks"; then
      echo "FAIL (plugin/$config): swift build failed." >&2
      failed=1
    fi
  done
}

check_plugin() {
  local stage="$log_dir/plugin-stage" config
  stage_plugin "$stage"
  build_plugin "$stage" "$log_dir/plugin"
  for config in debug release; do
    scan "$log_dir/plugin-$config.log" "plugin/$config"
  done
}

check_podspec() {
  local spec="$repo_root/macos/$plugin_name.podspec" log="$log_dir/podspec.log"
  echo "==> podspec"
  if ! command -v pod >/dev/null; then
    echo "FAIL (podspec): CocoaPods is not installed." >&2
    failed=1
    return
  fi

  # CocoaPods needs a UTF-8 locale.
  export LANG="${LANG:-en_US.UTF-8}" LC_ALL="${LC_ALL:-en_US.UTF-8}"
  if ! pod ipc spec "$spec" >"$log.json" 2>"$log"; then
    echo "FAIL (podspec): pod ipc spec failed:" >&2
    cat "$log" "$log.json" >&2
    failed=1
  elif [[ -s "$log" ]]; then
    echo "FAIL (podspec): CocoaPods printed diagnostics:" >&2
    cat "$log" >&2
    failed=1
  else
    echo "OK (podspec): evaluates without diagnostics."
  fi
}

check_pods() {
  local app="$log_dir/pods-consumer" mode log
  echo "==> pods: scratch app that uses the plugin through CocoaPods"
  if ! command -v pod >/dev/null; then
    echo "FAIL (pods): CocoaPods is not installed." >&2
    failed=1
    return
  fi
  export LANG="${LANG:-en_US.UTF-8}" LC_ALL="${LC_ALL:-en_US.UTF-8}"

  rm -rf "$app"
  if ! run_logged "$log_dir/pods-create.log" flutter create --platforms=macos \
    --project-name pods_consumer "$app" >/dev/null ||
    ! (cd "$app" && run_logged "$log_dir/pods-add.log" flutter pub add \
      "$plugin_name" --path "$repo_root" >/dev/null); then
    echo "FAIL (pods): could not create the scratch app." >&2
    failed=1
    return
  fi

  # Opt this one project out of Swift Package Manager, so Flutter integrates the
  # plugin through its podspec. The global Flutter setting is left alone.
  perl -0pi -e 's/^flutter:\n/flutter:\n  config:\n    enable-swift-package-manager: false\n/m' "$app/pubspec.yaml"
  (cd "$app" && flutter pub get >/dev/null)

  for mode in debug release; do
    log="$log_dir/pods-$mode.log"
    echo "==> pods/$mode: clean build"
    rm -rf "$app/build/macos" "$app/macos/Pods"
    if ! run_logged "$log" bash -c "cd '$app' && flutter build macos --$mode"; then
      echo "FAIL (pods/$mode): flutter build macos failed." >&2
      failed=1
      continue
    fi
    scan "$log" "pods/$mode" "$plugin_name"
  done
}

self_test() {
  local stage="$log_dir/self-test-stage" canary_log previous="$failed"
  echo "==> self-test: the plugin check must report an injected warning"
  stage_plugin "$stage"
  cat >>"$stage/$plugin_name/Sources/$plugin_name/$(basename "$plugin_source")" <<'EOF'

// Injected by tool/check_macos_native_warnings.sh self-test.
func xueHuaWarningGateCanary() {
    var canaryNeverMutated = 1
    _ = canaryNeverMutated
}
EOF
  failed=0
  build_plugin "$stage" "$log_dir/self-test"
  canary_log="$log_dir/self-test-debug.log"
  if [[ $failed -eq 0 ]] && grep -q "canaryNeverMutated" "$canary_log" && grep -qiE "$warning_pattern" "$canary_log"; then
    echo "OK (self-test): the injected warning was reported."
    failed=$previous
  else
    echo "FAIL (self-test): the injected warning was NOT reported (or the build failed); the gate is blind." >&2
    failed=1
  fi
}

for check in "${checks[@]}"; do
  case "$check" in
    example) check_example ;;
    plugin) check_plugin ;;
    podspec) check_podspec ;;
    pods) check_pods ;;
    self-test) self_test ;;
  esac
done

if [[ $failed -ne 0 ]]; then
  echo "macOS native check FAILED. Logs: $log_dir" >&2
  exit 1
fi

echo "macOS native check passed."

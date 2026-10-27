#!/usr/bin/env bash
#
# Builds the example macOS app from scratch and fails on ANY warning produced
# by the plugin's native code (macos/), the example Runner, or the Xcode
# project: swiftc, clang, the linker, asset catalog and xib compilers, the
# SwiftPM manifests, CocoaPods, and xcodebuild itself.
#
# Needs a Mac with Xcode and Flutter on PATH. macOS only (no iOS).
#
#   tool/check_macos_native_warnings.sh                # spm + pods, debug + release
#   tool/check_macos_native_warnings.sh pods debug     # one variant, one mode
#
#   spm                      Flutter's default Swift Package Manager integration.
#   pods                     CocoaPods integration (the plugin is built from its
#                            podspec). Temporarily disables the global Flutter
#                            setting enable-swift-package-manager and restores it.
#   debug|release|profile    Build configurations (default: debug release).
#
# SwiftPM suppresses warnings from package dependencies, and the plugin is a
# dependency of the package Flutter generates. The spm variant therefore covers
# the Runner target, the Xcode project, the SwiftPM manifests and the XCTest
# target; the pods variant is what surfaces warnings in the plugin's own Swift
# sources. Both must be clean.
#
# CI runs this same script (.github/workflows/ci.yml).
set -euo pipefail

usage() {
  sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
example_dir="$repo_root/example"
macos_dir="$example_dir/macos"
log_dir="${XUE_HUA_WARNING_LOG_DIR:-$repo_root/build/macos-warnings}"
mkdir -p "$log_dir"

variants=()
modes=()
for arg in "$@"; do
  case "$arg" in
    spm|pods) variants+=("$arg") ;;
    debug|release|profile) modes+=("$arg") ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "error: unknown argument '$arg'." >&2
      usage >&2
      exit 2
      ;;
  esac
done
if [[ ${#variants[@]} -eq 0 ]]; then variants=(spm pods); fi
if [[ ${#modes[@]} -eq 0 ]]; then modes=(debug release); fi

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: this check needs macOS and Xcode." >&2
  exit 2
fi

# Matches "file:1:2: warning: ...", "ld: warning: ...", "warning: ..." and
# "xcodebuild: WARNING: ...". Capitalised Flutter tool notices ("Warning: ...")
# are matched too, on purpose: they point at Xcode project settings that need
# attention.
warning_pattern='(^|[^[:alnum:]_])warning:'

created_podfile=0
spm_disabled=0
cleanup() {
  if [[ $spm_disabled -eq 1 ]]; then
    flutter config --enable-swift-package-manager >/dev/null 2>&1 || true
  fi
  if [[ $created_podfile -eq 1 ]]; then
    rm -f "$macos_dir/Podfile" "$macos_dir/Podfile.lock"
    rm -rf "$macos_dir/Pods"
  fi
}
trap cleanup EXIT

failed=0

# scan <log> <label>: prints every warning line in <log> and records a failure.
scan() {
  local log="$1" label="$2"
  if grep -niE "$warning_pattern" "$log" >"$log.warnings"; then
    echo "FAIL ($label): warnings found:" >&2
    cat "$log.warnings" >&2
    failed=1
  else
    echo "OK ($label): no warnings."
  fi
}

# clean_build <variant>: forget everything Xcode and Flutter cached. Xcode only
# reports compiler warnings for files it recompiles, so a warm build would
# hide them.
clean_build() {
  rm -rf "$example_dir/build/macos" "$macos_dir/Pods" "$macos_dir/Podfile.lock"
  (cd "$example_dir" && flutter pub get >/dev/null)
}

enable_variant() {
  case "$1" in
    spm)
      flutter config --enable-swift-package-manager >/dev/null
      ;;
    pods)
      flutter config --no-enable-swift-package-manager >/dev/null
      spm_disabled=1
      if [[ ! -f "$macos_dir/Podfile" ]]; then
        created_podfile=1
      fi
      ;;
  esac
}

for variant in "${variants[@]}"; do
  enable_variant "$variant"

  for mode in "${modes[@]}"; do
    label="$variant/$mode"
    log="$log_dir/build-$variant-$mode.log"
    echo "==> $label: clean build"
    clean_build

    set +e
    (cd "$example_dir" && flutter build macos "--$mode") 2>&1 | tee "$log"
    build_status=${PIPESTATUS[0]}
    set -e

    if [[ $build_status -ne 0 ]]; then
      echo "FAIL ($label): flutter build macos exited with status $build_status." >&2
      failed=1
      continue
    fi
    scan "$log" "$label"

    if [[ "$mode" == "debug" ]]; then
      # Compiles RunnerTests (the plugin's XCTest target) against the build
      # that was just produced. -quiet prints only warnings and errors.
      label="$variant/build-for-testing"
      log="$log_dir/build-for-testing-$variant.log"
      echo "==> $label"
      set +e
      xcodebuild build-for-testing \
        -workspace "$macos_dir/Runner.xcworkspace" \
        -scheme Runner \
        -configuration Debug \
        -derivedDataPath "$example_dir/build/macos" \
        -destination 'platform=macOS' \
        -quiet 2>&1 | tee "$log"
      test_status=${PIPESTATUS[0]}
      set -e
      if [[ $test_status -ne 0 ]]; then
        echo "FAIL ($label): xcodebuild exited with status $test_status." >&2
        failed=1
      else
        scan "$log" "$label"
      fi
    fi
  done

  echo "==> $variant: tracked files changed by the build (expect none):"
  git -C "$repo_root" status --short -- macos example/macos
done

if [[ $failed -ne 0 ]]; then
  echo "macOS native build is NOT warning-free. Logs: $log_dir" >&2
  exit 1
fi

echo "macOS native build is warning-free."

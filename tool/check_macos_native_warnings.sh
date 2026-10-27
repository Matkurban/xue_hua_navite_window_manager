#!/usr/bin/env bash
#
# Builds the example macOS app from scratch and fails on ANY warning produced
# by the plugin's native code (macos/), the example Runner, or the Xcode
# project: swiftc, clang, the linker, asset catalog and xib compilers, the
# SwiftPM manifest, and xcodebuild itself.
#
# Needs a Mac with Xcode and Flutter on PATH. macOS only (no iOS).
#
#   tool/check_macos_native_warnings.sh            # debug and release
#   tool/check_macos_native_warnings.sh debug      # one configuration
#
# CI runs this same script (.github/workflows/ci.yml).
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: this check needs macOS and Xcode." >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
example_dir="$repo_root/example"
log_dir="${XUE_HUA_WARNING_LOG_DIR:-$repo_root/build/macos-warnings}"
mkdir -p "$log_dir"

if [[ $# -gt 0 ]]; then
  modes=("$@")
else
  modes=(debug release)
fi

cd "$example_dir"

# Matches "file:1:2: warning: ...", "ld: warning: ...", "warning: ..." and
# "xcodebuild: WARNING: ...". Flutter tool notices are capitalised "Warning:"
# without a preceding diagnostic prefix and are matched too, on purpose: they
# point at Xcode project settings that need attention.
warning_pattern='(^|[^[:alnum:]_])warning:'

failed=0
for mode in "${modes[@]}"; do
  case "$mode" in
    debug|release|profile) ;;
    *)
      echo "error: unknown build mode '$mode' (use debug, release or profile)." >&2
      exit 2
      ;;
  esac

  log="$log_dir/build-$mode.log"
  echo "==> $mode: clean build"

  # Xcode only reports compiler warnings for files it recompiles. A cached
  # build would hide them, so always start from an empty build directory.
  rm -rf "$example_dir/build/macos"
  flutter pub get >/dev/null

  set +e
  flutter build macos "--$mode" 2>&1 | tee "$log"
  build_status=${PIPESTATUS[0]}
  set -e

  if [[ $build_status -ne 0 ]]; then
    echo "FAIL ($mode): flutter build macos exited with status $build_status." >&2
    failed=1
    continue
  fi

  if grep -niE "$warning_pattern" "$log" >"$log.warnings"; then
    echo "FAIL ($mode): the macOS build printed warnings:" >&2
    cat "$log.warnings" >&2
    failed=1
  else
    echo "OK ($mode): no warnings."
  fi
done

if [[ $failed -ne 0 ]]; then
  echo "macOS native build is NOT warning-free. Logs: $log_dir" >&2
  exit 1
fi

echo "macOS native build is warning-free."

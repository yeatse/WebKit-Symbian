#!/usr/bin/env bash
# Runs every step: the E32 converter, ICU, JavaScriptCore and jsc.exe, the
# WebCore libraries and WebCore, browser.exe and the SIS packages. Output goes
# to WebKitBuild/Symbian (SYMBIAN_WEBKIT_WORK).
# Needs SYMBIAN_SDK_PREFIX (an installed symbian-platform 0.3.0 SDK; default
# WebKitBuild/Symbian/symbian-sdk), plus host clang, lld, cmake, ninja, ruby,
# perl, gperf, flex, bison, unifdef and autotools. See README.md.
#
#   Tools/Symbian/build.sh          everything
#   Tools/Symbian/build.sh jsc      jsc.exe only
. "$(dirname "$0")/scripts/common.sh"
require_sdk
mkdir -p "$WORK"
step "symbian-native E32 converter";            "$REPO/scripts/build-symbian-native.sh"
step "ICU 63.2";                                "$REPO/scripts/build-icu.sh"
step "JavaScriptCore (JSCOnly, ARMv6 JIT)";     "$REPO/scripts/build-jsc.sh"
step "jsc.exe (E32)";                           "$REPO/scripts/link-e32.sh"
if [ "${1:-}" != jsc ]; then
  if [ ! -f "$DEPS_DIR/install/lib/libcurl.a" ]; then
    step "WebCore libraries";                   "$REPO/scripts/build-deps.sh"
  fi
  step "WebCore (PORT=Symbian)";                "$REPO/scripts/build-webcore.sh"
  step "browser.exe (E32)";                     "$REPO/scripts/build-browser.sh"
fi
step "SIS packages";                            "$REPO/scripts/package-sis.sh"

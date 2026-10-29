#!/usr/bin/env bash
# Run a sampled stress suite with a verified execution tier under ARMv6 QEMU. This does not
# exercise Symbian's allocator, threads, files, or native application shell.
. "$(dirname "$0")/../scripts/common.sh"
[ ! -f "$WORK/qemu/env.sh" ] || . "$WORK/qemu/env.sh"
jsc=$WORK/qemu/build/bin/jsc
stress=$WEBKIT_SRC/JSTests/stress
if [ ! -d "$stress" ]; then
  echo "This checkout does not contain JSTests/stress; supply a full WebKit test checkout." >&2
  exit 1
fi
export jsc TIMEOUT=${TIMEOUT:-120} MODE=${MODE:-dfg}
export QEMU_ARM=${QEMU_ARM:-qemu-arm} ARM11_LINUX_SYSROOT=${ARM11_LINUX_SYSROOT:-/usr/arm-linux-gnueabi}
python3 "$REPO/qemu/run-jit-tests.py" "$jsc" --profile "$MODE" --timeout "$TIMEOUT"
one() {
  cd "$(dirname "$1")" || return
  local result=0
  local options=(--useDollarVM=true --useFTLJIT=false --useConcurrentJIT=false --jitPolicyScale=0.05)
  case "$MODE" in
    llint) options+=(--useJIT=false --useDFGJIT=false) ;;
    baseline) options+=(--useJIT=true --useBaselineJIT=true --useDFGJIT=false) ;;
    dfg) options+=(--useJIT=true --useBaselineJIT=true --useDFGJIT=true) ;;
    *) return 2 ;;
  esac
  timeout "$TIMEOUT" "$QEMU_ARM" -cpu arm1136 -L "$ARM11_LINUX_SYSROOT" \
    "$jsc" "${options[@]}" "$1" > /dev/null 2>&1 || result=$?
  printf '%s %s\n' "$result" "$(basename "$1" .js)"
  return "$result"
}
export -f one
rg --files-without-match '^//@ skip' "$stress" -g '*.js' | sort | awk -v n="${SAMPLE:-1}" '(NR - 1) % n == 0' \
  | xargs -P "$JOBS" -I{} bash -c 'one "$@"' _ {}

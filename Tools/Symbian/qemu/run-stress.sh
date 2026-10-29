#!/usr/bin/env bash
# Run a sampled C_LOOP stress suite under Linux ARMv6 QEMU. This does not
# exercise Symbian's allocator, threads, files, or native application shell.
. "$(dirname "$0")/../scripts/common.sh"
jsc=$WORK/qemu/build/bin/jsc
stress=$WEBKIT_SRC/JSTests/stress
export jsc TIMEOUT=${TIMEOUT:-120}
one() {
  cd "$(dirname "$1")" || return
  local result=0
  timeout "$TIMEOUT" qemu-arm -cpu arm1136 -L /usr/arm-linux-gnueabi \
    "$jsc" --useDollarVM=1 "$1" > /dev/null 2>&1 || result=$?
  printf '%s %s\n' "$result" "$(basename "$1" .js)"
  return "$result"
}
export -f one
grep -L "^//@ skip" "$stress"/*.js | awk -v n="${SAMPLE:-1}" '(NR - 1) % n == 0' \
  | xargs -P "$JOBS" -I{} bash -c 'one "$@"' _ {}

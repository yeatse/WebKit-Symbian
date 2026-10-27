#!/usr/bin/env bash
# Differential run of WebKit's JSTests/stress under qemu-arm (ARM1136): every
# test with the LLInt only, with the JIT tiers forced early but inline caching
# off, and with the JIT tiers and inline caching. Prints
# "<llint> <jit, no IC> <jit> <test>" exit statuses; a test the LLInt passes and
# a JIT configuration fails points at the ARMv6 backend. SAMPLE=4 runs every
# 4th test. Some tests do far more work when $vm.useJIT() is true and need
# TIMEOUT well above the default under emulation.
# Needs JSTests/stress in the WebKit checkout:
#   git -C "$WEBKIT_SRC" sparse-checkout add JSTests/stress
. "$(dirname "$0")/../scripts/common.sh"
jsc=$WORK/qemu/build/bin/jsc
stress=$WEBKIT_SRC/JSTests/stress
export jsc TIMEOUT=${TIMEOUT:-120}
one() {
  cd "$(dirname "$1")"
  q="qemu-arm -cpu arm1136 -L /usr/arm-linux-gnueabi $jsc --useDollarVM=1"
  eager="--jitPolicyScale=0 --useConcurrentJIT=false"
  timeout "$TIMEOUT" $q --useJIT=false "$1" > /dev/null 2>&1; a=$?
  timeout "$TIMEOUT" $q $eager --forceICFailure=true "$1" > /dev/null 2>&1; b=$?
  timeout "$TIMEOUT" $q $eager "$1" > /dev/null 2>&1; c=$?
  echo "$a $b $c $(basename "$1" .js)"
}
export -f one
grep -L "^//@ skip" "$stress"/*.js | awk -v n="${SAMPLE:-1}" 'NR % n == 1 || n == 1' \
  | xargs -P "$JOBS" -I{} bash -c 'one "$@"' _ {}

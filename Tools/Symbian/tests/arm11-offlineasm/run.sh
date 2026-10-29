#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
if [[ -n ${ARM11_OFFLINEASM_BUILD:-} ]]; then
    build=$ARM11_OFFLINEASM_BUILD
    mkdir -p "$build"
else
    build=$(mktemp -d)
    trap 'rm -rf "$build"' EXIT
fi
"${RUBY:-ruby}" "$here/generate.rb" "$root" "$here/probe.asm" "$build"
"${ARM_CC:-arm-linux-gnueabi-gcc-14}" -march=armv6 -marm -mfpu=vfp -mfloat-abi=softfp \
    -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pic -fno-pie \
    -fno-stack-protector -nostdlib -static -no-pie -Wl,-e,_start -Wl,--build-id=none \
    -I"$build" "$here/probe.c" "$here/assembly.c" "$here/start.S" -o "$build/probe"
timeout "${ARM11_OFFLINEASM_TIMEOUT:-20}" "${QEMU_ARM:-qemu-arm}" -cpu arm1136 "$build/probe"

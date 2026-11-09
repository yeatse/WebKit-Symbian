#!/usr/bin/env bash
# Builds the split demo's variants and packages them as split-demo.sis:
#
#   splitdemoa  "Split A"  one EXE, as linked (the control)
#   splitdemob  "Split B"  split-e32.py: a stub EXE and 8 DLLs (sdb0..)
#   splitdemoc  "Split C"  as B, with KImageNoCallEntryPoint on the DLLs
#   splitdemod  "Split D"  as B, but 2 DLLs (sdd0, sdd1)
#
# Each writes C:\Data\splitdemo\<name>.txt and exits after 20 seconds.
. "$(dirname "$0")/../../scripts/common.sh"
require_sdk
converter=${DEMO_CONVERTER:-$SYMBIAN_NATIVE}
[ -x "$converter" ] || converter=$SDK/bin/symbian-native
here=$REPO/tests/split-demo
out=${DEMO_BUILD:-$WORK/build/split-demo}
mkdir -p "$out"
files=()

variant() { # name uid caption max-code(empty: unsplit) prefix dll-uid-base flags
  local name=$1 uid=$2 caption=$3 max_code=$4 prefix=$5 dll_uid=$6 flags=$7
  local build=$out/$name
  "$SDK/bin/cmake" -S "$here" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" -DSYMBIAN_SDK_PREFIX="$SDK" \
    -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" -DCMAKE_BUILD_TYPE=Release \
    -DSYMBIAN_NATIVE_CONVERTER="$converter" \
    -DDEMO_NAME="$name" -DDEMO_UID3="$uid" -DDEMO_CAPTION="$caption" > "$build.configure.log"
  "$SDK/bin/ninja" -C "$build" > "$build.build.log"
  # The browser's header settings.
  python3 "$REPO/scripts/e32-header.py" "$build/e32/$name.exe" 0x100000 0x8000000 0x100000 --fpu=vfpv2
  local images=$build/images
  rm -rf "$images"
  mkdir -p "$images"
  if [ -z "$max_code" ]; then
    cp "$build/e32/$name.exe" "$images/"
  else
    python3 "$REPO/scripts/split-e32.py" --elf "$build/$name.elf" --e32 "$build/e32/$name.exe" \
      --output-dir "$images" --prefix "$prefix" --uid-base "$dll_uid" --max-code "$max_code" --reserve 0x2000
    if [ -n "$flags" ]; then
      python3 "$here/set-e32-flags.py" "$flags" "$images"/*.dll
    fi
  fi
  for image in "$images"/*; do
    files+=(--file "$image=!:\\sys\\bin\\$(basename "$image")")
  done
  files+=(--file "$build/e32/${name}_reg.rsc=!:\\private\\10003a3f\\import\\apps\\${name}_reg.rsc"
    --file "$build/e32/${name}_loc.rsc=!:\\resource\\apps\\${name}_loc.rsc")
}

variant splitdemoa 0xE1A5C1A0 "Split A" "" "" "" ""
variant splitdemob 0xE1A5C1B0 "Split B" 0x30000 sdb 0xE1A5C1B1 ""
variant splitdemoc 0xE1A5C1C0 "Split C" 0x30000 sdc 0xE1A5C1C1 0x2
variant splitdemod 0xE1A5C1D0 "Split D" 0x80000 sdd 0xE1A5C1D1 ""

python3 "$REPO/scripts/make-sis.py" --uid 0xE1A5C1AF --name "Split demo" --vendor symbian-webkit \
  --version 1.0.0 --output "$out/split-demo.sis" \
  --platform 0x20022E6D=S60ProductID --platform 0x20032DE7=S60ProductID "${files[@]}"
ls -l "$out/split-demo.sis" "$out"/*/images

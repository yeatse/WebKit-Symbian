# ARM11 JavaScriptCore checks

Build JavaScriptCore for Linux ARMv6 and run it on QEMU's `arm1136` CPU using
ARM-state code, VFPv2 and the softfp ABI. The default build enables LLInt,
Baseline JIT and DFG.

## Build and run

Use the ICU 70.1 sources and host tools produced by
`Tools/Symbian/scripts/build-icu.sh`:

```sh
export SYMBIAN_WEBKIT_WORK=/absolute/path/to/arm11-work
export ICU_DIR=/absolute/path/to/icu-work
Tools/Symbian/qemu/prepare-tools.sh
. "$SYMBIAN_WEBKIT_WORK/qemu/env.sh"
Tools/Symbian/qemu/build.sh
```

The preparation script extracts the Debian cross compiler and QEMU into the
work directory. The build script creates ARM Linux ICU libraries, builds JSC
and runs the LLInt, Baseline and DFG profiles. Results are written under
`$SYMBIAN_WEBKIT_WORK/qemu/runs`.

For individual build stages:

```sh
Tools/Symbian/qemu/build.sh --dependencies-only
CONFIGURE_ONLY=1 Tools/Symbian/qemu/build.sh
```

## Run an existing build

```sh
python3 Tools/Symbian/qemu/run-jit-tests.py \
  "$SYMBIAN_WEBKIT_WORK/qemu/build/bin/jsc"
```

Use `--profile llint`, `--profile baseline` or `--profile dfg` to select a tier.
The suite exercises JavaScript execution, inline caches, OSR, arithmetic,
garbage collection, buffers and exceptions.

With `JSTests/stress` available, run a sampled stress suite with:

```sh
MODE=dfg SAMPLE=10 Tools/Symbian/qemu/run-stress.sh
```

See [Development](../docs/Development.md) for the Symbian build and
[Testing](../docs/TESTING.md) for the test workflow.

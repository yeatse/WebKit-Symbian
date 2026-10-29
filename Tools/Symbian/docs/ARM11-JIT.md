# ARM11 Baseline JIT and DFG

The `arm11-jit-dfg` work branch restores the traditional ARM backend for
WebKitGTK 2.52.6. Its target is ARMv6 in ARM state, VFPv2, and the softfp/base
procedure-call standard used by Symbian^3. It does not require Thumb-2, NEON,
integer divide, or VFPv3 instructions. FTL and WebAssembly remain disabled.

## Build modes

`SYMBIAN_JSC_MODE` selects the execution tiers for both `build-jsc.sh` and
`build-webcore.sh`:

| Mode | C_LOOP | ARM LLInt | Baseline JIT | DFG |
| --- | --- | --- | --- | --- |
| `dfg` (default) | off | on | on | on |
| `baseline` | off | on | on | off |
| `llint` | off | on | off | off |
| `cloop` | on | off | off | off |

Use a separate work directory to preserve the validated C_LOOP packages:

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/arm11-work
export ICU_DIR=/path/to/existing-work/icu
export DEPS_DIR=/path/to/existing-work/deps
export SYMBIAN_NATIVE_BUILD=/path/to/existing-work/symbian-native
export SYMBIAN_JSC_MODE=dfg
Tools/Symbian/scripts/build-jsc.sh
Tools/Symbian/scripts/link-e32.sh
```

`build-jsc.sh` also accepts `CONFIGURE_ONLY=1`. As with the existing port,
the SDK, ICU, dependencies and patched E32 converter must already be prepared.

## Architecture details

The compiler's `__ARM_ARCH` takes precedence over the SDK's compatibility
macro `__MARM_ARMV5__`. The latter describes SDK header compatibility and does
not mean the emitted code targets ARMv5. The JIT enablement check also requires
ARMv6 or later and hardware VFP support; explicitly disabling JIT is respected.

LLInt and JIT agree on the frame pointer (`r11`), bytecode/JIT-data register
(`r7`), and metadata register (`r10`). Native calls use AAPCS integer register
pairs for double arguments and results, with eight-byte alignment; generated
JavaScript frames maintain JSC's sixteen-byte stack alignment.

The executable allocator uses a 16 MB local code chunk, keeping replacement
branches within the A32 branch range. Code is published and repatched through
`User::IMB_Range`. Symbian code chunks are fully committed and writable;
executable guard-page requests and attempts to turn data reservations into
code are rejected. The existing disconnected data-chunk allocator remains
responsible for ArrayBuffer growth, shrinkage, and guard pages.

## Validation status

The standalone [executable-memory probe](../tests/arm11-exec-probe/README.md)
passed 20 checks in EKA2L1, including actual generated-code execution,
rewriting, cache flushing, near branches across the pool, and data-page
regressions. [Recorded evidence](validation-arm11/README.md) identifies the
tested image and emulator by SHA-256.

Full JSC compilation and execution-tier validation are still in progress.
Configuration flags, syntax checks, and the allocator probe alone do not
establish that Baseline or DFG executes correctly. The tier tests must observe
execution inside the requested tier and compare results across LLInt,
Baseline and DFG. Physical-device validation remains separate from emulator
validation.

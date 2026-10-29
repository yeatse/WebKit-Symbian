# ARM11 Baseline JIT and DFG

The Symbian port restores the traditional ARM backend for
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

Both build scripts also accept `CONFIGURE_ONLY=1`. As with the existing port,
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

The [offline assembler probe](../tests/arm11-offlineasm/README.md) also passed
8,460 assertions on QEMU's ARM1136, using the production Ruby lowering and
ARMv6/VFPv2 machine code. It checks constants, aliases, signed/unsigned and
floating-point comparisons, halfword addressing, register pairs and in-place
trailing-zero counts. An intentionally corrupted constant is detected and
returns a failing process status.

Full JSC compilation, linking and E32 conversion pass. Both EKA2L1/X7 and
QEMU ARM1136 pass 12 cases under each of LLInt, Baseline and DFG, with identical
semantic results. The tests observe actual tier execution, per-function DFG
compilation, entry into DFG during the first hot-loop invocation, and OSR exit
with live integer, double and object values. They also cover inline caches,
argument adaptation, softfp/libm, unsigned conversion, NaN comparisons, GC,
typed arrays and exceptions. [Symbian evidence](validation-arm11/eka-jsc/)
and [Linux ARM evidence](validation-arm11/qemu-jsc/) retain hashes and results.

Default concurrent DFG compilation also passes on both platforms: two
functions execute DFG code produced by background compiler threads, and
optimized allocation code retains live values across GC. No thresholds or
concurrent-compilation settings are changed in this additional smoke test.
The existing JavaScript, Intl and resizable-buffer suites pass with the
default JIT configuration.

Upstream WebKit 2.52 only optimizes dynamic RAB/GSAB lengths in JSValue64.
JSValue32_64 deliberately exits DFG with `UnexpectedResizableArrayBufferView`.
The ARM11 tests verify a normal typed-array read in DFG, then observe the
RAB guard's exit and check shrink/regrowth/GSAB results in the fallback.
This is correct fallback behavior, not in-DFG RAB length optimization.

The complete WebCore/WebKitLegacy browser builds, links and converts to E32.
The same JIT-enabled browser passes local DOM/JavaScript/Chinese rendering,
8 DOM cases, 19 WebCrypto vector/rejection cases, 23 HTTP/WebSocket cases and
6 HTTPS/WSS trust cases on EKA2L1/X7. Each run closes normally with host exit
zero and no guest panic. The release CA is restored after TLS testing.
Browser and JSC SIS packages were actually installed onto E and their
EXE/resources/test files verified byte-for-byte; C/E image hashes agree.
[Browser and installation evidence](validation-arm11/browser/README.md)
includes portable logs, reports and screenshots. The final images explicitly
declare VFPv2 in their E32 headers; only the requirement flag and header CRC
changed, with every executable-payload byte preserved. Both final SIS packages
were reinstalled and local/DOM/default-concurrent-DFG checks passed again.
[Final package hashes and header verification](validation-arm11/vfpv2-header/README.md)
link these rechecks to the original complete regression records.
Physical-device validation remains separate from emulator validation.

# Symbian monotonic clock source and host evidence

Symbian previously fell through `MonotonicTime::now()` to the adjustable Open C
realtime clock and a non-atomic `lastTime` fallback. The [source hunk](source.patch)
adds an explicit Symbian branch to the existing SDK native steady clock ABI.
Root has reviewed and merged this hunk for the upcoming C15 build. **This folder
records source/host/link checks, not C15 browser or phone acceptance.**

The SDK source `posix_time_legacy.cc` already has precise POSIX clock adapters,
but they are optional and absent from the installed archive profile used by
C13. C13 imports `clock_gettime` from firmware libc. Installed archive/ELF symbol
records and source/archive hashes are retained here; no claim is made that
firmware libc's exact tv_nsec behavior was measured.

The selected `SymbianRuntimeSteadyClockNanoseconds` implementation uses native
NTickCount and its period, with ordinary TickCount/period fallback. It holds
process-wide atomic state, extends counter wrap, clamps stale concurrent samples
and explicitly terminates on invalid native clock capability rather than using
wall time. Rebuilding the real SDK `clock.cc` for ARMv6 Thumb1/O3 reproduces its
installed 196-byte function body, relocations and initial state; see
[archive-thumb-code-comparison.json](archive-thumb-code-comparison.json).

The clock sampling contract cannot reconstruct more than half of a 32-bit cycle
without a sample (about 24.85 days for a 1ms native tick). That limit is recorded
rather than hidden by a synthetic wall-clock fallback. Resolution, long
background intervals, suspend semantics and physical-phone behavior need their
own runtime evidence. The patch does not introduce FastCounter direction or
short-wrap assumptions and does not claim lock-free SDK atomic operations.

## Checks recorded

- [host-tests.log](host-tests.log) and [host-summary.json](host-summary.json):
  seven isolated host scenarios pass against the actual SDK clock source with
  fake native API hooks: millisecond fractional progress, 32-bit wrap/stale
  sample, 15.625ms tick fallback, delayed concurrent stale sample, eight threads
  making 80,000 calls, named capability failure, and the documented half-wrap
  sampling boundary. Capability failure has expected exit6; it is not ignored.
- [clock-host-tests.cpp](clock-host-tests.cpp) preserves the exact harness used.
  It includes the ABI header at its original workspace path and links the real
  SDK `cpp/symbian/runtime/clock.cc`; absolute paths are evidence provenance,
  not a claim of portability without path adjustment.
- The actual modified WTF `CurrentTime.cpp` and a consumer of its actual public
  header compile with existing ARM WebKit flags. They link against the installed
  SDK archive with no undefined symbols and garbage collection of unused clock
  paths. [wtf-probe-symbols.txt](wtf-probe-symbols.txt) retains the actual WTF
  monotonic/native clock symbols without a retained `clock_gettime` dependency.
- [arm-commands.json](arm-commands.json) records compile/link commands. Successful
  compiler output logs are retained verbatim, including empty logs.
  [report.json](report.json) and [initial-identities.json](initial-identities.json)
  bind changed source and actual SDK archive/source inputs.

ApproximateTime delegates to MonotonicTime on Symbian and benefits from this
hunk. WallTime, currentTimeInNanoseconds, ContinuousTime and
ContinuousApproximateTime are outside the hunk and outside these checks.

The original C13 memory READY timeout remains a failed run. These host results
do not establish timely READY, native pressure against at least 80MiB of live
JSCell memory, 40s retention or normal browser exit. A separate ordinary browser
performance.now/setTimeout fixture was prepared externally for fresh-token C15
acceptance; its future binary identity and outcome must be recorded separately.

No SDK archive was replaced by this candidate. No emulator log/dump was inspected
and no guest/display or failed case was run during preparation.

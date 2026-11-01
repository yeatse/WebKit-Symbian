# SDK0009 double-complex runtime preparation

The experimental classic WebAudio whole-archive link reached Biquad's real
std::complex<double> operations and found missing __muldc3/__divdc3. SDK 0.3.0's
common runtime CMake source list omitted these complete upstream LLVM builtins;
its installed three archives had neither definition. This preparation fixes
the actual owner/source/build/archive boundary, without scalar-only substitutes,
fast-math or fake import proxies. This SDK candidate does not change WebAudio's default option.

SDK0009 changes one SDK CMake file: both original C sources enter the common
runtime list, with fast-math explicitly disabled and floating contraction off. The SDK's existing upstream
revision is 85ac560262434c9ccfc0c183ec22d4138ed647fb. The reproducible helper fetches
only eleven SHA-pinned small files: two sources, six headers, the two original
unit tests and their unchanged license. No complete LLVM checkout is required.
All original source notices are retained; source bytes and license hashes are
in the manifest and source-identities report.

## Completed checks

- Six actual ARM objects via normal SDK imported Runtime/Streams/NativeAtomics64
  targets compile. Each defines one complete builtin; EABI5/base PCS is checked.
- Actual std::complex multiplication/division canaries link all three complete
  staged archives with no explicit builtin object inputs. __muldc3 is 916 bytes,
  __divdc3 is 2064 bytes, with real existing SDK arithmetic/runtime dependencies.
- All 296 original members across three archives remain byte-identical,
  including the allocator and three previously repaired math members (293
  nonmath members). Only two complete members per profile are added.
- The two original upstream numerical unit tests pass under host GCC -O2,
  ASan/UBSan and no FP contraction. Each tests a 137 by 137 input grid: 18,769
  combinations per operation, covering signed zeros, finite quadrants/ranges,
  infinities and NaN classifications. These are upstream host tests, not full
  mathematical conformance or actual ARM numerical execution.
- Generic helper cold source acquisition, a controlled post-math prefix without
  the pair, stale cached-stage overwrite, explicit trial installation and warm
  exact-pair repeat pass. Warm output is byte-identical. The trial copies only
  2,553,370 archive bytes and symlinks read-only SDK inputs; no full SDK snapshot.
  Actual shared prefix hashes remain unchanged by these preparation checks.
- In that controlled trial, a partial pair and an unknown existing member both
  fail without changing prefix archive bytes. Original trial bytes are restored.
- The actual complete native patch helper passes 18 sparse real-v0.3.0 tests,
  including every old prefix 1..8 upgrade, old six/eight-series idempotence,
  partial/unknown complex owner refusal and preservation of user edits/index.
- Candidate YAML parses; all 28 cache paths are examined. Only the original
  SDK prefix is saved, before any mutation. Current order is allocator then
  math then complex before JSC. Complex work and dependency cache keys include
  the helper/manifest/0009; the complex step runs again on warm work caches.

Root separately reviewed and installed the earlier locally pinned helper at
/workspace/symbian-audio-complex-root-install/result.json and used the resulting
runtime to build experimental candidates. That Root action is distinct from
this generic CI preparation and is not a physical-device/audio success claim.
The generic helper was installed only into the controlled trial by this agent.
No shared source, SDK prefix, emulator asset, guest/display/server/audio was
modified or launched by this preparation. No workflow was dispatched.

## Integration

Apply the existing reviewed SDK0007/0008/EKA0031 prerequisite candidate first;
this candidate is a precise SDK0009 delta after that candidate. Its workflow
keeps the original-prefix cache lifecycle and adds a separate small complex
work cache. The combined patch is also provided for review against the current
committed base. SDK/EKA local unknown WIP is not reset or overwritten.

Existing 0007/0008 preparation records remain historical evidence with their
own eight-patch scope. This directory records the extended nine-patch scope.
Compilation/link/source/cache checks do not establish Symbian loader execution,
true audio output, native DSP interruption behavior or physical ARM11 numerical
compatibility. Actual WebAudio acceptance is separately owned by Root.

After validation, duplicate staged archives and the controlled trial copies/
backups were removed to protect runtime restoration space. All original
source/objects/ELF canaries/logs/member-hash reports remain; identical archive
bytes are pinned in Root's actual SDK prefix and its pre-complex backups.
The cleanup manifests list every removed duplicate and its exact canonical
identity. No actual SDK archive or runtime original was removed.

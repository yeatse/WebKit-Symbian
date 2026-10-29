# Dynarmic invalid TLB entries

This executes actual Dynarmic-generated A32 and Thumb loads/stores in separate
host processes. It does not mock the lookup or copy its machine-code emitter.
An existing configured EKA2L1 build supplies the unchanged libraries. `build.py`
compiles only the two memory-emitter translation units into private archive
copies, with and without patch 0019. It never builds or replaces the emulator.
The old control uses the exact headers from Dynarmic revision
`855e885288fc21a616ed07ee4437f34abe7c3922`.

Apply [0019](../../patches/eka2l1/0019-Reject-invalid-Dynarmic-TLB-hits.patch)
**inside the Dynarmic submodule**:

```sh
git -C "$EKA_SOURCE/src/external/dynarmic" apply \
  /absolute/WebKit/Tools/Symbian/patches/eka2l1/0019-Reject-invalid-Dynarmic-TLB-hits.patch
python3 Tools/Symbian/tests/eka-tlb/build.py \
  --eka-source "$EKA_SOURCE" --eka-build "$EKA_BUILD" \
  --output /absolute/new-empty-output-directory
```

Use the same host compiler environment as the EKA build. Required tools are
Python 3, Git, Ninja (query mode only), `ar`, and the configured C++ compiler.
The output directory must not exist. Commands, source/artifact hashes, compiler
logs and per-process return codes are retained. Core dumps are disabled for the
intentional old crashes.

The 33 cases cover cold entries, flush, per-page invalidation, null host bases,
denied read/write permissions, same-slot collisions, valid page zero, and the
highest A32 page. Byte, halfword, word and Thumb accesses are exercised. A miss
must invoke exactly the expected callback with the original address/value and
leave the backing page unchanged. Valid hits must bypass callbacks and preserve
normal loads/stores. The old control must reproduce 15 SIGSEGVs and four wrong
hits; its 14 valid/miss controls must pass. All 33 corrected cases must pass.

The callback records misses instead of emulating a Symbian exception. In EKA,
the unchanged `dynarmic_core_callback::MemoryRead*`/`MemoryWrite*` path invokes
`parent.exception_handler` when the MMU reports invalid access. Thus this test
establishes safe dispatch to that path, not successful application shutdown.
The original ROM null access remains an error and requires a separate guest
run. ARM64-host emitter changes have syntax validation only; these results are
from an x86-64 host executing A32/Thumb guest instructions.

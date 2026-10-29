# Native EKA media patch DLLs

Build the actual EKA audio/video guest patch DLLs with SDK 0.3.0, its restored
official Symbian^3 media headers, and the converter patches through `0004`.
The EKA checkout must already contain this port's native media source patches.
The existing EKA CMake default only copies its old prebuilt DLLs; rebuilding the
host emulator alone does not publish the changed guest code.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/work
export EKA_SOURCE_DIR=/path/to/patched/EKA2L1
bash Tools/Symbian/eka2l1/native-media-patches/build.sh
```

Override `SYMBIAN_NATIVE_CONVERTER` if its executable is outside the standard
work directory. `NATIVE_MEDIA_WORK` overrides the staged source/build/log root.
The script builds serially and produces `build/mediaclientaudio_general.dll`,
`build/mediaclientvideo_v100.dll`, their ELF/debug inputs, import proxies and
`SHA256SUMS`. It does not run EKA, alter its resources or install a guest file.

The wrapper preserves the patch DEF ordinals and ABSENT slots. Modern official
headers spell priority value parameters `TInt`; only those symbol spellings
and the resulting C++ substitution index change. HLE assembly receives proper
ELF function type/size metadata without changing its instructions or dispatch
IDs. The forced header selects Clang's real ARM EABI `std::__va_list` and
builtins before `e32def.h`, avoiding its incompatible generic pointer fallback.
Native leaving exceptions stay enabled. Neither DLL imports its own media
library to conceal a missing implementation.

SDK 0.3.0's DLL helper does not automatically retain unwind tables when native
leaves are enabled. This project explicitly uses the SDK unwind linker layout
and exception descriptor while keeping the DLL startup. Its published E32 must
contain the actual EHABI tables and descriptor; compilation with `-fexceptions`
alone is insufficient evidence.

After the host EKA build has completed, back up its resource `patch/` DLLs and
matching `.map` files with hashes. Deploy these two DLLs under those same names
in that resource directory; retain their original map routing. For a normal
checkout this is `<EKA build>/bin/patch/`, or the selected runtime resource root.
Do not replace ROM files or rename them to the firmware's primary library names.
Any subsequent EKA build may recopy its old group DLLs, so verify deployed hashes
immediately before a run. Restore the backed-up resource files after an isolated
experiment when appropriate.

The initial host checks on 2026-10-29 resolved all 118 audio exports and all
165 video ordinals (83 present, 82 ABSENT) against the real linked ELF, including
18 and 4 read-only object exports. Converter tests passed 54 cases; each actual
ELF passed unchanged and rejected 20 independently corrupted object metadata
variants. Those are build/format checks. Run `tests/native-mmf` separately for
callbacks, cancellation and control behavior, and capture real pixels/PCM for
presentation evidence before claiming media playback works.

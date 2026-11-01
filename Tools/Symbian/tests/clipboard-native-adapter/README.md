# Production native clipboard adapter probe

This E32 probe compiles the actual browser `ClipboardNative.cpp` through
`production.cpp`. Its BrowserLog stub counts errors without formatting or
recording clipboard contents. It is separate from the earlier fourteen-check
`native-clipboard` API feasibility probe and does not replace that evidence.

Build with `SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER` and an isolated
`PROBE_BUILD`, then run `bash build.sh`. The executable uses UID `0xe0000903`,
requests no capabilities and takes no arguments. The fresh log is
`C:/Data/WebKit/clipboard-adapter-probe.log`. Success requires **31/31**, its
explicit exit-0 marker, normal guest/host exits and no panic/access violation.

The exclusive emulator operator must back up the original native clipboard
file and restore its exact bytes and metadata afterwards, including its
original absence. This probe intentionally replaces the system clipboard with
its own text and malformed stores. It never prints the original contents.

It covers Chinese and emoji UTF-16, CR/LF/paragraph normalization, independent
CPlainText readback, explicit empty-stream presence and a single empty read
callback, oversized/negative/null writes preserving the old text, read-only and
exclusive-locked files, absent stores, oversized dictionary/file/text lengths,
reserved or truncated SCSU and invalid field flags. Repeated successful and
failed reads compare native heap allocation cells/bytes and process/thread
handle counts after warmup. Actual runtime results remain required; compiling
this probe alone does not establish those native semantics.

`python3 run-host.py --build /path/to/isolated-build` runs 24 additional host
boundary checks. The script extracts the actual normalization, write admission
and normalization prefix, serialized-length/store bounds, and drive-validation
code from the production source. Only descriptor storage, HAL results and
Leave mechanics are adapted. It records both source and extracted-code hashes.
This deliberately does not simulate native clipboard parsing or claim native
heap/handle cleanup; those are covered by the independent E32 probe.

# SDK allocator checks

`test-runtime-rebuild.py --work <sdk-runtime-work>` verifies the real rebuilt
archive and rejects changed native bridge members, member-list corruption,
hard-float ABI flags, new imports, unexpected internal definitions, unsupported
SDK versions and unrecognized release libraries. It needs no guest environment.
It also applies the real patch and release header inside a nested Git checkout,
with inherited outer Git settings, and rejects an unexpected post-patch hash
even when Git exits successfully. These bring the suite to 13 tests.
The production rebuild additionally compares all ARM attributes with the
baseline, allowing only the optimization goal to change.

`allocator-probe` is the real ARM pressure probe used for stock/A/B. Configure
it with the installed SDK's `symbian-arm.cmake`, `PROBE_RUNTIME` naming the
chosen archive, `PROBE_MODE=stock|rebuild-a|rebuild-b`, and `PROBE_UID=0xe00008f5`.
Use `SYMBIAN_NATIVE_CONVERTER` to select the validated E32 converter. Build
serially. Run each mode in a fresh guest process through the normal EKA runner,
respecting both browser/JSC locks. It writes only its own
`C:/Data/allocator-<mode>.log`, never a browser database or CA file.

Success requires a fresh complete `ALLOCATOR RESULT` with `failures:0`,
`ALLOCATOR exiting 0`, normal host exit and no guest panic. The pressure-phase
allocation boundary is a measurement, not an assertion that 56 MiB must fit.
The 72 MiB rejection is intentional. Original logs, build flags, hashes and
the distinction from WebKit's aligned pool are preserved in
`docs/validation-memory/sdk-mimalloc`.

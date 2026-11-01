# Narrow source and privacy review

An independent agent read only the explicitly named worker, build, fixture,
validator, preserved-runner, completion-gate and two preservation-helper source
files. It did not read original profile/configuration files, enumerate private
directories, read the original result JSON, or start a guest.

Those sources contain no embedded original user payload or original directory
listing. The worker has only three fixed public names, a restricted run token,
file indices, numeric API results and fixed expected hashes. The fixture writes
only those public files. The validator requires the exact seven-line contract
and exact case-field allowlist. The runner's preservation reports can contain
metadata about original runtime state, so the original result, backup and
journal were explicitly excluded rather than treated as safe merely because
the harness source is public.

The actual linking contract is WTF + ICU i18n/uc/data + mbedcrypto, with the SDK
native ABI, Streams, EUser and ordinary efsrv/hal/libdl imports and the runtime's
normal transitive dependencies. It requires no WebCore, JavaScriptCore, GUI or
event-loop run. Compilation matches the engine's ARM/VFPv2 softfp settings.
The build defaults to compile-only; its historical link script checks the
`libWTF.a` file hash before and after the link. This is a thin archive metadata
check, not a content pin for its external members. The later accuracy review
identified and corrected that documentation limitation; the historical source
snapshot remains byte-for-byte unchanged. No source issue was found that blocks this
ordinary API contract. No files were changed by the independent review.

The parent separately verified the public API logs and retained health fields.
The summaries preserve the old 8/9 FAIL and the new 3/3 PASS separately; they do
not contain any original runtime profile/configuration payload or item counts.

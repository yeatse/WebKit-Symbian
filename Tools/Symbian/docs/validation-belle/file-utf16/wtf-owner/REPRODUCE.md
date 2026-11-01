# Reproduction scope

These are exact source snapshots of the two existing external work roots;
`SHA256SUMS` pins the files in this documentation package. Runtime execution is
owned by Root. Documentation preparation did not rerun either worker.

The build uses the normal installed SDK at `/workspace/symbian-sdk`, its
`SymbianApp` helper, the converter at
`/workspace/symbian-work/symbian-native/cpp/symbian/tools/symbian-native`, the
actual WebKit checkout `/workspace/WebKit-Symbian-graphics`, and generated build
`/workspace/symbian-graphics-work/build/webcore`. ICU is the installed cross
build at `/workspace/symbian-work/icu/install`. The recorded owner build uses
the SDK after the normal math0007 installation; its input identities are in
`build/build-result.json`.

The original source roots are
`/workspace/symbian-file-io-contract-work` and
`/workspace/symbian-wtf-file-owner-contract-work`. The sources under `sources/`
are byte-for-byte copies of those public files, not relocated replacements.
The wrappers retain their original explicit absolute dependency paths and pins.
The public preservation-helper snapshots are included under
`sources/shared-preservation/`; their original locations remain in the wrapper
source. Restoring the original work-root layout reproduces the same path
contract. For a different layout, adjust only the build paths and public helper
locations, then regenerate and review the affected source pins before running.

To compile the owner worker without executing it, set the normal SDK variables:

```sh
export SYMBIAN_SDK_PREFIX=/workspace/symbian-sdk
export SYMBIAN_NATIVE_CONVERTER=/workspace/symbian-work/symbian-native/cpp/symbian/tools/symbian-native
bash /workspace/symbian-wtf-file-owner-contract-work/build.sh --compile-only
```

To link the completed owner archive, additionally use the exact accepted pin:

```sh
export CONTRACT_WTF_SHA256=045b0ebab6a9888a6649f10ab113c7936ee681ed23a83472b6ce3c57f0c59f33
bash /workspace/symbian-wtf-file-owner-contract-work/build.sh --link
```

The runner checks the actual E32 and public helper pins before isolation. A
different library or executable must receive a new reviewed artifact identity;
these accepted identities cannot be silently reused for a changed build.

Only Root may run the CLI in its existing display session, after sourcing
`/workspace/eka-validation/env.sh` and `/workspace/symbian-media-work/env.sh`.
Use a fresh output directory, for example:

```sh
python3 -B /workspace/symbian-wtf-file-owner-contract-work/runtime/run.py --execute --output /workspace/wtf-file-owner-run-02
```

Omitting `--execute` performs read-only preflight and creates no output or guest
directory. The runner deploys only its fresh three-file directory and fixed
worker within a preserved fresh WebKit profile, sets fixture mtimes to
1700000000, and restores all originals. Runtime PASS additionally requires the
complete actual API log, normal matching worker exit 0, natural host exit 0,
unchanged pins, no fatal markers and all restoration gates. Synthetic parser or
completion tests alone cannot establish runtime PASS.

The earlier direct comparison's accepted record is intentionally FAIL 8/9;
its exact worker, build, validator and harness source is retained for ordinary
API reproducibility. Its narrow supplementary-character gap is not hidden by
the later WTF result. Neither source snapshot validates GUI/FormData behavior.

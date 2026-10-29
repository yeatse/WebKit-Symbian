# Linux resource compiler identifier locations

SDK 0.3.0's Linux `rcomp` crashes on valid enum aliases and symbolic `LLINK`
fields when its input has no `#line` directives, as produced by preprocessing
with `-P`. The parser reads an uninitialized source filename while recording
the identifier location. The patch initializes the input filename on Linux
and registers each identifier's current location before reading it. It updates
both the grammar and the checked-in generated parser. Resource IDs, field
layouts and expression semantics are unchanged.

The source is the original EPL resource compiler from
[SymbianRevive/symbian-build at d3c2eadd](https://github.com/SymbianRevive/symbian-build/tree/d3c2eadd3ff7826bdf9e1d92f447c357571af18b/bintools/rcomp).
The two existing Linux compatibility patches and headers come from
`symbian-platform` revision `b105f752f84d1b5640052f64c4a703017810d7b8`.
[manifest.json](manifest.json) pins those files, this patch, and the shipped
Linux x86_64 `rcomp` and `uidcrc` bytes. Original copyright notices remain in
the upstream sources. A local SDK source change adds the same patch to the
SDK export process; no SDK repository commit or release is assumed here.

From the checkout root, after preparing the SDK and `symbian-platform` source:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-rcomp.py --install
```

The script respects `SYMBIAN_WEBKIT_WORK` and `SYMBIAN_SDK_PREFIX`; explicit
`--work-dir`, `--platform-source` and `--sdk` paths are also accepted. Omit
`--install` to build and verify a separate tool. It needs Git, `patch`, Python
3.12 and a host C++14 compiler (`g++` on Ubuntu 24.04). Neither bison/flex nor
an SDK-wide rebuild is needed. The generated parser is the pinned original
with the matching narrow change applied.

Each invocation extracts pinned Git objects into a new directory, applies the
three patches with zero fuzz, compiles, and runs 14 real compiler checks. A
cached checkout never substitutes a previous PASS for that work. The first
partial-clone/archive operation may fetch about 208 MiB of upstream objects;
the separate `sdk-rcomp` cache avoids repeating that download. An interrupted
managed Git initialization can resume; an unexpected cached remote is rejected.
Explicit `--source-repository` checkouts are read by revision, ignoring dirty
working-tree files, without changing their remote configuration.

Only `bin/rcomp` is replaced, atomically, after all checks pass. The original
release tool is retained under `sdk-rcomp/base`. A known previous installation
is accepted only with its matching per-work-directory installation record;
unknown installed bytes or a changed backup are rejected. Exact hashes of the
two shipped tools, rather than a mutable SDK version string, identify the
supported release. `verification.json` records source and patch identity,
compiler, and output hash. Host compiler versions can change the output bytes,
so the executable hash is recorded rather than pinned across distributions.

Tests include enum aliases, forward/backward symbolic links, generated resource
IDs, byte equality with equivalent numeric resources, seekable standard input,
line directives, normal diagnostic failures, and the stock tool's crash as a
negative control. They do not claim support for a nonseekable stdin pipe.
The [recorded host verification](../../../docs/validation-belle-rcomp/README.md)
also includes the real browser resource and a clean build nested in WebKit.
The four temporary-Git wrapper checks can also be run without network access or
an SDK:

```sh
python3 Tools/Symbian/tests/rcomp/test-rebuild-sdk-rcomp.py
```

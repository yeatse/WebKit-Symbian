# ECom plugin converter patch 0006

Apply `0006-Support-explicit-ECom-plugin-identities.patch` after existing
0001–0005 on symbian-platform v0.3.0. The original five patches are unchanged.
The SDK repository and the new owned test use Apache-2.0; this converter patch
contains no firmware bytes or imported EPL implementation code.

The patch retains the three production changes and registers six additional
identity tests in `symbian_e32_test` through `cpp/tests/CMakeLists.txt`.
The existing WebKit `build-symbian-native.sh` already applies all `*.patch`,
builds this target and runs its matching CTest entry when
`SYMBIAN_NATIVE_TESTS=ON` (the default). No build-script edit is necessary.

This portable stage performed source-only sequential replay of patches
0001–0006, reverse-applied checking for 0006 and Bash syntax checking of the
existing build script. Replayed production/test files exactly match the
previously tested overlay. Machine-local `.dev` experiment additions were
removed; removing those documentation hunks changes no production source or
compiled bytes.

The earlier external source build passed 62 native E32 tests (56 existing +
6 added) and 13 CLI checks. Those results identify the original external
build only. No new host build, complete portable rebuild, guest run or loader
acceptance is claimed by this packaging step. See `0006-manifest.json` for
source hashes, prior converter identity, test wiring and exact checks.

# EKA2L1 text input plugin

Build `avkonfep_general.dll` for EKA2L1's native text entry and composition
callbacks. Use a Symbian SDK 0.3.0 installation, the port's native converter,
and an EKA2L1 checkout with the FEP cancel-completion dispatcher and
`CHostBridgedImeFEP::CancelChangedTextL()` implementation.

## Build

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/build-root
bash Tools/Symbian/scripts/build-symbian-native.sh

python3 Tools/Symbian/eka2l1/fep/build.py \
  --eka-source /path/to/EKA2L1 \
  --work "$SYMBIAN_WEBKIT_WORK/fep-native" \
  --converter "$SYMBIAN_WEBKIT_WORK/symbian-native/cpp/symbian/tools/symbian-native"
```

Choose a fresh directory for `--work`. The builder stages the EKA sources and
SDK interface, generates the FEP import proxy, and builds the native plugin.
Use `--prepare-only` to stage the inputs or `--jobs` to set build parallelism.

Outputs include `build/avkonfep_general.dll`, its ARM ELF and linker map,
compile commands, tool logs and `result.json` under the selected work directory.

## Install

Build the host emulator, then place the plugin at
`EKA_BUILD/bin/patch/avkonfep_general.dll`. Its ECom identity and
`ImplementationGroupProxy` export match EKA's general FEP routing.

The EKA guest sources retain their GPL notices. The SDK interface files in
`official/` use EPL-1.0; [sources.json](official/sources.json) records their
upstream revisions and file identities.

See [Development](../../docs/Development.md) and
[Testing](../../docs/TESTING.md) for the build and test workflow.

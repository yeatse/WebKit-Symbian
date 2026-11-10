# Linux resource compiler

The Linux resource compiler builds the browser's RSS resources, including
symbolic identifiers, enum aliases and `LLINK` references in preprocessed
input.

From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-rcomp.py \
  --sdk "$SYMBIAN_SDK_PREFIX" --install
```

The helper uses `SYMBIAN_WEBKIT_WORK/sdk-rcomp` for source and build output.
It accepts `--work-dir`, `--platform-source` and `--source-repository` for
custom layouts. Requirements are Git, `patch`, Python 3.12 and a host C++14
compiler; `HOST_CXX` selects the compiler.

Sources come from
[SymbianRevive/symbian-build](https://github.com/SymbianRevive/symbian-build/tree/d3c2eadd3ff7826bdf9e1d92f447c357571af18b/bintools/rcomp).
The SDK compatibility files are pinned to `symbian-platform` revision
`b105f752f84d1b5640052f64c4a703017810d7b8`. `manifest.json` records these
inputs. Original Eclipse Public License notices are retained.

The build compiles the resource compiler and browser resource fixtures, then
installs `bin/rcomp`. See the [test guide](../../../docs/TESTING.md) for the
resource test workflow.

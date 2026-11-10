# Native SDK patch stack

The port uses `symbian-platform` v0.3.0 with the following patches, applied in
numeric order by the build script.

| Patch | Purpose |
| --- | --- |
| 0001 | Large E32 images and unwind tables |
| 0002 | LLD position-independent range-extension thunks |
| 0003 | ARMv6, VFPv2 and base AAPCS image attributes |
| 0004 | Frozen read-only DLL object exports |
| 0005 | NetworkControl capability support |
| 0006 | ECom plugin identities |
| 0007 | C99 `atan2l` in the guest runtime |
| 0008 | MMF client DLL identities |
| 0009 | LLVM double-complex runtime helpers |
| 0010 | ScreenPlay headers and frozen import proxies |

From the checkout root:

```sh
bash Tools/Symbian/scripts/build-symbian-native.sh
```

The script applies the complete stack, builds `symbian-native`, and runs its
E32 host tests. `SYMBIAN_PLATFORM_SRC` and `SYMBIAN_NATIVE_BUILD` select the
source and build directories. `SYMBIAN_NATIVE_TESTS=ON` enables the tests.

The target uses ARMv6/Thumb-1 instructions, VFPv2 and the softfp calling
convention. JIT executables declare their VFPv2 requirement through
`Tools/Symbian/scripts/e32-header.py`. The browser requests NetworkServices (`0x2000`);
network test controllers also request NetworkControl (`0x2400`).

Converter changes follow the SDK's Apache-2.0 license. Exported Symbian headers
retain their original Eclipse Public License notices. Source revisions and
import definitions are recorded in the accompanying manifests.

See [SDK setup](../sdk-0.3.0/README.md) and the
[development guide](../../docs/Development.md).

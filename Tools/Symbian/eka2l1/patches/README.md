# EKA2L1 integration

The browser and its test suites use the EKA2L1 integration on the
[`webkit-symbian`](https://github.com/yeatse/EKA2L1/tree/webkit-symbian) branch
of `yeatse/EKA2L1`. It adds GLES2, native MMF media, connection observers,
ScreenPlay surfaces, IAP service settings, mapped ROM files, file, network,
keyboard and FEP fixes.

| Patch | Purpose |
| --- | --- |
| `dynarmic-tlb.patch` | Reject invalid Dynarmic TLB entries before direct guest memory access. |

The Dynarmic patch targets the `src/external/dynarmic` submodule at revision
`855e885288fc21a616ed07ee4437f34abe7c3922`. Apply it inside that submodule:

```sh
git clone --recurse-submodules -b webkit-symbian https://github.com/yeatse/EKA2L1
git -C EKA2L1/src/external/dynarmic apply "$PWD/Tools/Symbian/eka2l1/patches/dynarmic-tlb.patch"
```

Then build the emulator with its CMake instructions. See
[Development](../../docs/Development.md) for the browser build and
[Testing](../../docs/TESTING.md) for emulator setup and test commands.

Install libcurl 7.85 or later before configuring the emulator for native HTTP
and HTTPS media playback. Build and install the
[native MMF DLLs](../native-media-patches/README.md) with the Symbian SDK.

# EKA2L1 native media DLLs

Build the native MMF audio and video DLLs used by EKA2L1's ROM routing. The
project uses the Symbian SDK 0.3.0, its Symbian^3 media headers, the port's native
converter and an EKA2L1 checkout with the native media implementations.

## Build

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK=/path/to/work
export EKA_SOURCE_DIR=/path/to/EKA2L1
bash Tools/Symbian/scripts/build-symbian-native.sh
bash Tools/Symbian/eka2l1/native-media-patches/build.sh
```

`SYMBIAN_NATIVE_CONVERTER` selects the converter executable.
`NATIVE_MEDIA_WORK` selects the staging and output directory; its default is
`$SYMBIAN_WEBKIT_WORK/eka-native-media`.

The build creates `mediaclientaudio_general.dll` and
`mediaclientvideo_v100.dll` in `build/`, with their ELF files, linker maps and
`SHA256SUMS`. It preserves export ordinals and uses the Symbian runtime for startup,
exceptions and heap allocation. `verify.py` checks the DLL layout,
unwind data and native imports during the build.

## Install

Build the host emulator, then copy the two DLLs into its runtime `patch/`
directory alongside these routing maps:

| DLL | Routing map |
| --- | --- |
| `mediaclientaudio_general.dll` | `mediaclientaudio.dll.map` |
| `mediaclientvideo_v100.dll` | `mediaclientvideo.dll.map` |

The standard runtime directory is `EKA_BUILD/bin/patch/`. Restart the emulator
with the updated media assets and run the media playback flow in
[Testing](../../docs/TESTING.md).

See [Development](../../docs/Development.md) for SDK setup.

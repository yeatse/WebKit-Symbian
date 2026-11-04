# Development

The port targets ARM11 devices running Symbian^3, Anna or Belle. It uses C++23, ARMv6, VFPv2 and the softfp ABI.

## Requirements

Use a Linux x86-64 host and [symbian-platform SDK 0.3.0](https://github.com/hpnkv/symbian-platform/releases/tag/v0.3.0), which includes Clang 23.1.2. Install CMake, Ninja, host Clang/LLD, g++, binutils, Git, curl, Ruby, Perl with JSON::PP, gperf, flex, bison, unifdef, pkg-config, ccache, OpenSSL, autotools, patch, and Python 3.12 or newer with `cryptography`.

Run the following commands from the repository root. Extract the SDK into the selected SDK directory.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
export SYMBIAN_WEBKIT_WORK="$PWD/WebKitBuild/Symbian"
export JOBS=4
```

## Prepare the SDK

These steps install the native interfaces, ARM runtime and math libraries, resource compiler and TLS support used by the port.

```sh
python3 Tools/Symbian/scripts/apply-sdk-patches.py
Tools/Symbian/scripts/build-symbian-native.sh
python3 Tools/Symbian/scripts/rebuild-sdk-runtime.py \
  --platform-source "$SYMBIAN_WEBKIT_WORK/symbian-platform" --install
for component in math complex; do
  python3 "Tools/Symbian/scripts/rebuild-sdk-$component.py" \
    --sdk "$SYMBIAN_SDK_PREFIX" \
    --source "$SYMBIAN_WEBKIT_WORK/symbian-platform" \
    --output "$SYMBIAN_WEBKIT_WORK/sdk-$component" --install
done
python3 Tools/Symbian/scripts/rebuild-sdk-rcomp.py --install
python3 Tools/Symbian/scripts/rebuild-sdk-mbedtls-ip.py --install
```

Source and license information is recorded with the [SDK patches](../patches/sdk-0.3.0/README.md) and [native build tools](../patches/symbian-native/README.md).

## Build and package

```sh
Tools/Symbian/scripts/build-icu.sh
Tools/Symbian/scripts/build-jsc.sh
Tools/Symbian/scripts/link-e32.sh
Tools/Symbian/scripts/build-deps.sh
Tools/Symbian/scripts/build-webcore.sh
Tools/Symbian/scripts/build-browser.sh
SIS_VERSION=0.3.0 Tools/Symbian/scripts/package-sis.sh
```

`build-webcore.sh` builds WebCore and WebKitLegacy. `build-browser.sh` also prepares the ScreenPlay SDK interfaces. Build logs, dependencies and generated files are stored under `SYMBIAN_WEBKIT_WORK`.

| Setting | Default | Purpose |
| --- | --- | --- |
| `SYMBIAN_JSC_MODE` | `dfg` | JavaScript execution mode: `dfg`, `baseline`, `llint` or `cloop`. Use a separate work directory for each mode. |
| `BROWSER_PRESENTATION` | `auto` | Try EGL, then ScreenPlay, then bitmap presentation. `screenplay` starts with ScreenPlay; `bitmap` selects bitmap presentation. |
| `ICU_DATA` | `file` | Load ICU data from `C:/resource/icu`. `static` embeds it in the executable. |
| `ICU_LOCALES` | Selected common locales | Space-separated locale list, or `all`. |
| `OPTIMIZE` | `-Os` | Target compiler optimization flags. |
| `JOBS` | Host CPU count | Parallel build jobs. |

The package is written to `$SYMBIAN_WEBKIT_WORK/sis/symbian-webkit.sis`. The browser, JavaScriptCore shell and application resources use the installer-selected drive. Shared ICU data, example scripts, certificate and font configuration files are installed on C:.

To sign the package, provide a PEM certificate and private key:

```sh
SIS_VERSION=0.3.1 \
SIS_CERTIFICATE=/path/to/certificate.pem \
SIS_KEY=/path/to/key.pem \
Tools/Symbian/scripts/package-sis.sh
```

## Run and test

Use EKA2L1 with GLES2, native MMF media, connection observers and the [integration patches](../eka2l1/patches/README.md). Set up a Symbian^3 or Belle firmware profile and an X display. The browser runner installs the executable, resources, ICU data and test pages into that profile.

```sh
export EKA2L1=/path/to/eka2l1_qt
export XDG_DATA_HOME=/path/to/eka-profile/data
export XDG_CONFIG_HOME=/path/to/eka-profile/config
export DISPLAY=:99
Tools/Symbian/eka2l1/run-browser.sh \
  "$SYMBIAN_WEBKIT_WORK/build/browser/e32/browser.exe"
```

Pass a URL as the second argument to open another page. Runs and screenshots are saved under `$SYMBIAN_WEBKIT_WORK/runs`. See [Testing](TESTING.md) for emulator setup and the main-flow suites, and [Progress](PROGRESS.md) for module status.

## GitHub Actions

The [Symbian workflow](../../../.github/workflows/symbian.yml) builds source pushes to `*-symbian` branches and uploads `symbian-webkit.sis` as the `sis` artifact. Tags named `symbian-v<version>` publish a release. Repository secrets `SIS_CERTIFICATE` and `SIS_KEY` supply the PEM signing certificate and private key.

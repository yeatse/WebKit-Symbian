# Context ownership and native argument regression evidence

Recorded on 2026-10-29. The adjacent JSON files identify the exact production
headers, fixtures, runners and ARM source versions by SHA-256. Logs contain the
unmodified host-test output. These checks are independent of emulator images.

From the repository root, reproduce the host checks with:

```sh
bash Tools/Symbian/tests/gl-context/run.sh
bash Tools/Symbian/tests/native-mmf/run-command-line-test.sh
```

Both use temporary build directories and accept `CXX` as a compiler executable.
The resource test requires C++20 plus host threads; the argument test requires
C++17. They directly include production headers and do not copy their algorithms.

`resource-state.log` verifies failure before painting, failed restoration after
an ANGLE-like context switch, upload-error propagation, matching GL names owned
by different contexts, deferred and exactly-once deletion, another-thread
release, abandoned contexts, address reuse, and final C++ token destruction.
Its make-current and delete callbacks are mocks: it does not exercise EGL,
WebGL, a real GL driver, or the complete layer tree. The frame fallback and
recursive propagation require integration validation in addition to this test.

`native-arguments.log` covers the production MMF probe tokenizer. It proves
quoted argument handling and bounds rejection, not the OS command-line ABI or
media playback. `User::CommandLine` and actual audio/video behavior are checked
separately by the native MMF probe.

`arm-feature-summary.json` records 10 real ARM syntax checks using a private
copy of the generated configuration: five with WebGL disabled while retaining
native composition, and five at the TextureMapper-disabled preprocessor
boundary. The latter is not a supported Symbian CMake switch: the port currently
forces TextureMapper ON. The script does not reconfigure CMake, link an image,
run a guest or claim that arbitrary disabled-feature combinations are supported.
See the reproduction command in `tests/gl-context/README.md` (relative to
`Tools/Symbian`). Full compiler diagnostics were retained in the validation
workspace under `context-feature-checks`; summary source/config hashes make the
record independent of that workspace's absolute paths.

The WebGL-disabled review also confirmed that
`GraphicsContextGLTextureMapperANGLE.h` guards its class with
`ENABLE(WEBGL) && USE(TEXTURE_MAPPER)`, so its include from `GLContextWrapper.cpp`
does not expose an undefined ANGLE base class. The new ownership state is
included only for `OS(SYMBIAN)`; other ports keep their existing GL deletion
paths. Changed backing-store methods return `bool`; the Symbian compositor
checks failures, while existing callers that ignore return values remain
source-compatible. This source review is not a non-Symbian build result.

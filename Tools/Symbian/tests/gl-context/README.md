This host regression includes the production `SymbianGLResourceState.h` used
by `GLContext`, `BitmapTexture`, shader programs and GPU buffers. Run `bash run.sh`.
Only the EGL make-current and GL-delete callbacks are substituted. It verifies
that failed native restoration after an ANGLE paint suppresses texture upload,
that failure before painting suppresses both operations, and that upload failure
is returned. It also checks per-context deletion of coincidentally equal names,
deferred deletion while ANGLE/another native context is current, release from a
non-owning thread, exactly-once deletion, failed teardown, context-address reuse,
and destruction of the remaining C++ ownership token.

The test does not execute EGL, WebGL, the complete layer tree or a real driver.
The backing-store/tile recursion and compositor fallback are checked by compiling
the real ARM translation units; browser integration still requires a guest run.

Disabled-feature ARM boundary checks can use the existing WebCore compilation
database without rebuilding its targets:

```sh
export SYMBIAN_SDK_PREFIX=/path/to/symbian-sdk
python3 Tools/Symbian/tests/gl-context/check-arm-configs.py \
  --compile-commands /path/to/webcore-build/compile_commands.json \
  --output /tmp/context-feature-checks
```

The script shadows only `cmakeconfig.h` in a temporary include directory and
removes object/dependency outputs. It checks WebGL OFF with native composition
retained, and the TextureMapper OFF preprocessor boundary. Symbian CMake
currently forces TextureMapper ON; this second check does not claim a supported
software-only build. Neither check links or runs a guest.

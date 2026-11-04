# ARMv6 complex runtime

The SDK runtime includes LLVM's `__muldc3` and `__divdc3` helpers for
`std::complex<double>`. They are built for the default, streams and
native-atomic64 ARMv6 profiles.

Apply the native patch stack through `0009`, then rebuild the allocator,
math and complex components in that order. From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-complex.py \
  --sdk "$SYMBIAN_SDK_PREFIX" \
  --source "$SYMBIAN_WEBKIT_WORK/symbian-platform" \
  --output "$SYMBIAN_WEBKIT_WORK/sdk-complex" --install
```

The helper builds the runtime archives and links `std::complex` canaries.
`manifest.json` pins LLVM revision
`85ac560262434c9ccfc0c183ec22d4138ed647fb` and the required source files.
LLVM's original license and source notices are retained. Output and
verification results are stored in `--output`.

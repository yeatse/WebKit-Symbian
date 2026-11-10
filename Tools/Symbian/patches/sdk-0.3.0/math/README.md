# ARMv6 math runtime

The SDK math runtime supplies C99 `atan2l` using the ARM long-double ABI and
its double-precision `atan2` implementation. The build updates the default,
streams and native-atomic64 ARMv6 runtime profiles.

Apply the native patch stack through `0007`, then run this helper after the
allocator rebuild. From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-math.py \
  --sdk "$SYMBIAN_SDK_PREFIX" \
  --source "$SYMBIAN_WEBKIT_WORK/symbian-platform" \
  --output "$SYMBIAN_WEBKIT_WORK/sdk-math" --install
```

The helper builds the SDK runtime targets, checks ARM attributes and links
math canaries. `manifest.json` identifies the runtime source and archive
members. Build output and verification results are stored in `--output`.

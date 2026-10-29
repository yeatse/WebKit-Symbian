# Rebuilding the two SDK TLS objects

The production rebuild script completed against SDK 0.3.0, then installed only
`libmbedx509.a` and `libmbedtls.a`. A separate SDK copy under the checkout's
`WebKitBuild/Symbian/mbedtls-ip-clean-test` completed both a fresh build/install
and a repeated build/install with identical output hashes. The nested run used
relocated source, headers, output and SDK paths. Production SDK source and
public headers were not changed.

The two changed objects preserve their public definitions, symbol metadata,
ELF flags and complete ARM attributes. All 25 other members and the complete
crypto library remain byte-identical. Exact archive and per-member hashes are
in `production-installation.json` and `nested-repeat-installation.json`.

Every invocation rebuilt the two ARM objects and the actual patched host TLS
sources, generated fresh temporary certificate fixtures, passed all 35 signed
certificate/ClientHello checks and passed 2,069 parser differential cases.
The eight additional wrapper checks use real pinned input bytes in temporary
SDK copies. Unknown compiler, header, crypto, either TLS archive, backup or
source bytes, and a repaired SDK without its original backup, are all rejected
before compilation or installation; the copied SDK archives remain unchanged.
No fake successful compiler or cached PASS marker is used.

Reproduce after the main rebuild has produced verified `base/` and `lib/`:

```sh
python3 Tools/Symbian/tests/tls-ip-san/test-rebuild-wrapper.py \
  --sdk "$SYMBIAN_SDK_PREFIX" \
  --work-dir "$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip" \
  --output "$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip/wrapper-rejections.json"
```

See [the patch instructions](../../../../patches/sdk-0.3.0/mbedtls-ip/README.md)
for prerequisites, the main command and original-backup requirements. These
results establish reproducible library construction and the named host checks;
guest HTTPS and cookie behavior are recorded separately. The original host
14/35 negative control remains in the parent evidence directory.

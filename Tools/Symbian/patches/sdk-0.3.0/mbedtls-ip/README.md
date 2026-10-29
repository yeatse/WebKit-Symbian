# Strict IP certificate identities and SNI

SDK 0.3.0 ships Mbed TLS 3.4.1. Its certificate-name verifier recognizes DNS SANs
but never matches binary `iPAddress` SANs; it can also mistake numeric DNS/CN
strings for IP identities. Its ClientHello writer sends IP literals as SNI.

This patch recognizes numeric reference identities first and accepts only an
exact binary IP SAN with the correct context tag and four- or sixteen-byte
length. Numeric DNS SANs and CNs cannot substitute for an IP SAN. The existing
DNS-name path is unchanged, and no certificate-validation flag is cleared.
The SNI writer omits IP literals while preserving `ssl->hostname` for identity
verification. Certificate chains, signatures, trust, validity dates and key
usage continue through the original verifier.

The shared internal parser is adapted from the portable implementation in
[Mbed TLS 3.6.7, revision 068ff080](https://github.com/Mbed-TLS/mbedtls/blob/068ff080b369adfac81509f9b57b2afabaf82dc5/library/x509_crt.c).
Its original copyright and license notice are retained. Three boundary
corrections accept a single compressed zero and a full dotted IPv4 tail, and
reject a single trailing colon after eight groups. The later library's complete
name matcher was not imported: it also accepts numeric DNS/CN names. No public
header, ABI or new external library dependency is introduced.

From the checkout root:

```sh
python3 Tools/Symbian/scripts/rebuild-sdk-mbedtls-ip.py --install
```

The script uses `SYMBIAN_SDK_PREFIX` and
`$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip`, with explicit `--sdk` and `--work-dir`
overrides. Omit `--install` for isolated outputs only. It requires the original
Linux SDK 0.3.0, Python 3.12 with `cryptography`, CMake 3.28+, Ninja, `patch`,
host C/C++ compilers and binutils `readelf`. It performs no network access.

Every invocation, including a cache hit, verifies pinned source/header/compiler
hashes and rebuilds the two ARM translation units. It replaces only
`x509_crt.c.obj` in `libmbedx509.a` and `ssl_client.c.obj` in `libmbedtls.a`.
All other 25 members and the complete crypto archive remain byte-identical.
Member order, public symbol metadata and data sizes, ELF flags and ARM ABI
attributes must match. The added imports are explicitly limited to functions
already supplied by the existing SDK libc/crypto libraries. Both complete
output archive hashes are pinned.

The actual patched 3.4.1 sources also build for the host on every invocation.
Before installation, freshly generated signed certificate fixtures exercise
35 certificate and TLS 1.2/1.3 ClientHello checks; 2,069 fixed parser inputs are
compared with host `inet_pton`, including boundary and malformed cases. These
tests do not disable verification or use a verification callback. Their private
test keys remain temporary and are deleted with the test directory.

Only the two verified ARMv6 archives are installed. Original copies remain in
`sdk-mbedtls-ip/base`, and `installation.json` records the change. Unknown
installed libraries, compiler bytes, source contents or changed backups are
rejected. A previously repaired SDK with a completely new work directory needs
the original pinned backups; the script will not invent an unverified baseline.
CI keeps the original SDK cache separate from this build cache and always reruns
the checks before installation. `verification.json`, `certificate-regression.json`,
`parser-regression.json` and build logs are the reviewable outputs.

After a successful rebuild, the maintained input-rejection checks use temporary
SDK copies and leave installed libraries untouched:

```sh
python3 Tools/Symbian/tests/tls-ip-san/test-rebuild-wrapper.py \
  --sdk "$SYMBIAN_SDK_PREFIX" \
  --work-dir "$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip" \
  --output "$SYMBIAN_WEBKIT_WORK/sdk-mbedtls-ip/wrapper-rejections.json"
```

All eight checks require a specific rejection before compilation/installation
and confirm that the copied SDK libraries have not changed.

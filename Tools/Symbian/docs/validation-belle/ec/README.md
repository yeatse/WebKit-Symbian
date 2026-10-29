# EC helper and Symbian ARM syntax evidence

The production EC helpers pass **244/244 checks on the native host** and
**244/244 on ARM Linux under QEMU ARM1136**. Six WebCore translation units pass
syntax checking with the actual Symbian SDK ARM compiler. These records cover
ECDSA and ECDH on P-256, P-384 and P-521; they do not establish integrated
browser or Symbian E32 runtime acceptance.

| Evidence | Result | Raw records |
| --- | --- | --- |
| Native host production helper harness | 244/244, exit 0 | [Log](host/test.log), [original result](host/result.json), [commands](host/commands.json) |
| ARM Linux/QEMU `-cpu arm1136` helper harness | 244/244, exit 0 | [Log](arm1136/test.log), [original result](arm1136/result.json), [commands](arm1136/commands.json) |
| Symbian ARM syntax checks | 6/6, exit 0 | [Original results](syntax/result.json), individual `syntax/*.log` and `syntax/*.command.json` |

The helper suite uses twelve fixed upstream ECDSA signatures covering three
curves and SHA-1/256/384/512, plus three known ECDH secrets. The cases also cover
fixed-width `r || s`, P-521's leading zero in its 66-byte ECDH result, exact
SPKI/PKCS8 export, raw/JWK components, optional SEC1 and PKCS8 fields, ordinary
invalid parameters, entropy failure and concurrent operations on immutable keys
using four 128 KiB pthread stacks. [Vector provenance](vectors/vector-sources.json)
records the fixed `4fb33923` Git blobs and SHA256 hashes. Generated vectors are
retained without regenerating them during evidence packaging.

The six syntax checks cover `CryptoKeyECMbedTLS.cpp`,
`CryptoAlgorithmECMbedTLS.cpp`, shared `CryptoKeyEC.cpp`,
`CryptoAlgorithmECDH.cpp`, `CryptoAlgorithmECDSA.cpp`, and the algorithm registry.
They used a VFS overlay for the new EC key container and the prepared registry
source before integration. The exact overlay and its two source snapshots are
retained under `syntax/`; their hashes match the now-integrated source files.
The original command arrays retain their original absolute workspace paths.
[Environment selectors](syntax/environment.json) record the task-scoped SDK
variables; reproducing these archival commands requires equivalent generated
WebCore/JSC/WTF headers and updating paths. The raw logs retain the existing
SDK `ASSERT` and pthread initializer warnings without filtering or ANSI removal.

[manifest.json](manifest.json) links each run to its source, static library,
command, log and vector hashes. Original `result.json` files remain unchanged.
The copied raw logs and command records are byte-for-byte identical to the
originals. `SHA256SUMS` covers every artifact in this directory except itself,
including the manifest. Additional hashes taken while packaging are identified
as such; neither file modification times nor the packaging timestamp are
presented as original execution timestamps. The manifest does not claim an
exhaustive snapshot of every generated or SDK transitive header. Libraries and
executables are identified by path, size and SHA256 but are not copied into the
repository. The ARM Linux library is not the Symbian E32 library.

The [30-case page/DedicatedWorker fixture](../../../tests/ec/README.md)
subsequently **passed integrated browser acceptance** on
[candidate 11](browser-c11/README.md). It exercises usage and exception
contracts, key formats, generation metadata, extractability, `deriveKey` and
`deriveBits` boundaries. The shared WebKit ECDH layer retains its existing
`ceil(bits / 8)` truncation without masking the last byte: null returns the full
secret, zero returns empty, and P-521's 66-byte representation rejects lengths
above 528 bits. The helper backend returns the complete padded coordinate.

CryptoKey structured cloning, worker key transfer and persistent CryptoKey
storage remain unsupported because the Symbian serialized-key wrapping backend
has no secure system master-key store. The worker fixture imports encoded keys
independently and does not transfer CryptoKey objects. The original host/ARM
evidence packaging performed no guest launch or whole-engine build; the later
browser evidence has its own identities, logs and result in `browser-c11/`.

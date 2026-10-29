# RSA optional PKCS8 attributes

The updated production helpers pass **171/171 checks on the native host and
171/171 on QEMU ARM1136 with 32-bit pointers**. The original 153 checks are
unchanged; 18 additional checks cover optional PKCS8 attributes, including
empty sets and friendlyName metadata, canonical lengths and high tags, OID
encoding, DER SET ordering, truncation, duplicate fields and bounded nesting.
The browser's existing DER case also checks attribute import and export, so
its expected total remains 33 cases.

Attributes are parsed as metadata and are not preserved in exported keys.
Their ASN.1 structure is checked; the implementation does not interpret the
semantics of arbitrary attribute OIDs. Nesting is limited to 16 and the entire
DER input retains the existing size bound. All RSA component mathematics and
CRT validation still run after parsing.

`result.json` records source and evidence hashes. `attributes-from-153.patch`
preserves the exact helper and harness change from the separately retained
153-check host/native evidence; those earlier results do not claim to cover
this extension. `syntax-overlay.log` records two actual SDK compiler checks,
with checkout, SDK and build paths normalized. The real key-header/registry
integration remains pending the quota candidate's freeze boundary.

The native E32 image was rebuilt separately and subsequently passed 171/171
in the [actual Symbian run](../native-attributes/README.md). These host and
QEMU results alone do not establish that the browser WebCrypto RSA API is
exposed; browser integration and its independent fixture remain pending.

# Symbian UTF-16 file boundary: ordinary API validation

The updated WTF file API passed all three controlled files on the preserved
Belle emulator run. The preceding direct API comparison remains an overall
FAIL: its narrow Open C path could not stat or open the supplementary-character
filename. No old result is reclassified.

| Public fixture | Earlier RFs/RFile | Earlier narrow stat/open/read | Earlier wide wstat/wopen/read | Updated WTF owner API |
| --- | --- | --- | --- | --- |
| `alpha.bin`, 1024 bytes | PASS | PASS | PASS | PASS |
| `zero.txt`, empty | PASS | PASS | PASS | PASS |
| `字😀.dat`, 24 bytes | PASS | FAIL: metadata/open ENOENT (2) | PASS | PASS |

The direct comparison has 8 passes and 1 failure, with normal worker exit 1.
The updated WTF run has 3 passes and no failures, with normal worker exit 0.
Both runner reports record natural host exit 0, no forced cleanup or deadline,
unchanged pinned inputs and complete guest/configuration restoration. Their
ordinary API logs were independently parsed with the original exact validators;
the retained health fields were separately checked. Raw host logs were not
copied or investigated during this review.

The actual WTF worker calls `fileExists`, `fileSize`, `fileModificationTime`,
`openFile` and `FileHandle::read`, using real UTF-16 `WTF::String` paths. All
three files returned their exact size, mtime 1700000000, sequential EOF, original
bytes and SHA-256, including the empty-file SHA. The production Symbian boundary
uses `waccess`, `wstat` and `wopen` with its explicit mode argument; it preserves
every UTF-16 code unit. No-follow `lstat` and unrelated operations remain outside
this change. The source patch and selected-branch compilation evidence are in
`production/`.

The two full production ARM translation units compiled successfully against the
real SDK and actual WTF headers. Their recorded preprocessed-function gates
confirm `OS(SYMBIAN)` selected the wide calls; an unselected branch was not
counted as compilation proof. The actual production path helper also passed
373 ASan/UBSan host string assertions. That finite host check uses an explicitly
named StringView code-unit adapter, not a replacement implementation of the
production conversion helper or target execution. Its small source, adapter
headers and exact production helper copy are included under
`production/helper-contract/`; its result is `production/string-check.log`.

`identities.json` records the actual executable and `libWTF.a` identities. The
WTF archive is SHA-256
`045b0ebab6a9888a6649f10ab113c7936ee681ed23a83472b6ce3c57f0c59f33`;
the 178,376-byte worker is
`8818c12aa7af3db53fc5622089dbc2149f7c7ec907f9c88b886e75111ad2c44a`.
Its normal ARM compilation, ELF link and E32 conversion completed with exit 0.
The direct worker is also identified. Large libraries and executable binaries
are referenced by identity rather than copied into this documentation.

The summaries in `summary/` are deliberately sanitized replacements for the
original `result.json` files. The original JSON was parsed locally, but only
the fixed public-file API records and explicit health/restored booleans were
retained. Original profile/configuration content, restoration metadata,
original identities, directory listings and item counts are omitted. No raw
result, host log, backup or journal is archived. `evidence/` contains only the
strictly validated public worker logs. `fixtures/` contains only the three
already-public, fixed fixture payloads. No original private directory was
enumerated to produce this package.

`sources/` contains the exact ordinary worker/build/validator/preserved-runner
sources and their public helper sources. `SOURCE-REVIEW.md` records the narrow
source review; `REPRODUCE.md` gives the existing build/runtime paths and commands.
The preserved runner uses the existing double locks, fresh whole WebKit profile,
private restoration helpers, natural completion and full restoration gates.
Normal assertion exit 1 remains FAIL; timeout or forced cleanup cannot pass.

This is controlled emulator API evidence, not physical-phone acceptance.
FileReader/FormData and native-picker GUI acceptance are not established by
this CLI result. Existing GUI/FormData FAIL evidence remains FAIL. No GUI,
layout, memory, crash reproduction or other diagnostic work was resumed, and
this documentation preparation started no guest, display or server.

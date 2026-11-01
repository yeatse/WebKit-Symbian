# Symbian downloads: implementation in progress

The controller, native file adapter, bounded-data path, FrameLoader policy and
native UI are connected and build in local C17. Real-browser acceptance has
passed native Save cancellation, exactly-once POST, GET, gzip, chunked transfer,
refusal of an existing destination followed by cancellation, and actual
transfer cancellation after 8192 bytes. See
`validation-belle/downloads/` for the individual image/host identities and raw
results. These are separate runs, rather than a claim that every case passed on
one release image. C19 restores the attached native shell after Save returns;
its actual Options menu displays 8192 bytes and cancels the transfer, with
terminal cleanup and normal close verified. Earlier menu failures remain
retained. Redirect, size,
network-error and storage-error browser cases still need acceptance.

## Reused components and boundaries

`SymbianDownload` uses the existing `ResourceHandle` / curl path. This retains
the port's cookie context, redirect limit, cross-origin credential stripping,
HTTP-to-local-protocol rejection, and TLS peer/hostname checks. It does not
create another libcurl easy handle with separate security settings. A challenge
received by the standalone download controller continues without supplied
credentials and reports an unsuccessful HTTP response. The page's new origin
authentication UI is a separate path and does not establish download prompting.

The old 2.32 `CurlDownload` is unsuitable for direct restoration. Its response
conversion discards the existing handle and sends the request again, potentially
repeating a POST. It also depends on removed CurlRequest file-writing methods.
The new `ResourceLoader::takeHandleForDownload` transfers the paused existing
handle to the download client before navigation's policy-change teardown. If a
live response cannot be transferred, conversion fails explicitly instead of
issuing a replacement request. A normal download request can start directly.
Its independent networking context retains the storage-session provider without
consulting a discarded document during redirects, and always removes an HTTPS
referrer on downgrade.

The first version accepts HTTP(S), complete 2xx responses, and at most
2,147,483,647 decoded file bytes. The limit matches the current native `RFile`
interface; progress arithmetic uses 64 bits. Unsolicited 206, multipart,
`file:`, `data:` and `blob:` are explicitly unsupported by this first controller.
Blob/local downloads need a separately bounded local stream and lifetime tests.
No downloaded file is automatically opened or executed.

## Data and file ownership

The download opts into a 256 KiB curl-to-client byte budget. Ordinary loads
retain their existing behavior. A whole incoming chunk acquires its budget
before SharedBuffer allocation; a full queue returns `CURL_WRITEFUNC_PAUSE`
without consuming that chunk. Curl retains the paused input for later replay.
The acknowledgement travels with the buffer through ResourceHandle's deferred
queue. It returns the budget only after the sink consumes or discards the
buffer, including nested callbacks, redirect-generation rejection and
cancellation. The queue cap is separate from libcurl's own paused input and
transport/TLS buffers; it is not a claim that all network memory totals 256 KiB.

The native `DownloadFileNative` adapter uses ordinary C++ ownership and native
RFs/RFile handles; it does not mix CBase allocation with portable deletion.
After destination selection it:

1. Validates a bounded absolute UTF-16 path and refuses any existing target.
2. Creates an exclusive temporary file in that target's existing directory.
3. Checks every native write, counts only successfully written bytes, and
   preserves disk-full, removed-media and permission errors.
4. Checks `RFile::Flush`, closes the writer, and publishes with `RFs::Rename`.
   If a target has appeared meanwhile, publication fails without replacing it.
5. On cancellation/error, deletes only a temporary file this instance
   successfully created. A cleanup failure remains visible and can be retried.

A failed `Temp` call does not confer ownership of its output filename. Neither
network cancellation nor startup sweeps unrelated files. A process crash may
leave an unpublished temporary file; no crash-recovery or power-loss durability
claim is made. Existing-target replacement is deliberately excluded: EKA's
current `RFs::Replace` deletes the old target before attempting rename.

Suggested filenames are hints, never paths. Content-Disposition's parsed
filename precedes the link suggestion and final URL basename. The bounded
sanitizer removes path prefixes, replaces forbidden/control/bidirectional
characters, retains valid UTF-16 pairs and avoids trailing dot/space ambiguity.
An invalid or empty basename gets `download`. The native file server separately
checks the actual user-selected destination and access permissions.

Content-Length is checked against written bytes only when an explicit, valid
length is present and neither Content-Encoding nor Transfer-Encoding is present.
Unknown and compressed decoded lengths remain unknown; explicit zero stays
strict. Curl still checks the encoded transfer and chunk framing. Progress is
coalesced with a one-shot timer so a stalled stream reports its last pending
byte count within 100ms. Success follows final native publication.

## Embedding integration

The SDK already exports
`AknCommonDialogsDynMem::RunSaveDlgLD(memories, path, 0, 0, observer)` in
`CommonDialogs` (the folder-browsing overload is ordinal 123). The browser
already links its proxy. The prepared native UI uses phone/internal-mass-storage/
removable-card choices and rejects existing targets through the selection
observer, then rechecks the returned full path including its protected extension.
A collision reports the refusal and opens another native Save selection, up to
eight blocking user selections; Cancel remains cancellation and other native
errors remain failures. It offers no overwrite contract.

The independent reference-counted manager retains each controller through its terminal callback,
routes the native selection to `chooseDownloadDestination`, provides the native
file factory, and displays progress plus a cancellation action. It enters
its closing state before calling `detachClient()` on every download, before
destroying the WebView or stopping its event loop. Destination selection is
posted after the original response-policy callback returns. The manager holds itself across native
callbacks and rechecks client/active-transfer state after the dialog returns.
Each view permits one active transfer, with distinct IDs for a rejected second
request. The prepared Options menu exposes received bytes and cancellation;
known lengths also use the existing URL-bar progress line. A negative native
save-dialog error remains a destination failure, distinct from user cancellation.

Response policy selects Download for attachments and unsupported
successful response MIME types. Navigation policy honors WebCore's
already-checked download attribute. WebCore's sandbox-download checks remain
in PolicyChecker/DocumentLoader. These policy/UI changes are applied; native
dialog behavior and lifecycle still require the integrated acceptance below.

## Checks and remaining acceptance

The independent controller, native adapter, and integrated core methods passed
the actual ARM/C++23 syntax commands. The host suites cover filename/path
boundaries, the real native adapter with injected errors, the production byte
budget with producer/consumer threads, and actual ResourceHandle queue/cancel
method bodies with lightweight type substitutes. See the
[test instructions](../tests/downloads/README.md) for their exact scope.

Guest acceptance must still cover actual selected-path bytes/hashes, Unicode
names, save-dialog cancellation, transfer cancellation, navigation/closing
during the dialog or progress callback, an attachment POST observed exactly
once by the server, redirects with credential isolation, good/wrong-host/
untrusted HTTPS, chunked and encoded bodies, truncated connections, a slow
destination with the queue staying bounded, write/flush failures, existing
target and target-created-during-transfer cases, and no successful target or
unowned-file deletion after any failed transfer. Final logs must identify the
actual browser, SDK and emulator images used.

# Symbian ScreenPlay software presentation

Status recorded on 2026-11-04: **the recorded native API and forced-ScreenPlay
browser display checks pass on the pinned emulator**. Host0038 passes two surface
sizes/four submissions, Available, Finish and cleanup. C33 on measurement host0039
then passes the public page and 14 owned screen-color regions: 7200 sampled
pixels, zero mismatches, one ScreenPlay submission tag and no bitmap fallback.
Both accepted runs pass their strict audit, normal host shutdown and restoration
gates. The browser run also completes task-private cleanup without force.

The browser page reports 9 passing checks and one intentionally skipped WebGL
check. Trusted input, default auto/EGL transitions, MMF handoff, full browser
window resizing, physical Belle hardware, vsync, zero-copy and performance are
not established by this run. The known stock-renderer Finish backpressure
limitation remains documented below. Earlier failed attempts remain unchanged.
Evidence is in [validation-graphics/screenplay](validation-graphics/screenplay/README.md).

## Selection and compatibility

`Tools/Symbian/browser/CMakeLists.txt` exposes the build setting
`BROWSER_PRESENTATION`, with these values:

| Value | Browser selection |
| --- | --- |
| `auto` (default) | Try the existing native EGL compositor, then ScreenPlay software presentation, then the existing bitmap path |
| `screenplay` | Skip EGL and try ScreenPlay; service, format, mapping or frame failures still fall back to bitmap |
| `bitmap` | Build the existing bitmap path without the ScreenPlay presenter or EGL presentation attempt |

C33 is the full local build with `-DBROWSER_PRESENTATION=screenplay` and private
1.1.3 client used for the accepted browser run below. C31 preserves the firmware
client; C32 keeps the first private-client build. All include local IAP,
CookieStore and multi-file upload WIP. They are not CI packages or releases.
C33's UI resource is 383 bytes; it must not be confused with a later pure CI
package or its resource sizes. The accepted run verifies the forced ScreenPlay
route only; it does not verify default `auto` behavior.

The current ScreenPlay build imports `surfacemanager.dll` through the SDK proxy.
It targets the Symbian^3/Belle interface contract. A missing imported DLL can
prevent loading before runtime fallback exists. `auto` cannot make this binary
work on a system lacking that DLL; build a separate `bitmap` binary where the
older path is required. The browser no longer imports
`surfaceupdateclient.dll`; its Surface Update session is browser-local.

SDK native patch
`0010-Export-pinned-ScreenPlay-headers-and-imports.patch` restores pinned original
headers, frozen imports and the normal SDK export/target integration. It keeps
the original `@publishedPartner` and `@prototype` annotations; it does not turn
them into a stable publishedAll interface, supply a firmware implementation or
add a capability. `rebuild-sdk-screenplay.py` checks the pinned SDK revisions and
refuses differing existing files. The normal browser capabilities are unchanged.

## Browser-local Surface Update client

`ScreenPlayUpdateSession.{h,cpp}` and `ScreenPlayUpdateProtocol.h` retain the
complete official production 1.1.3 client under the distinct local name
`RScreenPlayUpdateSession`. EPL notices/license, the pinned upstream revision,
original hashes and mechanical transformations are recorded in
`ScreenPlayUpdateSession.manifest.json`. It requests exactly 1.1.3 and keeps the
official opcodes, descriptor bytes, argument slots, validation, notification
caching, cancellation and close preconditions. A local lifetime fix keeps the
nonempty dirty-region descriptor alive through synchronous IPC without changing
protocol bytes. It does not reinterpret an unsupported newer protocol as success.

A narrow host0036 native comparison recorded firmware-client `Connect()` at
`-5`, while explicit 1.1.3 `CreateSession` succeeded and closed normally. Fixed
service tags identify the firmware minor-version rejection and accepted
explicit version. This is record-only evidence: its whole run failed outer
audit. It does not verify frame submission, Available, display or all Belle
firmware compatibility.

Private-client ARMv6 compile/link/E32 and SDK/public/private header coexistence
checks pass. All nine frozen ABI entries are defined locally; no original
update-session import remains. Its host tests compile the production client to
check IPC construction and notification lifetimes. The presenter host adapter
mocks this API and verifies presenter buffer ownership; those scopes differ.

## Buffer and ownership contract

`ScreenPlaySurface.cpp` owns a GUI-thread Surface Manager session, Surface Update
session and mapped chunk. The Surface Update session uses the private client. It requests two cached, CPU-mappable,
non-physically-contiguous `ARGB_8888_PRE` buffers. Allocation and painting use the
actual `SurfaceInfo`, `GetBufferOffset` and chunk bounds, including returned
stride/padding, nonzero offsets and non-overlap checks.

WebCore paints through Cairo directly into the selected mapped buffer. Each
buffer accumulates its own damage, so alternating buffers repaint all changes
since their previous use. Each dirty region starts white and the resulting
premultiplied pixels are flattened over white before submission, leaving every
submitted pixel opaque. This is an opaque software output surface. It does not
implement a transparent background surface.

Before `SubmitUpdate(KAllScreens)`, the presenter synchronizes the cache with
`ESyncBeforeNonCPURead` and registers `NotifyWhenAvailable` storage. A successful
Available completion permits that buffer to be reused. When both buffers remain
in flight, the browser retains invalidation and awaits a callback without
blocking the UI for the current front buffer. Displayed notifications are not
used as reuse signals. Cancelling a notification does not retract a submitted
update or make its buffer immediately safe to paint.

After attachment, the retained Window Server drawing is cleared using that
window's actual `KeyColor()`, without assuming a particular color value or
composition mode. Detachment first checks the current background surface ID;
it does not remove an attachment now owned by EGL or MMF. Teardown cancels and
drains notifications, balances explicit registration, and closes the surface
and mapping in their required order.

Direct Cairo painting describes the application's destination buffer. It does
not prove zero-copy through Window Server, physical scanout, vblank, cache
coherency on a phone or a performance improvement.

## Opaque window and MMF transition

The browser starts with an opaque window. ScreenPlay requires that the window
has never been made transparent: the official `GetBackgroundSurface` and
configured-background-surface preconditions cannot be recovered by assuming an
alpha window became opaque. Deprecated `SetNonTransparent` is a no-op and is not
used as a repair.

Before the first MMF `AddDisplayWindowL`, `PrepareNativeVideoWindow()`
synchronously disconnects EGL, detaches and deletes ScreenPlay, and permanently
disables ScreenPlay for that browser window. Only then does it enable the
window's alpha channel. The first preparation result is retained. Subsequent
paint for this window uses the bitmap/media path; it does not attempt to restore
ScreenPlay after MMF closes. This transition needs its own native/browser
acceptance and is not verified by the presenter host adapter.

## EKA candidate and limits

The separate EKA v3 source candidate implements the managed surface driver,
Surface Update IPC and Window Server background-surface plumbing. It was
replayed and independently source-reviewed across 19 paths, then compiled and
linked into external host0035. Host0036 adds fixed diagnostic service tags for
the connection comparison. A successful build is not native API or display
acceptance.

Root subsequently applied a one-line S^3/v10 slow-executive-table fix for
`ChannelRequest` (`0x0A`) and compiled/linked host0037. It makes the existing
ordinary logical-channel handler reachable; no manual CPU return-value writes
were added. The follow-up source report is preserved in `eka/ldd-return/`, with
Root's later integration status recorded separately. Host0037 then returned
zero from create/info/map/offsets/register/configuration/set/get/sync, and
verified mapped layout, mapped test pixels and its background ownership. Those
pixel checks inspect mapped memory, not a display. First Submit still returned
`-6`; Finish returned `-18`. All notification, mapping, registration and surface
cleanup completed, without a timeout. Host0038 then fixes optional-region presence by descriptor type rather than an
unspecified IPC argument word, and lets the ordinary scheduler drain pending
redraw before the graphics fence. The same private-client probe now passes two
complete cycles/four submissions, Available, Finish and cleanup. All outer
health/audit/restoration gates pass. This accepts the recorded native API subset
on that emulator. Browser presentation is accepted separately below; arbitrary
Belle firmware compatibility is not established.

The implemented subset accepts CPU-mappable `ARGB_8888_PRE`, up to three
buffers, 4096 per dimension and 32 MiB per surface. Its 64 MiB live registry
budget excludes chunks retained only by mapped handles; normal kernel RAM
accounting still applies. Existing-chunk creation, physically-contiguous
allocation, other formats, optional hints, DMA-write synchronization and
Displayed/DisplayedXTimes are unsupported.

The emulator copies the submitted buffer into owned image/mailbox storage
before reporting Available. That release contract allows the application to
reuse its buffer; it is not zero-copy or proof that the image was displayed.
Host0038's Finish releases the kernel while the normal timing-thread scheduler
drains pending drawing, checking flags under a nonblocking screen lock. Its
polling and ordered graphics-worker fence share a 2000 ms budget, with neither
kernel nor screen lock held while sleeping or waiting for that fence. Timeout,
aborted and unsupported outcomes remain real errors.

The subsequent RAII reacquisition of the kernel lock still follows stock
renderer backpressure: the ordinary scheduler may hold the kernel and screen
locks while its legacy submission waits for graphics-queue space. The whole
API's worst-case duration is therefore not proven to be two seconds, and a bounded host contract confirms that stock full-queue backpressure can
delay kernel-lock reacquisition past that budget. Stalled-GPU/full-queue
robustness is not established by the ordinary native run. The successful ordinary
native run does not establish display visibility, vblank, scanout or a Displayed
event.

Probe v1 enabled window transparency and was superseded before execution.
Probe v2 keeps the firmware client and remains historical comparison evidence.
The new full probe v3 uses private 1.1.3, its own opaque window, actual KeyColor,
finite buffer waits and two surface sizes. Its ARM/link/E32 build and 34
synthetic parser checks pass. Its first Root runtime record rejected positive native status fields; suspect
values remain omitted. The later host0037 record passes parser and outer audit/control gates but remains
UNAVAILABLE at first Submit `-6`. Host0038 subsequently passes the complete probe
and all outer gates. A build/parser PASS or host exit zero alone does not prove
native capability, successful outer gates or visible rendering.

## Recorded evidence and remaining gates

| Scope | Recorded result | Boundary |
| --- | --- | --- |
| SDK native patch stack | 21 checks pass; 14 overlay files prepared | Header/import/export integration, not firmware service support |
| Actual ARM headers and imports | Six top-level headers compile; all 16 Surface Manager and 9 Surface Update frozen export words match | No runtime execution |
| SDK installer | Seven install/idempotence/rejection checks pass; Root installed the reviewed interface and preserved runtime archives | Not a fresh full SDK export or phone test |
| Actual presenter source | 47 host scenarios / 298 assertions pass with UBSan; separate actual ARM compile passes | Owned adapter does not verify real scheduler, driver, cache or display |
| EKA source and follow-ups | Original 19-path v3 plus v10 registration, optional-region and redraw-drain fixes; final scoped patch has 20 paths | Recorded native subset accepted; browser display has separate host0039 evidence; phone and queue-stall robustness remain unaccepted |
| Private 1.1.3 client | 8 host scenarios / 75 assertions with GCC, Clang and ASan/UBSan/use-after-scope; actual ARM/link/E32 | Client IPC/lifecycle and ABI only; no native notification/display proof |
| Local C33 | Full build/link plus accepted forced-ScreenPlay browser run on measurement host0039 | Original WIP and 383-byte UI resource; not CI/release or default-auto/phone acceptance; C31/C32 kept as history |
| host0034 native attempts 01–03 | All whole runs remain FAIL; attempt 03 has a parser-valid v2 record reporting UNAVAILABLE at Surface Manager (`manager_rc = -1`), zero cycles and zero submissions | Outer audit failed; neither native API pass nor display proof |
| host0035 native attempt 01 | Host exit zero and fresh v2 record, outer audit FAIL | No accepted native API/display result |
| host0036 narrow SUS comparison | Firmware client `-5`; direct 1.1.3 connect/close succeeds | Whole run FAIL; no submission/display test |
| host0036 v5 narrow attempt | Audit capture/pairing/guard/restoration pass; two `OTHER_AF_UNIX_PATH_TMP_OTHER` connections | Whole run FAIL; unknown peers not allowed or ignored |
| Full private-client probe v3, first run | Positive native status fields rejected | Historical FAIL preserved; suspect values omitted |
| host0037 private-client v3 | Complete outer audit, normal completion/restoration; control/attachment/cache and mapped test pixels pass; first Submit `-6`, Finish `-18` | Capability UNAVAILABLE; no frame submissions, native API or screen-pixel pass |
| host0038 private-client v3 | Two sizes/four submissions, Available, Finish, cleanup and complete outer audit/control pass | Native API subset accepted on pinned emulator; mapped pixels are not browser/display or phone proof |
| C33 browser, host0039 harness v8 | Page, 14/7200 color samples, ScreenPlay tag, normal close, audit and restoration observations pass | Whole run remains FAIL because task-private cleanup failed; global display/native flags stay false |
| C33 browser, host0039 harness v9 | Page 9 pass/1 WebGL skip; 14/7200 owned screen-color samples match, ScreenPlay 1/fallback 0; normal close, audit, restoration and private cleanup pass | Accepted local forced-ScreenPlay display subset; no input, default EGL/MMF, phone or performance claim |
| Default auto source compile | Actual WebContainer TU with TRY_EGL and SCREENPLAY enabled compiles | Compile only: no link or default-route runtime acceptance |

The accepted browser run uses a fixed-title measurement label added only to an
external Qt rendering-child TU in host0039; it is excluded from the production
20-path EKA patch. Process namespace, traced executable, ancestry/start-time and
XRes ownership are checked separately. The known render node can be non-leaf;
its exact role and fixed native content geometry determine the 14 flat color
regions. The controller does not locate them by searching pixels. It then sends
one owned WM_DELETE and independently observes host exit.

Fixture content v2 registers animationend before starting the CSS animation and
waits for the real terminal motion check. Expected position, color regions and
9000 ms watchdog remain unchanged. The original fixture and failed browser runs
are preserved. Harness v9 only prepares its own six-file copied MMF patch view
for removal after no-host, restoration and input-pin gates pass; it does not
change source patches or ignore cleanup failures.

Remaining acceptance includes default EGL-to-ScreenPlay-to-bitmap transitions,
actual browser window resize/input-panel/foreground lifecycle, trusted input,
failure-pressure cleanup and the one-way MMF transition. Physical Belle
hardware/driver compatibility, memory pressure, performance and power remain
separate. The page's DOM-resize check is not a native window-resize test, and its
WebGL scope was intentionally disabled. Historical EGL/WebGL results in
[GRAPHICS.md](GRAPHICS.md) do not validate this new path.

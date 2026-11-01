# Symbian network state

The production bridge observes `RConnectionMonitor`; it never probes a URL or
uses DNS/HTTP errors as evidence of disconnection. It subscribes before taking a
connection snapshot, discards snapshots invalidated by a connection event, and
retains the previous state on API failures or unknown/transitional stages.
At least one known connected interface means online. A complete empty/closed
snapshot means offline. Requests time out after five seconds; a thirty-second
resnapshot also handles missing platform notifications. WebCore's existing
two-second main-thread timer coalesces changes and suppresses duplicate events.

The observer and timer are native `CBase` subclasses. Their explicit deletes
must use `User::Free`, matching `CBase::operator new`; portable C++ delete is a
different allocator. Destruction cancels the timer, query and notification
subscription. The host regression includes self-destruction from a callback.

## Host checks

```sh
python3 Tools/Symbian/tests/network-state/run-host.py \
  --output /absolute/scratch/network-state-host
```

This compiles the actual production `.cpp` under ASan/UBSan. Only the Symbian
API boundary is replaced. The 24 assertions cover unknown state, connection
handover, duplicate events, stale/error snapshots, timeout, cancellation,
destruction and native heap pairing. GCC's inherited-new/derived-delete warning
is disabled for the mock; ASan checks the actual calloc/free pairing instead.

To additionally compile the two changed EKA service translation units and run
19 checks against the actual connection registry implementation:

```sh
python3 Tools/Symbian/tests/network-state/run-host.py \
  --output /absolute/scratch/network-state-host \
  --eka-root /absolute/EKA2L1 --eka-build /absolute/EKA2L1/build
```

The optional mode needs EKA's configured Ninja build and host dependencies. It
reads `ninja -t commands`, compiles into the scratch directory, and never builds
or replaces the emulator. These checks are not a substitute for guest IPC tests.

## Native IPC probe

```sh
cmake -S Tools/Symbian/tests/network-state/native \
  -B /absolute/scratch/network-state-native -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/symbian-sdk/cmake/symbian-arm.cmake \
  -DSYMBIAN_SDK_PREFIX=/absolute/symbian-sdk \
  -DSYMBIAN_NATIVE_CONVERTER=/absolute/symbian-native \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /absolute/scratch/network-state-native -j1
```

The image is `e32/network_state_probe.exe`, UID3 `0xe00008fb`, NetworkServices
capability. It writes `C:/Data/WebKit/network-state-probe.log`. Run it with the
existing native-probe runner under both browser/JSC locks, with a fresh log and
a 45-second bound. Close other guest network applications first: the test
requires an initially empty active-interface list, attaches a second normal
owner, closes owners, explicitly stops its own interface and reconnects it.
It does not toggle a test-only connectivity switch or contact an HTTP server.

Require all 19 PASS lines, `RESULT: passed=19 failed=0`,
`network-state-probe exiting 0`, normal guest/host exit, and no panic. A host
exit of zero alone is insufficient. It also tests authoritative-delete event
data, invalid enumeration index, independent subscription cancellation, and
no callbacks after destroying the production observer.

SDK 0.3.0 lacks `es_enum.h`. The probe carries the complete unmodified public
header and inline definitions from the pinned official source in
`native/official/`; split-header mode excludes private headers it does not use.
No SDK header, proxy, signature or ordinal is invented or overwritten.

## Emulator scope and remaining validation

Patch `0020` connects ConnMon to EKA's existing shared `RConnection` registry,
with real Start/Stop/Attach/last-owner-close events, bounded pending event
delivery and cancellation. It also wires the official post-reform esock IPC
numbers to the existing connection operations. `provenance.json` records the
official protocol source revisions and ABI references.

EKA currently represents its host-backed virtual interface. This patch does
not add host physical NIC/radio discovery, internet reachability checking, or
cancel already-open host sockets when a guest interface is stopped. Those
transport semantics are separate from the connection state observer.

The first old-host probe exposed a native heap mismatch on observer teardown;
its panic remains a failed attempt. With that bridge bug fixed, the same old
host completes normally with 6 passing and 13 failing checks, guest exit 1,
no panic: the missing connection notifications remain visible. On rebuilt
host `046bc411...`, a strict retry now passes 19/19 with guest/host exit zero.
The first run on that host also passed all guest checks but ended with host
SIGSEGV, so it remains FAIL and unresolved; a diagnostic GDB run did not
reproduce it. The [native evidence](../../docs/validation-belle/network-state/README.md)
keeps all attempts. Browser event validation remains pending.

## Browser event controller

`browser.html` adds a separate 19-check suite. It verifies actual trusted
window events and `navigator.onLine` over two disconnect/reconnect cycles,
then waits longer than WebCore's debounce interval to reject duplicate
notifications. A real extra normal Attach/Close while another owner remains
must preserve online state. Startup observations are retained separately.
There is no synthetic `dispatchEvent` or HTTP-failure inference.

Build `controller/` with the same SDK arguments as the native probe above.
Converter patch `0005` is required: the test-only image has UID3 `0xe00008fc`
and capabilities `NetworkServices | NetworkControl` (`0x2400`). The browser
keeps `0x2000`. The resulting `e32/network_state_controller.exe` opens the
real socket/ConnMon APIs, requires an initially empty active-interface list,
starts `C:\sys\bin\browser.exe`, and performs real authoritative Stop/Start
and normal Attach/Close operations after fresh page acknowledgements.

Copy `browser.html` to `C:\Data\WebKit\network-state-browser.html`. With a
browser built from the notifier integration and host patch `0020`, use the
existing native-probe runner and both browser/JSC locks to launch the
controller with one fresh 8–48 character ASCII token (letters, digits, `_-`).
The controller creates the browser within the same emulator instance; do not
start a second emulator or another guest network application. Use a fresh
`C:\Data\WebKit\network-state-controller.log` and preserve the resulting
browser `C:\Data\WebKit\log.txt`. Allow 90 seconds externally. The controller
leaves the final page visible for five seconds for a fresh screenshot, then
checks the browser's PID/SID and requests official `TApaTask::EndTask`; it
requires the browser's normal exit before returning success. Patch `0022`
fixes the emulator's ALF teardown cancellation; do not replace normal exit
with a WM close or forced process termination.

```sh
python3 Tools/Symbian/tests/network-state/validate-browser.py \
  --browser-log /scratch/browser-log.txt \
  --controller-log /scratch/network-state-controller.log \
  --run FRESH_TOKEN --output /scratch/browser-validation.json
python3 Tools/Symbian/tests/network-state/test-browser-validator.py
```

The validator requires the fresh unique 19-check report, four ordered trusted
events, all native operations/counts, browser normal exit, and the controller
success marker. The outer runner must additionally require normal controller
and host exit, no panic, no forced cleanup, and a fresh nonblank screenshot.
The validator's five host tests include 17 mutated-record failures; they do
not substitute for browser execution. Controller/browser guest PASS is pending.

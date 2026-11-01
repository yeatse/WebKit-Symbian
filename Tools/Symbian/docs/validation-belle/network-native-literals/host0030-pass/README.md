# EKA0030 native literal API acceptance — 11/11 PASS

Root built the reviewed TCP-only prerequisite on 2026-11-01 and ran the unchanged
normal-API worker against that exact new image. This independent review performed
no guest/display/server/runtime operation and changed no shared source or host.

| Input or gate | Actual evidence |
| --- | --- |
| Host image | 65,484,680B, SHA256 `e84037d173141a0183a9c4cfe4df361edbd9e387bccead27dd0fd16f59cfb498` |
| Worker | 81,432B, SHA256 `71198c60381adaaec38f5a412a2b7b88f0a0587b1c5658b294cdb0d0400c7b96`, unchanged |
| Fresh identity | `literal-5dc3b2b5d5d44e8b8ce2a3a5fec54cd5` |
| Fixture | Exact11 cases, 11PASS, 0FAIL, final flushed `exiting 0`; original validator PASS |
| Target/host closure | Existing Kernel Main categoryNone exit0; natural CLI host0; no fatal/panic/access violation marker |
| Failure cleanup | No timeout, WM close or forced cleanup |
| Isolation/restoration | Owned host stopped before restore; guest and private configuration transactions restored; inputs unchanged; both locks released |

The acceptance uses all these gates together. Main is the existing Kernel primary
thread name, not an independently identified process-exit observer; the single CLI
target, complete fresh fixture output and whole-host fatal scan remain required.
Host0 alone cannot establish successful guest completion.

The old image passed10/11 and returned KErrGeneral(-2) on native RHostResolver
GetByName(::1). The new image passes the same native IPv6 case and all ten
previously passing cases. `../baseline-host0029/` retains both original FAIL runs.
Numeric Open C getaddrinfo/getnameinfo and native TInetAddr IPv6 already passed on
the old ROM. No Open C capacity patch or ROM replacement was applied.

## Change and build

Exported `0030-Defer-TCP-address-family-and-resolve-KAfInet-dual-stack.patch`
contains only the two changes against Root's saved preceding source:

* KAfInet resolver hints use AF_UNSPEC; an explicit IPv6 resolver retains AF_INET6.
* TCP defers host descriptor family until a concrete endpoint is known, including
  SO_REUSEADDR bind materialization. UDP retains eager IPv4 creation so its existing
  broadcast and pre-send write-poll behavior is preserved.

The incrementally built host exited0. `build-result.json` records the new image
and ten restored native assets: five identical assets in each of the two real
patch directories. FEP is `fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae`.
Only the host hash changes in `wrapper-host0030.diff`; the worker, original
validator and acceptance logic are unchanged. Reverse apply-check against the
integrated two source files passed. The new image is retained externally as
`/workspace/symbian-belle-work/host-ipv6-20261101/eka2l1_qt-host0030`; Root temporarily
restored host0029 for the separately pinned authentication tests.

This run exercises the resolver hunk and ordinary literal APIs. It does not
exercise the TCP socket hunk. Actual hostname/AAAA resolution, IPv6 TCP, IPv6-only
and IPv4 fallback, HTTPS/WSS, guest mbedTLS ALPN, H2 multiplexing/budgets, UDP socket
IPv6 and physical phone behavior remain unaccepted. Host OpenSSL experiments and
this literal result do not substitute for those guest tests.

## Raw metadata correction and evidence preservation

The unchanged wrapper's raw `prerequisite_integrated` field still says false:
it was a constant preparation description inherited from the old-image baseline.
That field is stale for this host-pinned variant. The raw file remains unchanged;
this explicit correction is based on the saved preceding source, exported exact
two-file patch, build output, new executable hash and actual run input pin.
`metadata-erratum.json` records the correction separately. It changes no functional
condition, actual result or original log.

`public-results.json` includes narrow public outcomes, pins and restoration
booleans. Original profile/configuration contents and inventories are excluded.
The full raw fresh host log and raw wrapper result remain externally at the
paths and hashes recorded there. `worker.log` is copied byte-for-byte. No crash
diagnostics were added or inspected. SHA256SUMS covers this archive.

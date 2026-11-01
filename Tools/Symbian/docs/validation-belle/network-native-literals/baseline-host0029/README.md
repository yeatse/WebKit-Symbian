# Literal API baseline on host0029 — FAIL retained

Root executed both runs on 2026-11-01. The independent review did not run any guest, display or server, and changed no shared source, runtime, profile or configuration.

| Run | Actual result | Overall |
| --- | --- | --- |
| network-native-literals-run-01 | EKA own CLI parser rejected the opaque guest argument; host255; no worker BEGIN or API result | FAIL |
| network-native-literals-run-02 | Fresh exact11 cases: 10PASS, only native-resolver-v6-literal returned KErrGeneral(-2), zero addresses; worker deliberately exited1 | FAIL |

Version2 adds exactly one leading space to the opaque guest command-line argument. `wrapper-v2.diff` is the entire wrapper difference; worker, validator, pins, isolation and acceptance gates are unchanged.

Run02 passed ABI sizes, native TInetAddr IPv4/IPv6 literals, numeric Open C getnameinfo IPv4/IPv6, getaddrinfo IPv4/IPv6 with explicit family and AF_UNSPEC (including exact sockaddr lengths, address bytes, numeric names and service443), and native RHostResolver IPv4. Its AF_UNSPEC IPv6 getaddrinfo succeeded with residual errno22; errno is not an error indicator after successful getaddrinfo. The actual return code and returned address were validated.

The failing native API opens RHostResolver with KAfInet/KProtocolInetUdp then calls GetByName(::1). This selects a resolver provider, not an application UDP socket. At the time of this run, EKA mapped KAfInet to host AF_INET and combined it with AI_NUMERICHOST. The existing host log reports getaddrinfo code-9, translated by that source into guest KErrGeneral(-2). This directly matches the sole failure. The later reviewed AF_UNSPEC resolver change and its new result are archived separately under `../host0030-pass/`.

The wrapper labels any nonzero target thread exit as a failure marker. Run02's host0 does not override the explicit fixture exiting1 and Kernel Main categoryNone exit1. Both complete host logs contain no panic, KERN-EXEC, access violation, SIGSEGV or SIGABRT. This is a functional API failure, not a crash or overall normal-completion PASS.

Both runs used no timeout, WM close or forced cleanup, stopped their owned host before restoration, kept their input hashes and restored guest and private configuration transactions. `public-results.json` keeps only narrow public result fields and restoration booleans, excluding originals' inventories, contents and configuration values. Full original host logs and results remain at recorded paths and SHA256 hashes; only normal API/exit lines are quoted.

The actual ROM numeric IPv6 getaddrinfo/getnameinfo and 32-byte sockaddr_in6 round trips passed. This provides no justification to apply the old reference-only Open C capacity patch; no matching ROM source/producer or failed capacity-copy contract was established. Numeric success does not prove every backend hostname path. This literal baseline makes no hostname/AAAA, IPv6 TCP, HTTPS/WSS/H2, UDP socket or physical-phone acceptance claim.

`fixture-build-manifest.json` is unchanged historical preparation/build evidence with SDK/import/ROM identities. Its original preparation-time runtime status is historical. The worker source and validator are unchanged. Both runtime runs remain FAIL.

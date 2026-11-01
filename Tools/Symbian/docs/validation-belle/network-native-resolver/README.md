# Native DNS and default socket protocol — C25 / host0032

Current controlled browser networking passed with C25 b3ebbb964066eb2c3ca17a686e3a104839a387196a4d875271e84bfa06aa29f3 and host0032 692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd. These are local images containing independent Multi/Cookie WIP; future CI images have separate exact-source acceptance.

- [Real AAAA/dual/fallback HTTPS/WSS](c25-host0032-dns-pass/README.md): actual UDP DNS, IPv6 peers for AAAA-only/dual, IPv4 peer after unavailable AAAA, fixed4096B bodies and normal masked WSS8B/close1000.
- [Real AAAA hostname IPv6 H2](c25-host0032-aaaa-ipv6-h2-pass/README.md): initial hostname directly uses real DNS and one::1 TLS connection,12 unique item streams/704512B, actual peak4/limit8/window256KiB/8 updates.
- [Current IPv4 H2/H1 regression](c25-host0032-ipv4-h2-pass/README.md): original multiplex/flowcontrol and H1 ALPN fallback pass on the current image/host.
- [Native fields](native-records-pass/README.md) and [default socket4](socket-host0032-pass/README.md): legal API contracts, natural CLI0 and restored originals.

Two distinct runtime gaps were measured: Open C hostname address conversion
failed while native resolver records were correct, and EKA rejected the SDK's
undefined Internet protocol used by curl's IPv6 availability probe. The owned
libcurl bridge and exact EKA0032 default selection fix these boundaries without
replacing ROM DLLs/proxies or applying a guessed capacity patch. Native records
are capped at16, enumeration at64, total DNS timer10s; exceeding caps fails whole
lookup. The C adapter owns allocator cleanup/ports;19 ASan/UBSan cases passed.

The build also rejected [C24's unselected native bridge](c24-selector-failed/README.md)
even though both objects were archived. The final CMake property and actual-code
postcondition now protect selection; C25 actual macros/object/final ELF passed.
[Implementation checks](implementation-and-build/README.md) retain their original
checkpoint status. Old [C22 DNS](c22-dns-failed/README.md), [OpenC2/4](openc-four-case-failed/README.md),
[C25/oldhost DNS](c25-host0030-dns-failed/README.md) and [oldsocket1/4](socket-host0030-failed/README.md)
remain FAIL. Host0 never converts failed guest/application checks into PASS.

Private user/net/mount namespaces carried actual DNS packets and TLS listeners;
PID namespace remained shared. Host resolver was never read/changed; no hosts,
CURLOPT_RESOLVE, preload or DNS shim. Unknown hostnames were counts only. Public
reserved names, raw fresh results, paired validators and frozen harnesses are
archived; keys, original profile/config contents, binaries/full host logs are not.

PASS scope is controlled positive networking and ordinary own-PID Qt close with
quiet/zero-panic/unforced host0/full restoration. It does not establish official
EndTask, device IAP/ISP DNS/latency, scoped/link-local/DNS64, actual TCP-DNS exchange,
a timed Happy Eyeballs race, cancellation or allocation exhaustion. Known
CookieStore/online/memory-pressure whole-run failures remain unchanged.

# Numeric-IP curl initialization failure

The original candidate reached `https://localhost:8791` but could not open
`https://127.0.0.1:8791`, including a fresh process started directly at the IP.
The server received no IP request. Candidate 4's explicitly enabled diagnostic
log captured `sa_addr inet_ntop() failed ... Address family not supported`,
followed by `CURLE_FAILED_INIT`. TCP and TLS had not started; this was not a
certificate, entropy, SameSite-policy, or redirect-lifecycle failure.

The real SDK defines `AF_INET` as `0x0800`, 16-bit socket family fields and an
8-bit `sa_family_t`. curl 7.88.1 chose the typedef for an explicit cast in
`Curl_he2ai`, turning the family into zero for numeric addresses. The different
localhost builder does not make that cast. Independent disassembly of the
installed old archive confirms `ldrb` followed by `strh` at this assignment.

`build-deps.sh` now defines `CURL_SA_FAMILY_T=unsigned short` for curl only. The
SDK ABI and TLS validation remain unchanged. `curl-rebuild.json` records the
actual compiler argv, old/new archive hashes and the unchanged other eighteen
dependency archives. Root handles browser relinking; the runtime owner handles
guest verification with a matching IP SAN and a real trusted test CA.

The reproduction is in [tests/curl-address-family](../../../tests/curl-address-family/README.md).
`host.log` runs the entire actual curl address-building source with a host ABI
shim: the old three cases fail with family zero, the corrected three retain
family/address/port/name. `old-abi.log` and `corrected-abi.log` compile assertions
against the actual SDK headers and configured ARM toolchain. Both complete
actual-source ARM assemblies are saved; the corrected assignment loads the
full value before the 16-bit store. The host shim cannot establish guest
networking or TLS behavior.

At the time of this evidence snapshot, corrected guest numeric-IP HTTPS and
the full two-process SameSite acceptance remain pending. Existing diagnostic
code writes only curl connection/error text under the explicit debug flag;
request/response headers and bodies are not added to the diagnostic file.

# Symbian numeric-IP address-family regression

Open C uses `AF_INET == 0x0800` and a 16-bit `sockaddr::sa_family` /
`sockaddr_in::sin_family`, but declares `sa_family_t` as an 8-bit type. curl
7.88.1 detects that typedef and casts through it in `Curl_he2ai`. A numeric IPv4
URL takes `Curl_ip2addr` → `Curl_he2ai`, which previously stored family zero;
`localhost` uses a different builder with a direct, untruncated assignment.
The failure precedes TCP/TLS: `sa_addr inet_ntop() failed ... Address family
not supported`, then `CURLE_FAILED_INIT`.

The dependency recipe sets `CURL_SA_FAMILY_T` to `unsigned short` for **every
curl translation unit**. It does not change the SDK typedef, socket ABI, TLS
verification, address resolution, or WebCore request semantics.

Run the host regression against the exact dependency source:

```sh
./run-host.sh /path/to/curl-7.88.1
```

This compiles the complete, unchanged `lib/curl_addrinfo.c`. A small host-only
header supplies Symbian's address constants and field layout, while the actual
curl builders, allocations, byte copies and port conversion run normally. The
old cast must fail for three numeric addresses; the corrected cast must retain
the family, address, port and hostname. This is an ABI/algorithm regression,
not a host substitute for Symbian socket or TLS validation.

With the SDK toolchain environment already configured, check the real headers
and generate both ARM assemblies using the dependency's actual compiler flags:

```sh
./run-native.py --curl-build /path/to/curl-7.88.1/build-symbian \
    --ninja /path/to/sdk/bin/ninja --output /path/to/evidence
```

The script only reads Ninja's compiler commands (`-t commands`); it does not
build/install the dependency or change its configuration. Static assertions
using real SDK headers must reject the old cast and accept the corrected one.
Both variants compile the actual curl source to assembly for inspection. On
the recorded ARMv6 build the old `ldrb` followed by `strh` truncates the family;
the corrected `ldr` followed by `strh` preserves `0x0800`. Checking preservation
of `AF_INET6` does not enable IPv6 in the current curl configuration.

After rebuilding curl and relinking the browser, separately require a real
numeric-IP HTTPS request with a matching IP SAN and trusted test CA. Preserve
wrong-host/untrusted-certificate rejection, existing localhost HTTP/TLS/WSS
regressions, and the two-process SameSite fixture. The compile/host checks do
not establish those browser outcomes.

# IPv6 and HTTP/2 positive acceptance

The host suite builds curl 7.88.1 and nghttp2 1.68.0 from local release archives.
It enables the production Symbian resolver and HTTP/2 branches in a temporary
host source copy, without selecting the Open C ABI globally. TLS uses host
OpenSSL; this is not a Belle network runtime test.

Requirements: C compiler, CMake, Ninja, OpenSSL development files, zlib,
libpsl, Python and `h2==4.3.0` (its dependencies are hpack and hyperframe).

```sh
bash Tools/Symbian/tests/network-protocols/build-host.sh \
  /tmp/belle-network-host /path/curl-7.88.1.tar.xz /path/nghttp2-1.68.0.tar.xz
python3 Tools/Symbian/tests/network-protocols/run.py \
  /tmp/belle-network-host/client /tmp/belle-network-results
```

Tests run against listeners on `::1` and `127.0.0.1`. They check separate-family
resolution, IPv6 HTTPS, IPv4 connection fallback, verified TLS with HTTP/1.1
ALPN fallback, WSS Upgrade/text echo, and twelve HTTP/2 transfers sharing one
connection with at most eight active streams. A 512 KiB response requires
flow-control window replenishment; the server honors the peer's windows.
The budget executable checks allocation accounting and release at the limit.

The SDK test compiles Open C IPv6 structure assertions and links a real curl
transfer with nghttp2 and SDK mbedTLS. It does not execute the resulting ELF.

```sh
SYMBIAN_SDK_PREFIX=/path/sdk bash Tools/Symbian/tests/network-protocols/build-sdk.sh \
  /tmp/belle-network-sdk /path/network-deps/install /path/deps/install /path/icu/install
```

Results and limitations are recorded in
[`docs/validation-belle/network-protocols`](../../docs/validation-belle/network-protocols/README.md).

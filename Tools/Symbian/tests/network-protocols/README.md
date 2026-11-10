# Network protocols

Exercises IPv6 HTTP and HTTP/2 with the port's curl and nghttp2 sources. Requires
a host compiler and the curl 7.88.1 and nghttp2 1.68.0 source archives.

```sh
bash Tools/Symbian/tests/network-protocols/build-host.sh \
  /tmp/network-host /path/to/curl-7.88.1.tar.xz /path/to/nghttp2-1.68.0.tar.xz
python3 Tools/Symbian/tests/network-protocols/run.py \
  /tmp/network-host/client /tmp/network-results
```

The runner starts local endpoints and records each request result. Passing
requests negotiate the expected protocol and receive the fixture response.

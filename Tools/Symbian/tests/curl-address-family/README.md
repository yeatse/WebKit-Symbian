# Curl address families

Checks numeric IPv4 addresses and hostname resolution through the Symbian curl
address-family selection. Requires the curl 7.88.1 source tree and a host compiler.

```sh
bash Tools/Symbian/tests/curl-address-family/run-host.sh /path/to/curl-7.88.1
```

The runner builds the host fixture and checks the selected address families and
connection setup. It exits successfully when the requests use the expected path.

# TLS certificate names

Checks DNS and IP certificate names and Server Name Indication. Requires OpenSSL,
a host compiler, patched mbedTLS sources and libraries, and Abseil headers.

```sh
python3 Tools/Symbian/tests/tls-ip-san/make-certificates.py /tmp/ip-san-certificates
bash Tools/Symbian/tests/tls-ip-san/build-host.sh \
  /path/to/mbedtls /path/to/host/tls-library /path/to/abseil/include /tmp/ip-san-probes
python3 Tools/Symbian/tests/tls-ip-san/run.py --probe /tmp/ip-san-probes/probe \
  --fixtures /tmp/ip-san-certificates --output /tmp/ip-san-results
```

The runner starts local TLS endpoints and records connection results. Passing
results accept matching certificate identities, reject mismatches and send SNI
for the expected hostname requests.

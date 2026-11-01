# C22 ordinary IPv6 literal HTTPS / WSS runner, v2

The v1 IPv4 h2/H1 harness and first actual PASS are frozen and unchanged. This
separate v2 only points the same preservation/closure runner at the independent
literal6 fixture, default mode literal6; C22/host0030/resources/helpers/native
assets/public CA/leaf pins and full 120-second/quiet/own-close contracts are unchanged.

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 -B /workspace/symbian-network-guest-runner-c22-v2-work/run.py \
  --execute --mode literal6 --output /workspace/network-c22-literal6-run-01
```

The owned fixture serves the bootstrap page over IPv4 TLS h2 at 8824. The page
then uses exact IPv6 literal ::1 at 8826 for strictly verified TLS HTTPS and WSS.
The AF_INET6 listener sets IPV6_V6ONLY, verifies actual IPv6 peers and HTTP1 ALPN,
returns fixed4096 bytes, performs the real dynamic WebSocket Upgrade/accept,
normal browser masked fixed text/echo and close1000 exchange. It does not use
CURLOPT_RESOLVE/hosts mappings or claim actual AAAA DNS/Happy Eyeballs support.

Preflight does not create output or start any server/display/guest. Runtime
server alone uses the existing leaf private-key path; it is never opened/copied
by preparation or the outer wrapper. PASS still requires fresh paired page/server
proof, complete unforced ordinary own-PID Qt close, host0, full-log zero fatal
markers and all WebKit/config/bindings/Qt/CA/resource restoration. No target panic
is exempted. A live host on cleanup failure keeps private originals and FAIL.
No screenshot, crash diagnostics or private original contents are archived.
This is not official app EndTask or real-device validation.

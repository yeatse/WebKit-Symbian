# C22 ordinary IPv4 TLS protocol runner

Prepared externally for root; preparation does not start a guest, server or display,
change runtime assets, or open any existing profile/configuration/private key.

The only executing command is root-owned and self-contained. It launches its own
finite local TLS fixture, waits for its exact fresh token, starts C22 on pinned
host0030, and performs ordinary own-PID Qt close after page/server validation and
a 2-second quiet period. Default mode `ipv4` covers h2 and independent H1 fallback.
It does not establish AAAA DNS, IPv6/TCP/WSS, budget exhaustion or app EndTask.

```sh
. /workspace/eka-validation/env.sh
. /workspace/symbian-media-work/env.sh
python3 -B /workspace/symbian-network-guest-runner-c22-work/run.py \
  --execute --mode ipv4 --output /workspace/network-c22-ipv4-run-01
```

Without `--execute`, only public input pins are read. The output remains absent.
A different current host yields `host0030_deployment_needed:true` in this preflight;
execution requires e84037d173141a0183a9c4cfe4df361edbd9e387bccead27dd0fd16f59cfb498.
C22 browser is d490e376f5008322de5f6a413811673c85f13f70eb98896283463820e869c2d9.

Both runtime locks are held and inherited by the owned host. Whole C:\Data\WebKit,
C22 resources, ICU/fonts, and C:\resource\webkit\cacert.pem are renamed aside and
restored. The configuration helper privately renames/clones/restores config.yml,
the complete bindings directory and complete Qt configuration directory. Original
profile contents and configuration inventories never enter evidence. The TLS
server alone receives the existing private key pathname; this wrapper never opens,
reads, copies or archives the key. Public CA/leaf and all ten FEP/MMF asset pins are
checked. Every guest starts with an empty profile and verbose curl tracing off.

PASS requires exact fresh page and server protocol validation, unforced own-PID
close after functional completion, host0, no target panic/access violation/nonzero
thread exit anywhere in complete fresh logs, unchanged pins and full restoration.
The 120-second deadline and failure cleanup cannot PASS. Failure cleanup only
attempts the owned Qt close; it sends no kill signal to a host. A remaining live
host retains private originals/journals and fails rather than restoring over it.
The fixture is a wrapper-owned local Python process and is stopped after the host.

This ordinary Qt close evidence has the same limit as the current auth normal
cases: it does not prove an official application EndTask/destructor path. The
separate known C21 CookieStore EndTask failure remains unresolved and unchanged.
No screenshots or debug/crash diagnostics are taken by this runner.

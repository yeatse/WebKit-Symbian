# C22 IPv6 literal HTTPS / WSS — actual browser PASS

Root executed fresh net-119166fc383849fbbb4ec14916afc320 on host0030 and C22.
The existing public certificate includes IP SAN ::1; TLS chain/hostname verification
remained enabled. The IPv4 h2 bootstrap served the public test page only.

The AF_INET6/V6ONLY server proves peer ::1 for actual HTTPS and WSS. HTTPS
returned the exact4096 fixed bytes (browser checked every byte and server SHA
matched). WSS negotiated HTTP1 ALPN, used the actual dynamic Sec-WebSocket-Accept
and normal browser masked ping, returned one exact4-byte echo, and exchanged
clean close1000. Server errors remained empty and stopped normally.

Independent paired validator, complete-log zero fatal markers, unforced host0,
own-PID ordinary Qt close after actual functional completion and quiet2s, unchanged
pins and full WebKit/resources/CA/config/bindings/Qt restoration all passed.
No target panic is exempted; host full log remains external with identity above.
No private key, original profile/config contents, credentials or binary is archived.

This establishes controlled IPv6 literal HTTPS/WSS using actual guest SDK mbedTLS.
It does not establish hostname/AAAA DNS, Happy Eyeballs fallback, navigation cancel,
allocation exhaustion, official app EndTask, real-device IAP/TLS or performance.
Known C21 CookieStore EndTask failure and every prior FAIL remain unchanged.
Frozen v1/v2 sources and original raw results were not modified.

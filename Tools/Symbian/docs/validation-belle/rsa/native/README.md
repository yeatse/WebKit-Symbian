# Actual SDK RSA probe

The E32 probe `dc6de763…` passed **153/153** checks on host `71cbce0a…`, with
guest exit 0, host exit 0 and no panic/access violation. The raw guest log and
runner result are preserved. The host excerpt confirms four actual 128 KiB
pthread stacks and their normal zero exits; only the emulator source root was
normalized in that excerpt.

This executable uses the production RSA helpers, installed ARMv6 mbedTLS 3.4.1
archive and WTF's actual hardware-entropy bridge. Coverage includes 24 fixed WPT
vectors, strict DER and CRT checks, exact PSS/OAEP lengths, injected entropy
failure and concurrent use of the same key. `manifest.json` identifies source,
library, image and log hashes. The probe process had no Symbian capabilities.

The exclusive runtime owner used a private runner with only an RSA choice/map
and an exact 153-check acceptance rule. `runner-adaptation.json` identifies both
runner hashes; the small adaptation patch is preserved when the shared base
matches. The shared runner was not modified by this validation.

This proves native RSA operation semantics. **The browser's WebCrypto registry,
key metadata/usage routing and worker API still require their separate 33-case
page run after integration.** CryptoKey cloning/persistence remain outside this
batch. Reproduction is in [`tests/rsa/README.md`](../../../../tests/rsa/README.md).

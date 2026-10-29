# Native clipboard probe

This standalone ARM E32 executable uses the real Symbian `CClipboard` and
`CPlainText` store. It does not test or implement WebCore clipboard integration.
Use only an exclusively owned emulator profile: preserve its existing clipboard
file before write modes and restore its original bytes and metadata afterwards.
Never publish the original file or print its text.

Build with `SYMBIAN_SDK_PREFIX`, `SYMBIAN_NATIVE_CONVERTER`, and an isolated
`PROBE_BUILD` set, then run `bash build.sh`. The UID is `0xe0000901`; the program
requests no capabilities and reads its mode from the real Symbian command line.
The SDK startup's synthetic `argc=1` does not discard the selected mode.

Use `Tools/Symbian/eka2l1/run-native-probe.py --probe native-clipboard` with the
built image and actual image/emulator SHA-256 values. Pass one mode after `--`:

- `inspect`: resolve the native clipboard drive and open read-only, without
  extracting any prior text; two checks.
- `write`, then `read` in separate processes: commit and recover exactly twenty
  UTF-16 code units, including Chinese, an emoji surrogate pair, and two LF
  newlines; three checks per process.
- `empty-write`, then `empty-read` in separate processes: commit and recover an
  empty native plain-text clipboard; three checks per process.

Each mode emits one fresh JSON RESULT, an exact PASS summary, and a successful
exit marker in `C:/Data/WebKit/clipboard-probe.log`. The runner verifies the
effective mode, complete check count, guest/host exits, and absence of panic.
It records the host PID so separate process launches remain auditable.

On the tested X7 emulator, inspection resolved drive D and the native store was
`D:/System/Data/Clpboard.cbd`. It did not exist initially. Four independent
processes wrote/read a 62-byte nonempty store and a 21-byte empty store. Reads
left the store bytes unchanged. The test removed only its newly created store
and empty Data directory, restoring the original absence. All fourteen checks,
including inspection, passed. See
[`validation-belle/native-clipboard`](../../docs/validation-belle/native-clipboard/README.md)
for portable evidence. No physical-device result is claimed.

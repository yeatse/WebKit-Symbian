# ALF policy cancellation

This test compiles the actual EKA `alf.cpp` and `alf.h`, replacing only the IPC,
thread and kernel boundary. It checks cancellation matching, completion order,
unknown requests and teardown with AddressSanitizer/UndefinedBehaviorSanitizer.
It does not run a guest or claim that the application exits normally.

```sh
python3 Tools/Symbian/tests/alf-cancel/run-host.py \
  --eka-root /path/to/EKA2L1 --output /tmp/alf-cancel-tests
```

`--exit-contract-only` isolates the actual failure: outer function 2 with inner
35 must complete the earlier outer function 3 / inner 33 request. It fails on
the original source, whose synchronous reply succeeds while the request stays
pending. The full suite additionally checks plugin and full 64-bit thread
matching, forged identities, duplicate requests, descriptor failures, unload,
session destruction, dead/exited requesters, UID reuse and kernel wipeout.

The official source pin and protocol are recorded in `provenance.json`.
Outer function 4 accepts a plugin message ID; it is not a policy opcode. The
GfxEffects plugin's `CancelMessage` is unimplemented, so this repair rejects it
instead of assigning it invented cancellation semantics. Effects remain
otherwise disabled. Guest normal-exit validation belongs to the same
candidate9 image on the rebuilt emulator; the previous strict FAIL is kept.

# ALF cancellation repair: host validation

**29 host checks and the same-image guest normal-exit check pass.**
The original actual ALF source fails the isolated official cancellation
contract: synchronous cancellation reports success without completing the
previous policy notification. This reproduces the service-side cause of the
[candidate9 exit hang](../ui/exit-candidate9/README.md), whose FAIL evidence is
unchanged. No browser change or forced exit is used in this repair.

Patch [0022](../../../patches/eka2l1/0022-Complete-ALF-policy-cancellation-before-client-teardown.patch)
implements outer IPC 2 / inner opcode 35 using the plugin UID and the actual
subscription requester's 64-bit identity. It removes pending state before
completion, checks the requester remains alive with the same UID, and skips
exited, unmapped or kernel-wipe completions. Unload and close drain their
pending requests. The independent protocol review identified that outer IPC
4 carries a plugin message ID, not opcode 33; it is unsupported rather than
being used as a guessed policy cancellation path.

`host-tests.json` and `host-tests.log` record 29 checks against the actual
production ALF source with ASan/UBSan and fake IPC/kernel boundaries. The
isolated `baseline-contract.json` remains FAIL, and its log shows the expected
uncompleted-status assertion. `translation-unit.json` records a separate
compile with the actual EKA build flags and headers. `chain-0022.json` verifies
18 root patches in order and byte-identical replay of the two changed source
files. Patch 0019 belongs to the separate Dynarmic submodule and is excluded
from this root replay.

Run instructions and pinned official protocol sources are in
[the test directory](../../../tests/alf-cancel/README.md). The evidence contains
no proprietary ROM binaries. A real same-image guest normal-exit check on the
rebuilt host has now passed: [candidate9 with host0022](../ui/exit-candidate9-host0022/README.md)
uses the unchanged browser `0d6e8880...` and helper `e668e662...`, host
`046bc411...`, official EndTask while the native picker is open, and natural
host exit 0 without WM close or forced cleanup. That host includes patches
0020, 0021 and 0022; it is not described as a one-patch build. The earlier two candidate9
FAIL runs remain unchanged. This is the named emulator fixture, not a
physical-device exit test.

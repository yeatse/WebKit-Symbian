# EndTask candidate9 — transition cancellation hang

Both observations remain **FAIL**. Candidate9 moves browser control removal
into `PrepareToExit`, and both runs reach every browser destructor stage and
the final `WebKit Browser exiting` line without the candidate8 font exception.
The process nevertheless stays alive: the official EndTask helper times out
after 10 seconds, and the observation harness eventually kills the host after
180 seconds. A destructor log alone does not establish a normal exit.

Browser SHA256:
`0d6e88803e2c5ea0a3937c527f8a9141f9752666f1c6ca4752416fc444f1a6f7`.
Host SHA256:
`71cbce0aa8a8eff9d167614a34f2870d26387df18b3940cf4818a34524933093`.
The [first strict result](exit/validation-result.json) and
[diagnostic result](exit-state-diagnostic/validation-result.json) retain the
failure. Neither run used WM close. `forced_cleanup: true` and host exit `-9`
record the harness cleanup honestly; they are not application exit evidence.

The second run captured the actual guest thread contexts while the process
was alive. Its main thread waits on its request semaphore, with PC
`0x8045b65c`, LR `0x80466803`, and SP `0x57dbd0`. The 512-byte
`exit-state-diagnostic/guest-main-stack.bin` starts at `0x57db90` and was read
through the guest process address mapping. The other browser thread is in a
5 ms `User::After` wait. The raw GDB record is
`exit-state-diagnostic/live-guest-state2.log.gz`; the earlier unsuccessful
type/attachment attempts are also retained and are not treated as evidence.

The main stack and actual ROM imports localize the wait:

- `gfxtransadapter.dll` loads at `0x85525000`; return sites `+0x1739`,
  `+0x175f`, `+0x637`, and `+0x669` are its policy active-object and adapter
  destructors. At `+0x1734` the code calls the `CActive::Cancel` import.
- `gfxtrans.dll` loads at `0x85522000`; `+0x635` is in transition-effect
  destruction. This object is a negative-priority `CCoeStatic`, destroyed
  after the application UI.
- `eikcore.dll` loads at `0x855e6000`; `+0x1c7f` is in
  `CEikonEnv::DestroyEnvironment`, and `+0x6d7d` is in
  `EikStart::RunApplication`. This is still native environment cleanup,
  before returning to the SDK's C++ finalizers.

The pinned official adapter implementation calls `CancelPolicyUpdates`,
unloads its plugin, and then deletes `CAlfTransitionRequest`. That active
object's `DoCancel` is deliberately empty: the preceding server command must
complete its pending notification. `CancelPolicyUpdates` sends outer IPC
function **2**, with plugin UID in argument 0 and an argument-1 descriptor
containing little-endian `int32(35)` followed by the caller's `uint64` thread
ID. The original subscription uses outer IPC function **3**, inner opcode
**33**. The actual ROM cancellation code also loads `0x23` at adapter offset
`+0x566`, independently confirming inner opcode 35.

This host's ALF service returns success for every outer function-2 call but
does not complete the pending policy request. Its existing cancellation only
handles outer function 4. Consequently `CActive::Cancel` waits indefinitely.
Correcting the service's actual cancellation contract is required; bypassing
the browser's native environment destruction would hide this fault. This
directory does not claim that the subsequent service correction has passed.

Official source references:

- [`gfxtransenginetfx.cpp`](https://github.com/SymbianSource/oss.FCL.sf.mw.uiaccelerator/blob/773fc5e215e04584c3e00a1b1fa05c5a8c50440a/uiacceltk/hitchcock/plugins/alftranseffect/alfgfxtransadapter/src/gfxtransenginetfx.cpp)
- [`AlfTransEffectPlugin.h` opcodes](https://github.com/SymbianSource/oss.FCL.sf.mw.uiaccelerator/blob/773fc5e215e04584c3e00a1b1fa05c5a8c50440a/uiaccelerator_plat/alf_extended_visual_api/inc/alf/AlfTransEffectPlugin.h)
- [`RAlfTfxClient` IPC encoding](https://github.com/SymbianSource/oss.FCL.sf.mw.uiaccelerator/blob/773fc5e215e04584c3e00a1b1fa05c5a8c50440a/uiacceltk/hitchcock/AlfDecoderServerClient/src/alfdecoderserverclient.cpp)
- [`CGfxTransEffect` lifetime](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/lafagnosticuifoundation/graphicseffects/ClientSrc/GfxTransImpl.cpp)

`manifest.json` verifies all archived raw bytes; gzip timestamps are fixed.
The diagnostic GDB scripts preserve their original environment-specific
paths. ROM binaries are not included. The [candidate8 font failure](../exit-candidate8/README.md)
remains separate history.

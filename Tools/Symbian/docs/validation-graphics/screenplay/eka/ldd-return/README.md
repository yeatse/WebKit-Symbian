# EPOC 10 ChannelRequest registration correction

Confirmed source blocker: the S^3/EPOC 10 SVC table lacks slow executive call `0x0A`, `ChannelRequest`. This is a registration omission, not a lost `TInt` return in Surface Manager or the bridge.

`kernel/src/reg.cpp:42–43` selects `svc_register_funcs_v10`; the EPOC 10 selection is also present in `kernel/src/libmanager.cpp`. In `kernel/src/svc.cpp:6047–6048`, that table jumps from TickCount (`0x05`) to MathRandom (`0x0B`). The existing v93 table registers `0x0A` to `logical_channel_request`. For an unregistered SVC, `libmanager.cpp:1355–1361` returns false before calling any return-value bridge. That missing-dispatch behavior is consistent with Root's public API observation of the same handle-like positive result from CreateSurface, SurfaceInfo and CloseSurface. This source review does not independently prove which path the observed executable took.

Pinned public kernel source revision `0c3208650587ac0230aed8a74e9bddb5288023eb` establishes the ordinary API route. `kernel/eka/euser/us_exec.cpp:1118–1121` implements the two-argument `RBusLogicalChannel::DoControl` as `return Exec::ChannelRequest(iHandle,aFunction,a1,a2)`. `kernel/eka/kernel/execs.txt:629–637` declares ChannelRequest as a `TInt` slow executive call; it is the eleventh slow call, number 10 (`0x0A`). The download URLs, original tree blobs and verified byte identities are recorded in `source-pins.json`.

The existing emulator path is already correct once reachable: `logical_channel_request` returns `logical_channel_do_control`, which returns the driver's `do_control` result (`svc.cpp:5896–5925`). The non-void `hle::bridge` obtains that result and calls `write_return_value`. HAL uses the same returned-`TInt` channel interface. Surface Manager correctly returns zero for successful create/info/close and a positive mapped chunk handle for MapSurface. Adding manual CPU-state writes in the Surface Manager controller would conceal the missing registration and duplicate the normal bridge behavior.

The minimal external patch adds exactly one line to `svc_register_funcs_v10`:

```cpp
BRIDGE_REGISTER(0x0A, logical_channel_request),
```

`v10-channel-request.patch` SHA256: `93d20ffc5fc99f945927f4346eb66ad69b539afd587c5c2ca2081caececd110d`. It changes `src/emu/kernel/src/svc.cpp` from SHA256 `9ab34c9d569a5b56b48c9619abda2817d43193ab183a1249aa9207fc1b18b2ca` to `4d87532f139d1959ba554bdc2f4b7a5e5c5c2c952ddd37ece5011de17bbbde5d`. External apply-check, exact replay and reverse-check passed. The v94 table has the same omission, but that separate compatibility change is outside this Belle/S^3 minimal patch.

`verify_channel_contract.py` passed: it derives the expected public dispatch number, reproduces the missing baseline entry, verifies the corrected active table and all unchanged prior entries, and compiles/runs the exact unchanged production handler bodies with ordinary host C++ channel/kernel stand-ins. Four cases preserve zero success, a negative error, a positive mapped handle, and invalid-channel error. `test-result.json` records this narrow evidence. The test does not execute a CPU bridge, actual Surface Manager, guest, renderer or display and does not prove native runtime success. Full production compilation and rerunning the strict native record remain Root-owned.

This reviewer modified only this external review directory. Shared source, SDK, private data and the original frozen 19-path candidate were not edited. The earlier source review did not inspect whether this existing per-version SVC table exposed the new driver's ordinary control path; this follow-up identifies and corrects that coverage gap without rewriting prior frozen evidence.

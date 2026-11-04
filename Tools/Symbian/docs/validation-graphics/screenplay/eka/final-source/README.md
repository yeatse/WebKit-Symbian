# ScreenPlay scoped EKA source composition

Status: **source composition draft; browser display validation pending**.

`screenplay-native-surfaces-final.patch` contains exactly 20 paths: the original
19-path v3 ScreenPlay candidate plus `src/emu/kernel/src/svc.cpp`. This is a
bounded source patch, not a diff of the shared working tree. The exact before and
after identities are listed in `manifest.json`.

The v2 composition retains v1's exact before snapshots and first five patches,
then appends the comment-only correction. It was made by replaying these reviewed patches in a new external
tree, checking each component's hash and its input/output source identities:

1. Original v3 Surface Manager, Surface Update, WServ, GL finite-finish and tests.
2. Fixed Surface Update connect-classification tags. They log fixed categories,
   not version values or client input.
3. Optional dirty-region presence determined by the IPC descriptor type. An
   unspecified fourth argument's raw word is not a region pointer.
4. A finite WServ redraw drain before the ordered graphics completion fence.
   The two-second budget covers pending-redraw polling and the remaining fence
   wait. It is not a strict deadline for the whole API call.
5. The existing `ChannelRequest` handler registered at the public Symbian^3
   slow-executive number `0x0A`, which was absent from EKA's v10 table.
6. A comment-only correction: frontend teardown follows the synchronous Finish
   call returning. This replaces an incorrect OS-thread-join explanation. No
   executable code changes in this sixth component.

All 20 final files match the corresponding shared source files byte for byte.
The combined patch passed external apply checking, exact forward replay,
reverse checking and exact reverse replay. No shared source, SDK, previous
candidate, default host, configuration or guest data was modified by this
composition. The IAP getter and POSIX read-only private-COW changes are outside
this patch. They can remain dependencies of Root's particular validation host;
their absence here must not be read as a claim that this patch reproduces that
whole host executable.

The shared repository currently reports EKA commit
`5bbc0435cf9a3a3d0f08d20259d35f60255c3a8a`. The before snapshots also contain
pre-existing port work. Therefore the commit alone is insufficient to identify
an applicable checkout: require the 20 before identities in the manifest.
Do not apply this patch to an unknown or differing working tree without review.

`base/` contains only the listed before source files; `overlay/` contains only
the listed final source files. `components/` preserves the five component
patches. `stage/` and `replay/` are external composition scratch views. The
portable `verify.py` performs a fresh temporary replay and never patches its
input checkout:

```sh
python3 verify.py
python3 verify.py --source /path/to/matching/EKA2L1
```

The actual-method host contract passed 15 cases with ASan/UBSan. A separate
bounded backpressure counterexample confirmed that the stock legacy scheduler
can block on a full graphics queue while holding the kernel lock. Finish can
then exceed its polling budget when reacquiring that lock. The counterexample
drains the queue after 2250 ms; it does not demonstrate a strict whole-call
2000 ms guarantee or repair the existing legacy-queue stall. See
`provenance/finish-host-contract-result.json`. Its source identity precedes the
comment correction; `comment-only-validation.json` proves the exact comment
replacement and absence of executable changes.

Root separately ran host38's native public probe successfully: two dimensions,
four submissions, Available completion, Finish and cleanup passed with the
outer audit and restoration gates. That is a native API observation, not
browser display, scanout, vsync, performance or Nokia 808 proof. The source
composition does not execute a guest and does not elevate this evidence.

The supported runtime subset remains CPU-mappable ARGB8888_PRE, noncontiguous
buffers and explicit owned surface lifecycle. Displayed notifications, other
pixel formats, physically contiguous buffers, existing-chunk creation, hints
and DMA-write cache operations remain unsupported. A graphics completion fence
does not establish physical visibility or vblank.

No private profile/configuration, inventory, runtime log, raw trace, ROM content
or hash, heap/address diagnostic, register dump or disassembly is included.

The external host39 Qt measurement marker is not part of these 20 production
source paths. No display observation from that measurement host is implied by
this source composition.

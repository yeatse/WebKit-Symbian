# Finish pending-redraw verification

This is an external verification package. It does not change the EKA2L1 checkout, run a guest, access a display, inspect process memory, or read private configurations, logs or traces.

`run.py --source-root /path/to/EKA2L1` extracts the actual `window_server::finish_rendering` method and copies the actual `common/queue.h`. It compiles the method in its production namespace with owned API, kernel-lock, screen-lock, session and driver stand-ins. It does not reproduce the Finish algorithm in the test adapter. AddressSanitizer and UndefinedBehaviorSanitizer are fail-fast. The independently compiled full EKA2L1 source and the native runtime are separate Root-owned evidence.

The 15 method scenarios pass: idle completion; scheduler progress after kernel unlock; screen-lock contention; pending-redraw timeout; preexisting abort; wipeout; abort during polling; finite-fence timeout, unsupported and abort results; stopped-thread and already-completed suppression; session retention across final client-handle closure; no screens; and no driver. They check lock restoration, absence of held kernel/screen locks at the fence, notification completion and session pin cleanup.

One additional bounded counterexample uses both the actual method body and the actual queue implementation. The queue is prefilled; a scheduler analogue holds kernel and screen locks during legacy `queue.push`. A watchdog releases capacity after 2250 ms. Finish exhausts its 2000 ms polling budget and then waits to reacquire the kernel. The test finishes normally and proves that the whole call can exceed the polling budget. No thread or process is killed.

The result is **host contracts passed, with a documented stock backpressure limitation**. The redraw polling and GPU fence share a finite budget; reacquiring the kernel mutex is subject to existing renderer backpressure. This package does not establish a strict two-second whole-call bound, GPU completion by itself, physical display, vsync, tearing, performance or phone support.

`lifetime-review.md` gives independent source evidence for ordinary session, IPC-owner, screen-list and driver lifetime. It also traces the legacy blocking submission chain. A future nonblocking scheduler would need to retain an unaccepted owned command list and retry it without replaying CPU-side cache or texture mutations. Merely replacing `push` with `try_push` and discarding a rejected list would lose work, so no such broad change was made here.

The candidate source comments that server destruction follows the OS-thread join are imprecise. The system can be destroyed on that thread after its emulation loop returns and before join. This remains after the synchronous Finish call returns, so the reviewed lifecycle is safe. Root was advised to clarify those comments without claiming a stronger deadline.

The package retains compile/run evidence and identities in `test-result.json`, `source-identities.json`, `lifetime-review.json`, and `lifetime-source-pins.json`. Its host binaries are local build products and excluded from the text-only checksum manifest intended for archiving.

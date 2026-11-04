# Finish pending-redraw lifetime and lock review

Scope: source review of the candidate `window_server::finish_rendering` in the current EKA2L1 checkout. No guest, display, process-memory, register, disassembly, private configuration, raw host log, or trace inspection occurred. No shared source was changed. Source identities are in `lifetime-source-pins.json`.

The ordinary lifetime and lock order are supported by the source. A strict two-second end-to-end completion guarantee has a limitation when the existing graphics submission queue stalls.

## Lifetime and normal lock order

* `system_impl::loop()` holds its system mutex throughout CPU run/step (`epoc.cpp:750`), including synchronous HLE Finish dispatch. `start_access()` takes the same mutex (`epoc.cpp:369`); reset and device replacement use it. Unlocking the kernel inside Finish therefore does not permit a UI reset to replace screens or wipe out the server while this invocation remains on the stack.
* The screen list is built by `window_server::init_screens()` (`window.cpp:2172`), including writes to `screens` and `next`. Teardown occurs in `window_server::~window_server()` (`window.cpp:1763`). The reviewed window sources contain no runtime removal/relink path. Under the system mutex, the copied `first_screen` chain remains valid during this invocation.
* The request increments the kernel session access count (`window.cpp:2516`). Normal final handle closure cannot destroy the session and dispatch `window_server::disconnect` until the pin is dropped. Session destruction sends its HLE disconnect (`session.cpp:214`); window-server disconnect erases the client (`window.cpp:2460`), whose destructor clears windows while taking screen locks (`window.cpp:103`). The pin is dropped after the kernel lock has been reacquired.
* In-flight IPC retains its owner thread through `ipc_msg::ref()` (`ipc.cpp:80`). Finish checks that owner exists and is not stopped before status completion (`window.cpp:2577`). Normal process/thread stop does not create an unprotected owner pointer in this path.
* Scheduler redraw takes kernel then screen (`scheduler.cpp:216`); Finish releases the kernel before trying each screen lock, then drops the screen lock before sleep or fence wait. There is no direct kernel/screen lock inversion in its polling path.
* `screen::redraw(builder, ...)` clears flags while the scheduler screen lock remains held (`screen.cpp:470`); its outer overload submits the resulting list before that lock is released (`screen.cpp:482`, `scheduler.cpp:223`). A subsequent successful screen lock orders the fence after that scheduler submission.
* OS shutdown returns from the emulation loop before `state.symsys.reset()` runs (`thread.cpp:313`, `thread.cpp:330`); system destruction stops the timer before wiping services (`epoc.cpp:183`). Graphics-driver teardown follows frontend thread joins. The candidate comment that all server destruction follows the OS-thread join is imprecise: the system/server can be destroyed on the OS thread before it joins, but only after this Finish returns. The distinction does not expose a lifetime gap here.

## Strict deadline limitation

The polling and `finite_finish(remaining)` share a two-second budget. Reacquiring the kernel mutex in `unlocked_kernel::~unlocked_kernel()` has no deadline.

A source-permitted schedule is:

1. Finish releases the kernel so the timing-thread scheduler can drain pending redraw.
2. `animation_scheduler::invoke_due_animation` takes kernel and screen (`scheduler.cpp:216–230`).
3. `screen::redraw(driver)` calls `driver->submit_command_list` while both remain held (`screen.cpp:496`).
4. OpenGL submission calls `list_queue.push` (`graphics_ogl.cpp:1882`). `request_queue::push` waits without timeout while full (`queue.h:176`). Only consumption or explicit abort releases that wait.
5. If the consumer stalls, Finish's own polling deadline expires, but kernel-lock reacquisition waits behind that scheduler. Finish cannot return its timeout until queue capacity or abort lets the scheduler release the kernel.

Finish calls no unbounded legacy submission directly, but running the normal scheduler leaves this indirect dependency. Normal native four-submission success does not prove a worst-case end-to-end two-second bound under a stalled/full queue. This source limitation was reported to the parent and Root before finalizing this review.

A broader nonblocking scheduler design would affect `graphics_driver`, OpenGL admission, screen redraw, and scheduler retry. A failed admission would need to retain the already-composed owned command list and retry it without replaying CPU-side cache/texture mutations, keep redraw pending until admission, and release nested command resources safely during teardown. Merely dropping a list or clearing flags on rejection loses work. That larger change was not implemented in this review.

## Result

No new ordinary session, IPC-owner, screen-list, or driver-lifetime blocker was found within the reviewed normal frontend path. The polling/fence budget is finite; total duration is subject to existing kernel-lock backpressure. This review certifies neither guest execution nor rendering, display, vsync, or performance. Root owns the independent native runtime result.

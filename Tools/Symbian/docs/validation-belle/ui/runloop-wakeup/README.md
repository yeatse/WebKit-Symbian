# Wake callback unregister ordering

The original generic-loop setter failed the in-flight callback lifetime check;
the corrected Symbian setter passes it and the detached-callback control.
Both builds compile the actual three production functions. Their source and
extracted-function hashes, raw results and logs are retained, including the old
exit 1. The host adapter deliberately uses a stable callback pointer so that the
old failure is an ordering assertion rather than undefined callable destruction.

`wakeUpWithLock` invokes the callback while holding `m_loopLock`, whereas the old
setter changed it without synchronization. Clearing now acquires that lock,
so a `CRunLoopDriver` may close its native thread handle after any already-started
callback has finished. This is separate from removing controls before AVKON
releases its font array. The native callback only performs an atomic flag update
and `RThread::RequestComplete`; it does not recursively acquire the RunLoop lock.

See [the reproducer](../../../../tests/runloop-wakeup/README.md). These host
checks do not claim actual guest shutdown success.

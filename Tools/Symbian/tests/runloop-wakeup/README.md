# Native wake-up callback lifetime

From the checkout root:

```sh
python3 Tools/Symbian/tests/runloop-wakeup/run.py --output /absolute/new-output
```

The runner extracts the actual generic RunLoop setter and wake functions and
compiles them with a host C++20 compiler. Host mutex/condition wrappers replace
WTF's platform synchronization. A stable atomic function-pointer adapter avoids
making the deliberately broken old case depend on destruction of a callable
that another thread is executing; it tests the registration lifetime contract.

A worker holds an invocation of the native callback open while the owner thread
unregisters it. Unregistration must wait for that invocation before the owner
may be destroyed. Subsequent wakeups must not invoke the detached callback.
The old setter returns too early and fails the first check; the Symbian setter
now takes the same lock used around invocation and passes both checks. The
native callback only completes an RThread request; it does not reenter RunLoop.

`--source` selects a historical source for an A/B check. Each output directory
must be new and retains extracted production functions, input hash, compiler
arguments, log and result. The host regression does not exercise RThread or
prove AVKON shutdown; those require the separate actual application exit test.

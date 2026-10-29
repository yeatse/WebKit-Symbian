# Native EndTask exit — candidate8 failure localization

This run remains **FAIL**. Host `71cbce0a…` includes the emulator's null-TLB
correction: it now reports the guest's invalid read of address 4 instead of
crashing the host. Host exit 0 therefore does not establish success. The guest
panicked with `KERN-EXEC 3`, and the final browser destructor marker is absent.
See [`exit/validation-result.json`](exit/validation-result.json).

Browser SHA256:
`1767da19ec00d9c823c60ff13d4f1c0a828d28cbd2abe97691f78c95ddeaca59`.
Host SHA256:
`71cbce0aa8a8eff9d167614a34f2870d26387df18b3940cf4818a34524933093`.
The helper identity, official EndTask request, no-WM-close/no-force records,
and fresh page URL are retained in each observation directory.

The new browser stage logs show `exit command 256`, then `appui destructor
begin`, followed by the exception before `delete suggestions`. This localizes
the failure to `RemoveFromStack`. The official framework's destruction order
explains the null font array: `CEikonEnv::DestroyEnvironment` releases its fonts
before `CCoeEnv::DestroyEnvironmentEnd` deletes the application UI. Removing a
control invokes `HandleStackChanged`, which can focus/layout the remaining
control and request fonts. Application UI teardown must not initiate this
native layout after the environment has released them.

`exit-gdb-stack/guest-stack.bin` is a 512-byte **guest** stack snapshot covering
`0x57d730..0x57d930`, with SP `0x57d7b0`. It was obtained through the emulator's
guest process address translation, not from the host thread's stack. The raw
GDB log records the calls and words. Return sites include `CEikonEnv::Font`,
`AknLayoutUtils::LayoutFontFromId`, and `TAknLayoutText::LayoutText`. The shorter
snapshot does not reach the browser return frame; the browser's phase markers
independently delimit the failing operation.

The following candidate moves the idempotent removal into the framework's
`PrepareToExit` callback, before font release, and suppresses client/editor
callbacks during teardown. `CCoeEnv::Execute` invokes that callback only after
the outer `CActiveScheduler::Start` has returned/unwound. The browser's run-loop
active object can therefore be cancelled and deleted at that point. The base
AVKON callback remains in the chain. This directory is failure evidence, not
runtime validation of that subsequent fix.

The ordering is documented in the pinned official
[`CCoeEnv::Execute` / `DestroyEnvironmentEnd`](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/lafagnosticuifoundation/cone/src/COEMAIN.CPP),
[`CEikonEnv::DestroyEnvironment`](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/commonuisupport/uikon/coresrc/EIKENV.CPP),
and [`CCoeAppUi::HandleStackChanged`](https://github.com/SymbianSource/oss.FCL.sf.mw.classicui/blob/dcea899751dfa099dcca7a5508cf32eab64afa7a/lafagnosticuifoundation/cone/src/COEAUI.CPP).

Raw logs retain their exact bytes in gzip; `manifest.json` records the stored
and original hashes. The prior host-crashing failure is preserved separately
in [extra-candidate5](../extra-candidate5/README.md).

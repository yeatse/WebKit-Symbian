# Browser connection events: overall failure retained

The actual candidate 11 page passed all 19 checks: two real native disconnect/
reconnect cycles produced four correctly ordered trusted window events and
matching `navigator.onLine` values. Ordinary Attach/Close churn produced no
extra events. No synthetic DOM event or HTTP-failure inference was used.

**The complete acceptance run failed.** After the controller requested official
EndTask for the matching browser PID/SID, the host exited with status -11.
The controller did not complete its normal-exit check, and the strict validator
rejects the run. The page result is not promoted to overall PASS and this does
not establish a cause or fix for the earlier intermittent host exit failures.

The original browser/resources/profile were restored, including the original
profile directory inode. The raw page/controller logs, screenshot and result
are retained here; the unmodified host log remains external with its hash and
size recorded in [manifest.json](manifest.json). No additional crash replay,
memory dump or diagnostic shim was used. Browser lifecycle acceptance remains
outstanding.

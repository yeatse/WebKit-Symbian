# Native EndTask with picker open — candidate9 PASS

The unchanged candidate9 browser and unchanged official EndTask helper pass
the strict exit validator on the updated EKA host. The native picker was open
when the helper sent `TApaTask::EndTask`. The browser dismissed the picker,
ran `PrepareToExit` and all destructor stages, and then exited normally. The
host exited naturally with code 0; the driver sent neither WM close nor
forced cleanup. No guest panic or access violation occurred.

| Artifact | SHA256 |
| --- | --- |
| Browser | `0d6e88803e2c5ea0a3937c527f8a9141f9752666f1c6ca4752416fc444f1a6f7` |
| EKA host | `046bc41192886a671c144983dd74d1e45de6860d179ae073a335a5db997c1e64` |
| EndTask helper | `e668e662f15b563b71e5237b910db422f86fc265de7f6a88925b310776e7489d` |

The observation ran from **2026-10-29 21:24:37.805 UTC** to
**21:24:56.254 UTC**. [validation.json](exit/validation.json) is **PASS**;
replaying the unchanged validator against the raw inputs reproduced that
exact result. [observation.json](exit/observation.json) retains the actual
host exit and the explicit no-WM/no-force flags. The observation's intermediate
status is `OBSERVATION_COMPLETE_PENDING_VALIDATION`; the separate validation
record supplies its final verdict.

The [picker screenshot](exit/picker-open.png),
[helper launch screenshot](exit/helper-path-entered.png), and
[native input actions](exit/actions.json) show the interaction. The helper log
records the checked browser UID, process ID and executable path, followed by
the official request and `REQUEST_SENT`. Its optional final `EXIT` line did
not flush before the emulator CLI closed on the browser's termination. The
complete browser destructor marker, normal host exit, and kernel log's main
`WebKit` thread exit code 0 provide the remaining required evidence. The
kernel's generic log wording is “forcefully killed” even for this normal
thread exit; the harness's `forced_cleanup` is independently **false**.

The preceding [candidate9 runs on host71cb](../exit-candidate9/README.md)
remain **FAIL**. They reached the browser destructor but hung in the native
transition adapter waiting for an ALF policy cancellation. This host includes
the [0022 cancellation correction](../../alf-cancel/README.md), along with
0020 connection observation and 0021 memory-threshold support. Thus this is
the same-browser retry on the updated host, not a claim that the host binary
differs by only one patch. The earlier font-lifetime failure and its repair
are documented in [candidate8](../exit-candidate8/README.md).

This validates native picker application shutdown in EKA's RM-707 environment.
It does not validate the subsequently merged touch, download, clipboard or
memory-pressure browser batch, nor physical-device exit behavior.

`manifest.json` records original and stored hashes for 17 files. Raw logs and
Python snapshots are gzip-compressed without changing their decompressed
bytes. `source-identities.json` and `staged-images.json` preserve the tested
fixture, validator, helper driver, browser and resource identities. The
private driver snapshots contain environment-specific workspace paths;
they document the run and are not portable launch scripts. `previous-*` logs
are explicitly quarantined pre-run files and were not inputs to validation.

# Explicit NetworkControl image capability

The SDK converter previously rejected the real `RConnection` controller's
`0x2400` capability mask. Patch 0005 permits official `NetworkControl` bit 10
alongside `NetworkServices` bit 13 in both conversion and inspection. All other
bits remain unsupported. The browser stays at `0x2000`; this is not an OS,
installer or signing authorization change.

The 56-test E32 suite passes. Its two new cases cover both accepted profiles
and every other capability bit, using valid header CRCs for inspection
negatives. `patch-replay.json` records exact baseline revision
`b105f752f84d1b5640052f64c4a703017810d7b8`, sequential patches 0001–0005,
byte-identical source replay and repeat reverse checks. `verification.json`
records the old/new tool hashes, byte-identical repeat build, source-header
identity and actual controller E32 identity/capability/CRC. No SDK commit or
push was made.

These are host build/format results. The controller and browser's real event
sequence still require separate guest execution evidence. The controller
uses the converter directly; no post-conversion capability mutation remains.
The local isolated host build requires `/workspace/host-env.sh` for its host
library paths; those paths are not a clean-runner dependency requirement.

# Resource compiler host verification

The SDK 0.3.0 Linux compiler crashed in `String::GetBuffer(this=null)` while
compiling valid enum aliases and symbolic `LLINK` fields without line directives.
The source filename had not been initialized on Linux, and the identifier action
did not register its location before reading it. The narrow fix covers both
initializations and keeps the generated parser in sync with its grammar.

The fixed compiler passed 14 actual compiler checks, including the shipped
compiler's crash as a negative control, forward/backward link relocation,
generated IDs, byte-identical equivalent numeric resources and ordinary error
diagnostics. The complete official Avkon enum input and the real browser dialog
resource also compiled successfully. Resource outputs from the debug and
optimized fixed compilers were byte-identical.

A clean build directory inside the WebKit checkout fetched the pinned upstream
source, applied all three patches, rebuilt, passed the same 14 checks and
installed into an isolated test SDK. Repeating the command with the populated
source cache passed again and produced the same executable bytes. The working
SDK uses that same verified tool. Only its `bin/rcomp` was replaced; `uidcrc`,
SDK headers and target libraries were unchanged by this correction.

See [the source and reproducible command](../../patches/sdk-0.3.0/rcomp/README.md).
`verification.json` records the complete patched-source hashes and compiler;
`regression.json` records the compiler tests; `rebuilds.json` records the clean,
cached and real-resource checks. This is host build evidence. It does not assert
that the new browser UI has passed guest interaction or physical-device tests.

Four separate wrapper tests passed using temporary local Git repositories:
recovery after `git init` inside an outer checkout, refusal of an unexpected
cached remote, pinned-object reads despite dirty sources and inherited Git
environment variables, and rejection of unknown tool bytes before fetching or
building. Their output is in `wrapper-contracts.log`.

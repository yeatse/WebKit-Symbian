# Owned curl native resolver bridge

Actual controlled DNS exposed a runtime boundary: host DNS and native
RHostResolver hostname results passed, but ROM Open C getaddrinfo hostname
results failed the public loopback/port contract for both protocol0 andTCP6.
Numeric Open C IPv6 still passed. The C22 browser DNS run remains FAIL.
No ROM DLL, import proxy or speculative reference-capacity patch was changed.

Three owned shims provide a bounded C ABI and curl allocator adapter. Native
C++ supplies at most16 unique records, network-order address bytes and Scope.
It accepts ASCII/IDNA hostnames up to253 bytes and rejects raw UTF8/overlength
rather than truncating. Numeric addresses use native Input without DNS.
One RTimer provides a10s total request/enumeration deadline inside curl's
existing resolver worker. Cancellation waits for completion before Close.
No scheduler/thread or diagnostic logging is added. Next EOF and NotFound
after at least one native entry terminate enumeration; initial lookup errors
and every timeout/cancel/no-memory/other operational error remain failures.
Physical-provider end codes and mobile-network latency still need device tests.

More than16 unique addresses or reaching64 native entries fails the entire
lookup with EAI_FAIL. Results are never silently truncated or described as
unbounded DNS support. C constructs Curl_addrinfo nodes using curl's allocator,
sets family/protocol/port/Scope itself and places IPv6 candidates before IPv4.
Partial allocation failure frees the complete partial list through curl's
actual Curl_freeaddrinfo. Allocation/native errors leave the output empty.

The updated curl patch explicitly enables CXX when SYMBIAN is selected, adds
both C and CPP to libcurl after its generated source list, defines the private
bridge switch, and supplies SDK native includes. The archive contains its own
native implementation; it does not depend on unresolved WebCore functions.
Browser, link-webcore and standalone SDK links import esock/insock. Build cache
network version2 forces a rebuild of earlier cached curl. No Multi/Cookie hunk
was replaced; only the owned insock line was changed in shared browser CMake.

Actual C and native CPP ARM object compiles passed, including the final narrow
Next-NotFound branch. Fresh six-file patch application and repeated idempotent
call passed. Actual C allocator/port/family/error cleanup tests with real curl
free code passed19 checks under ASan/UBSan. Root's four-case native-record run
passed on the production CPP snapshot before the Next-NotFound compatibility
branch: AAAA-only, dual, fallback and numeric address bytes/families/Scope0,
natural CLI host0/positive Main0, zero panic and full original restoration.
The tested EKA uses EOF, so that existing tested path is semantically unchanged.

source-freeze.json pins the nine production files. Root owns full libcurl,
standalone SDK link, engine/browser C24 build and genuine DNS/TLS follow-up.
Those results, actual public Internet/real-device DNS, cancellation and scoped
addresses must not be inferred from native-field or host allocator checks.
All original failed runs and previous successful IPv4/literal harnesses remain
unchanged. No original profile/config, private key, binary or host dump archived.

C24 selector verification rejected an otherwise successful build because curl
CMake replaced an earlier COMPILE_DEFINITIONS append. That image was not run.
The final property now retains the selector and build-deps validates real code
before version2. C25 actual generated macros, archive/native call and final ELF
bridge/ALPN/H2 symbols and esock/insock link passed; controlled browser DNS results
remain separate until the fresh Root run completes.

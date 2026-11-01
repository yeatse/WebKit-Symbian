# CI48 host-only filesystem adapter repair

The actual CI48 native-filesystem step failed before SDK/engine/package work:
its existing fake WTF include tree lacked `<wtf/Platform.h>` when the real new
production `SymbianFilePath.h` was included. Simply adding that header would
leave its old byte-only String, missing wide calls and host four-byte wchar_t
contract unresolved. CI48 is retained as FAIL, with no SIS artifact.

The six-file test-only patch has no production edits. It adds finite Platform
and StringView include adapters, gives the existing String fixture independent
UTF-16 code units through host iconv, and explicitly builds with -fshort-wchar.
The unchanged real production helper is included from `Source/WTF`, not copied.
Test-only wstat/waccess convert those 16-bit units through iconv before calling
real host stat/access; they never call the host libc's four-byte-wchar_t wide
interfaces. Existing fault wrappers and all original assertions remain active.
This is host control-flow evidence, not native ABI/Open C/phone acceptance.

The complete ordinary host suite passes with -Wall -Wextra -Werror. The complete
ASan/UBSan variant also passes. Added controlled files exercise ASCII content,
zero-length content and a BMP + surrogate-pair filename through actual production
exists/size/mtime/following-type calls, preserve symlink no-follow behavior, and
reject empty/NUL/257-unit paths before wide calls. The unchanged production
helper also accepts exactly 256 units and writes its final NUL. This fixture
compiles FileSystemSymbian.cpp, not the separate POSIX openFile implementation;
actual native/wide open reading remains covered by the separate pinned guest
worker evidence already committed in 67140bd, not by this host run.

`host-adapter-utf16.patch` and `patch-manifest.json` pin the exact changes against
67140bdc320bb9c911bf6f4045a3b38755b7707a. Shared source, staging, runtime,
SDK prefix and emulator assets were not edited by this agent. Root applies
and commits only after review. To repeat the external prepared normal check:

```
/workspace/symbian-ci48-validation/native-filesystem-fix/source/Tools/Symbian/tests/native-filesystem/run.sh
python3 /workspace/symbian-ci48-validation/native-filesystem-fix/run-sanitized.py
```

Both checks create their own finite temporary filesystem and remove it normally.
No guest, display, audio, server or crash/heap diagnostic was started.

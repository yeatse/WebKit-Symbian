# Split demo

A small AVKON application to try `scripts/split-e32.py` on a phone without
building WebKit. It calls across the image the ways WebKit does (direct
branches, function pointers in writable and in constant data, vtables, a
static constructor, a C++ exception and a leave) and links the browser's
system DLLs.

    tests/split-demo/build.sh    # -> WebKitBuild/Symbian/build/split-demo/split-demo.sis

The package installs four applications, on the drive chosen at install:

| Menu    | Image                                            |
|---------|--------------------------------------------------|
| Split A | one EXE, as linked (the control)                 |
| Split B | split: a stub EXE and its DLLs `sdb0.dll`...     |
| Split C | as B, with `KImageNoCallEntryPoint` on each DLL  |
| Split D | as B, cut into two DLLs only                     |

Each shows its results, appends them to `C:\Data\splitdemo\<name>.txt`
(one line per step, written before the next step runs; `main` comes first),
and exits after 20 seconds or at any key. `virtual`, `throw`, `leave` and
`libc` are checked; `init`, `chain` and `cchain` must equal Split A's.

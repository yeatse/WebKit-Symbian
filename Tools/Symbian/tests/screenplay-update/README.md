# Browser-private Surface Update client host checks

Run from the repository root with Python 3 and a host GCC or Clang C++17 compiler:

```sh
python3 Tools/Symbian/tests/screenplay-update/run.py
python3 Tools/Symbian/tests/screenplay-update/run.py --sanitize
```

The runner directly compiles `Tools/Symbian/browser/ScreenPlayUpdateSession.cpp`
with its production headers and the small session, descriptor and completion
stand-ins in `fake-sdk/`. It does not copy or extract the implementation, require
an SDK, download files, or execute a guest. The default build uses a temporary
directory that is removed afterwards. A successful run exits zero and prints a
JSON result with 8 cases, 75 assertions and the production source hashes.

Use `--cxx clang++` or `CXX=clang++` to select a compiler. `--sanitize` adds
AddressSanitizer, UndefinedBehaviorSanitizer and address use-after-scope checks;
these require the chosen compiler's sanitizer runtime. To retain the host binary
and a JSON report, add `--build-dir /tmp/screenplay-update-build` and
`--output /tmp/screenplay-update-result.json`. `--browser-dir` selects a different
production browser directory when checking an isolated candidate. Source, build
and output paths containing spaces are supported. Quote compiler paths within the
`--cxx` command string if they contain spaces.

The eight cases cover the fixed production server, protocol 1.1.3 and six default
message slots; connection errors; notification caching and replacement; all three
invalid submission arguments; submission IPC order and argument slots; null,
empty and nonempty dirty regions; invalid display counts; cancellation before and
after submission; and submission errors followed by cancellation. They exercise
all public client functions, including idempotent `Close` after cancellation.

The fake backend records queued notifications and completes them when cancellation
arrives. This distinguishes server-owned requests from cached client requests and
checks duplicate local completion. It checks the actual client's decisions and
IPC arguments; it does not validate the real server, kernel message slots,
request scheduling, device policy, physical display, vsync or performance.

The official `Close` precondition still applies: outstanding notifications must
already have been cancelled and drained before caller-owned status storage is
released. `Close` closes the handle and clears cached pointers; it does not
complete unsubmitted notification storage. The tests follow that precondition.

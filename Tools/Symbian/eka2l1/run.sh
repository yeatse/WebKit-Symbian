#!/usr/bin/env bash
# Run jsc with fresh guest/host logs and require an exit marker and expected PASS lines.
# EKA2L1 names the emulator; EKA2L1_DEVICE defaults to RM-707; TIMEOUT is seconds.
# XDG directories default below WORK/eka2l1. Install firmware using the same XDG_DATA_HOME.
# EKA2L1_EXPECT_PASS is a newline-separated override for the expected full output lines.
. "$(dirname "$0")/../scripts/common.sh"
: "${EKA2L1:?set EKA2L1 to the eka2l1_qt executable}"
export XDG_DATA_HOME=${XDG_DATA_HOME:-$WORK/eka2l1/data}
export XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$WORK/eka2l1/config}
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-$WORK/eka2l1/cache}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-$WORK/eka2l1/runtime}
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
export EKA2L1_DATA=${EKA2L1_DATA:-$XDG_DATA_HOME/EKA2L1/data}
export EKA2L1_DEVICE=${EKA2L1_DEVICE:-RM-707}
export TIMEOUT=${TIMEOUT:-600}
export SYMBIAN_JSC_IMAGE=$E32_BUILD/e32/jsc.exe
export SYMBIAN_ICU_DATA=$ICU_DIR/install/share/icu/icudt70l.dat
export SYMBIAN_TESTS_DIR=$REPO/tests
export SYMBIAN_RUNS_DIR=$WORK/runs
python3 - "$@" <<'PY'
import datetime
import fcntl
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def stop_group(process):
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    # xvfb-run or a failed emulator can leave children after its leader exits.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def interrupted(signum, frame):
    raise KeyboardInterrupt


signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGINT, interrupted)
data = Path(os.environ["EKA2L1_DATA"]).resolve()
data.mkdir(parents=True, exist_ok=True)
lock = (data / ".jsc-run.lock").open("w")
try:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
except BlockingIOError:
    sys.exit("A JSC run already uses this emulator data directory")
arguments = sys.argv[1:] or ["C:/Data/basic.js"]
known_tests = {path.name: "PASS: " + path.stem for path in Path(os.environ["SYMBIAN_TESTS_DIR"]).glob("*.js")}
expected = os.environ.get("EKA2L1_EXPECT_PASS", "").splitlines()
if not expected:
    expected = [known_tests[name] for argument in arguments
                if (name := argument.replace("\\", "/").rsplit("/", 1)[-1]) in known_tests]
if not expected or any(not item for item in expected):
    sys.exit("Set EKA2L1_EXPECT_PASS to the expected full PASS line(s) for custom scripts")
if "-x" not in arguments:
    arguments.insert(0, "-x")
if any(argument.replace("\\", "/").endswith("/resizable-arraybuffer.js") for argument in arguments):
    if not any(argument.startswith("--useSharedArrayBuffer=") for argument in arguments):
        arguments.insert(0, "--useSharedArrayBuffer=true")
# Quote using the native jsc argument parser's double-quote/backslash convention.
argument_string = " " + " ".join('"' + argument.replace("\\", "\\\\").replace('"', '\\"') + '"' for argument in arguments)
if len(argument_string.encode()) >= 4095:
    sys.exit("Guest command line exceeds jsc's 4096-byte buffer")
runs = Path(os.environ["SYMBIAN_RUNS_DIR"])
runs.mkdir(parents=True, exist_ok=True)
stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
run = Path(tempfile.mkdtemp(prefix="jsc-" + stamp, dir=runs))
c = data / "drives/c"
for directory in (c / "sys/bin", c / "Data", c / "resource/icu"):
    directory.mkdir(parents=True, exist_ok=True)
shutil.copy2(os.environ["SYMBIAN_JSC_IMAGE"], c / "sys/bin/jsc.exe")
shutil.copy2(os.environ["SYMBIAN_ICU_DATA"], c / "resource/icu/icudt70l.dat")
for script in Path(os.environ["SYMBIAN_TESTS_DIR"]).glob("*.js"):
    shutil.copy2(script, c / "Data" / script.name)
guest_log = c / "Data/jsc.log"
if guest_log.exists():
    guest_log.rename(run / "previous-jsc.log")
emulator = str(Path(os.environ["EKA2L1"]).resolve())
command = [emulator, "--device", os.environ["EKA2L1_DEVICE"], "--run", "C:\\sys\\bin\\jsc.exe", argument_string]
if not os.environ.get("DISPLAY"):
    command = ["xvfb-run", "-a", "-s", "-screen 0 1280x900x24 -nolisten tcp"] + command
host_log = run / "emulator.log"
panic = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC", re.I)
deadline = time.monotonic() + float(os.environ["TIMEOUT"])
failure = None
process = None
try:
    with host_log.open("w") as output:
        process = subprocess.Popen(command, cwd=str(Path(emulator).parent), stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        while process.poll() is None:
            if panic.search(read(host_log)):
                failure = "guest panic or access violation"
                break
            if time.monotonic() >= deadline:
                failure = "host timeout"
                break
            time.sleep(0.2)
        if not failure and process.returncode != 0:
            failure = "emulator exited with status " + str(process.returncode)
except KeyboardInterrupt:
    failure = "run interrupted"
finally:
    if process is not None:
        stop_group(process)
    guest_output = read(guest_log)
    (run / "jsc.log").write_text(guest_output)
print(guest_output, end="" if guest_output.endswith("\n") else "\n")
print("logs: " + str(run), file=sys.stderr)
lines = guest_output.splitlines()
if panic.search(read(host_log)):
    failure = failure or "guest panic or access violation"
if "jsc exiting 0" not in lines:
    failure = failure or "missing successful guest exit marker"
if any(re.match(r"jsc exiting (?!0$)", line) for line in lines):
    failure = failure or "guest reported a failure exit"
if any(line.startswith("FAIL:") for line in lines):
    failure = failure or "guest test reported failure"
missing = [item for item in expected if item not in lines]
if missing:
    failure = failure or "missing expected output: " + ", ".join(missing)
if failure:
    sys.exit("FAIL: " + failure)
print("PASS: guest exit and expected test markers verified")
PY

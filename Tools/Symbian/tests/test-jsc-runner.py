#!/usr/bin/env python3
"""Exercise result and timeout checks without firmware or an emulator.

The fake emulator does not validate Symbian or JavaScriptCore behavior.
"""
import os
from pathlib import Path
import subprocess
import tempfile

RUNNER = Path(__file__).resolve().parents[1] / "eka2l1/run.sh"
FAKE_EMULATOR = r'''#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys
import time

assert '"-x"' in sys.argv[-1], sys.argv
case = os.environ["CASE"]
path = Path(os.environ["EKA2L1_DATA"]) / "drives/c/Data/jsc.log"
if case == "stale":
    sys.exit(0)
if case == "timeout":
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    (path.parent / "child.pid").write_text(str(child.pid))
    time.sleep(30)
path.write_text(("" if case == "missing-pass" else "PASS: basic\n")
                + ("jsc exiting 3\n" if case == "guest-failure" else "jsc exiting 0\n"))
if case == "panic":
    print("Access violation in guest", flush=True)
'''

with tempfile.TemporaryDirectory(prefix="jsc-runner-") as temporary:
    root = Path(temporary)
    image = root / "build/e32/e32/jsc.exe"
    image.parent.mkdir(parents=True)
    image.write_bytes(b"test image")
    icu = root / "icu/install/share/icu/icudt70l.dat"
    icu.parent.mkdir(parents=True)
    icu.write_bytes(b"test data")
    emulator = root / "emulator.py"
    emulator.write_text(FAKE_EMULATOR)
    emulator.chmod(0o755)
    cases = [("pass", True), ("stale", False), ("panic", False),
             ("guest-failure", False), ("missing-pass", False), ("timeout", False)]
    for case, success in cases:
        data = root / case / "data"
        (data / "drives/c/Data").mkdir(parents=True)
        if case == "stale":
            (data / "drives/c/Data/jsc.log").write_text("PASS: basic\njsc exiting 0\n")
        environment = dict(
            os.environ, E32_BUILD=str(root / "build/e32"), ICU_DIR=str(root / "icu"),
            SYMBIAN_WEBKIT_WORK=str(root), EKA2L1=str(emulator), EKA2L1_DATA=str(data),
            XDG_DATA_HOME=str(root / case / "xdg-data"),
            XDG_CONFIG_HOME=str(root / case / "config"),
            XDG_CACHE_HOME=str(root / case / "cache"),
            XDG_RUNTIME_DIR=str(root / case / "runtime"),
            DISPLAY=":fake", TIMEOUT="1", CASE=case, EKA2L1_EXPECT_PASS="PASS: basic")
        result = subprocess.run(["bash", str(RUNNER)], env=environment, capture_output=True,
                                text=True, timeout=10)
        if (result.returncode == 0) != success:
            raise AssertionError((case, result.stdout, result.stderr))
        if case == "timeout":
            pid = int((data / "drives/c/Data/child.pid").read_text())
            status = Path(f"/proc/{pid}/status")
            if status.exists() and "State:\tZ" not in status.read_text():
                raise AssertionError("Timed-out emulator child is still running")
        print("PASS: runner " + case)

#!/usr/bin/env python3
"""Run the same ARM11 E32 JSC image under LLInt, Baseline and DFG profiles."""

import argparse
import datetime
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS / "tests"))
from arm11_jit_common import PANIC, PROFILE_OPTIONS, compare_results, validate_output


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def interrupted(signum, frame):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    raise KeyboardInterrupt


def stop_group(process):
    for signum, seconds in ((signal.SIGTERM, 3), (signal.SIGKILL, 3)):
        try:
            os.killpg(process.pid, signum)
        except ProcessLookupError:
            break
        try:
            process.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            pass
    process.wait(timeout=3)


def run_profile(args, mode, run):
    run.mkdir()
    guest = args.data / "drives/c/Data/jsc.log"
    if guest.exists():
        guest.rename(run / "previous-jsc.log")
    arguments = ["-x", *PROFILE_OPTIONS[mode], "C:/Data/arm11-jit.js", "--", mode]
    argument_string = " " + " ".join('"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"' for value in arguments)
    command = [str(args.emulator), "--device", args.device, "--run", r"C:\sys\bin\jsc_arm11.exe", argument_string]
    host = run / "emulator.log"
    failure = None
    process = None
    result = None
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        with host.open("w") as output:
            process = subprocess.Popen(command, cwd=args.emulator.parent, stdout=output,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            deadline = time.monotonic() + args.timeout
            while process.poll() is None:
                if PANIC.search(read(host)):
                    failure = "guest panic or access violation"
                    break
                if time.monotonic() >= deadline:
                    failure = "host timeout"
                    break
                time.sleep(0.1)
            if not failure and process.returncode != 0:
                failure = "host exited with status " + str(process.returncode)
    except KeyboardInterrupt:
        failure = "run interrupted"
    except OSError as error:
        failure = str(error)
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        if process is not None:
            stop_group(process)
        text = read(guest)
        (run / "jsc.log").write_text(text)
    if PANIC.search(read(host) + text):
        failure = failure or "guest panic or access violation"
    if "jsc exiting 0" not in text.splitlines():
        failure = failure or "missing successful guest exit marker"
    if any(re.match(r"jsc exiting (?!0$)", line) for line in text.splitlines()):
        failure = failure or "guest reported a failure exit"
    if not failure:
        try:
            result = validate_output(text, mode)
        except (ValueError, AssertionError) as error:
            failure = str(error)
    report = {"mode": mode, "options": PROFILE_OPTIONS[mode], "host_exit": process.returncode if process else None,
              "failure": failure, "result": result}
    (run / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True, help="ARM11 E32 jsc.exe, not the C_LOOP release image")
    parser.add_argument("--icu-data", type=Path, required=True)
    parser.add_argument("--emulator", type=Path, default=os.environ.get("EKA2L1"))
    parser.add_argument("--data", type=Path, default=os.environ.get("EKA2L1_DATA"))
    parser.add_argument("--device", default=os.environ.get("EKA2L1_DEVICE", "RM-707"))
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("--modes", choices=tuple(PROFILE_OPTIONS), nargs="+", default=list(PROFILE_OPTIONS))
    args = parser.parse_args()
    if args.data is None and os.environ.get("XDG_DATA_HOME"):
        args.data = Path(os.environ["XDG_DATA_HOME"]) / "EKA2L1/data"
    if args.emulator is None or args.data is None:
        parser.error("Set --emulator/EKA2L1 and --data/EKA2L1_DATA or XDG_DATA_HOME")
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be finite and positive")
    if len(set(args.modes)) != len(args.modes):
        parser.error("--modes must not repeat an execution profile")
    args.emulator = args.emulator.resolve(strict=True)
    args.data = args.data.resolve(strict=True)
    if args.data.name != "data" or args.data.parent.name != "EKA2L1":
        parser.error("--data must have the XDG layout <XDG_DATA_HOME>/EKA2L1/data")
    # The emulator locates files through XDG, not the runner's --data argument.
    os.environ["XDG_DATA_HOME"] = str(args.data.parent.parent)
    args.image = args.image.resolve(strict=True)
    args.icu_data = args.icu_data.resolve(strict=True)
    locks = []
    for name in (".jsc-run.lock", ".browser-run.lock"):
        lock = (args.data / name).open("a")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error("Another run uses this emulator data directory")
        locks.append(lock)
    c = args.data / "drives/c"
    for directory in (c / "sys/bin", c / "Data", c / "resource/icu"):
        directory.mkdir(parents=True, exist_ok=True)
    image = c / "sys/bin/jsc_arm11.exe"
    shutil.copy2(args.image, image)
    shutil.copy2(args.icu_data, c / "resource/icu/icudt70l.dat")
    shutil.copy2(TOOLS / "tests/arm11-jit.js", c / "Data/arm11-jit.js")
    args.reports.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    run = Path(tempfile.mkdtemp(prefix="eka-arm11-jit-" + stamp, dir=args.reports))
    summary = {"image_sha256": sha256(args.image), "installed_image_sha256": sha256(image),
               "icu_sha256": sha256(args.icu_data), "emulator_sha256": sha256(args.emulator),
               "test_sha256": sha256(TOOLS / "tests/arm11-jit.js"), "device": args.device, "profiles": [], "failure": None}
    try:
        for mode in args.modes:
            report = run_profile(args, mode, run / mode)
            summary["profiles"].append(report)
            print(("FAIL: " if report["failure"] else "PASS: ") + mode, flush=True)
            if report["failure"]:
                summary["failure"] = report["failure"]
                break
        if not summary["failure"]:
            compare_results([profile["result"] for profile in summary["profiles"]])
    except (ValueError, AssertionError) as error:
        summary["failure"] = str(error)
    (run / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("Logs: " + str(run))
    if summary["failure"]:
        sys.exit("FAIL: " + summary["failure"])
    profiles = ", ".join(args.modes)
    if len(args.modes) > 1:
        print("PASS: ARM11 profiles " + profiles + "; execution evidence and matching results verified")
    else:
        print("PASS: ARM11 profile " + profiles + "; execution evidence verified")


if __name__ == "__main__":
    main()

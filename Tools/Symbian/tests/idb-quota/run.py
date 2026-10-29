#!/usr/bin/env python3
"""Bounded quota regression across three independent EKA launches."""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time
from urllib.parse import urlencode, urlsplit
import uuid

from contract import CASES, validate

SPEC = importlib.util.spec_from_file_location("network_runner", Path(__file__).resolve().parents[1] / "run-browser-network-tests.py")
network = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(network)


def run_phase(args, phase, token, output, work, lock):
    total = len(CASES[phase])
    marker = f"IDB_QUOTA_DONE run={token} phase={phase} passed={total} total={total}"
    url = args.origin + "/index.html?" + urlencode({"run": token, "phase": phase, "basePort": urlsplit(args.origin).port})
    report_path = args.reports / f"{token}-{phase}.json"
    if report_path.exists(): raise RuntimeError("run report already exists")
    previous = {path.resolve() for path in (work / "runs").glob("browser-*")}
    environment = os.environ.copy()
    wait = 250 if phase == "write" else 70
    environment.update({"WAIT": str(wait), "EKA2L1_EXPECT_PASS": marker,
                        "EKA2L1_BROWSER_LOCK_FD": str(lock.fileno()),
                        "SYMBIAN_WEBKIT_WORK": str(work), "EKA2L1_DATA": str(args.data)})
    environment.pop("SCREENSHOT", None)
    launcher = output / (phase + "-launcher.log")
    process = None
    descendants = {}
    timed_out = False
    with network.adopt_orphaned_children() as excluded:
        try:
            with launcher.open("w") as stream:
                process = subprocess.Popen([str(network.BROWSER_RUNNER), str(args.browser), url],
                    stdout=stream, stderr=subprocess.STDOUT, env=environment,
                    start_new_session=True, pass_fds=(lock.fileno(),))
                deadline = time.monotonic() + wait + 45
                while process.poll() is None:
                    network.collect_descendants(descendants, excluded)
                    if time.monotonic() >= deadline:
                        timed_out = True; break
                    time.sleep(.2)
        finally:
            if process is not None: network.stop_process_tree(process, descendants, excluded)
    if report_path.exists():
        (output / (phase + "-report.json")).write_bytes(report_path.read_bytes())
    if timed_out: raise RuntimeError(f"{phase}: host watchdog expired")
    if process.returncode: raise RuntimeError(f"{phase}: browser driver exited {process.returncode}; see {launcher}")
    match = re.search(r"^logs: (.+)$", network.read(launcher), re.M)
    if not match: raise RuntimeError("driver did not identify logs")
    run = Path(match.group(1)).resolve()
    if run in previous or not run.is_relative_to((work / "runs").resolve()):
        raise RuntimeError("stale or unrelated guest logs")
    guest = network.read(run / "log.txt")
    host = network.read(run / "emulator.log")
    if not guest or not host or network.PANIC.search(host + "\n" + guest):
        raise RuntimeError("missing logs or guest panic/access violation")
    if not re.search(re.escape(marker) + r"[ \t]*$", guest, re.M) or re.search(r"IDB_QUOTA_(FATAL|TIMEOUT)", guest) or re.search(r"\bFAIL ", guest):
        raise RuntimeError("missing fresh success marker or guest failure")
    for name in CASES[phase]:
        expected_case = f"IDB_QUOTA_CASE run={token} phase={phase} PASS {name}"
        if not re.search(re.escape(expected_case) + r"[ \t]*$", guest, re.M):
            raise RuntimeError("report not corroborated by console: " + name)
    report = json.loads(report_path.read_text())
    validate(report, token, phase, args.origin)
    return {"phase": phase, "passed": total, "driver_exit": process.returncode,
            "logs": str(run), "report": str(report_path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("browser", type=Path)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--phase", choices=("all", "cleanup"), default="all")
    parser.add_argument("--run", help="reuse only for cleanup of an interrupted token")
    parser.add_argument("--origin")
    parser.add_argument("--data", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    args.origin = args.origin or "http://localhost:8790"
    parsed = urlsplit(args.origin)
    if parsed.scheme != "http" or not parsed.hostname or not parsed.port or parsed.path or parsed.query or parsed.fragment:
        parser.error("--origin must be an HTTP origin without a path")
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", network.SYMBIAN.parents[1] / "WebKitBuild/Symbian")).resolve()
    xdg = Path(os.environ.get("XDG_DATA_HOME", work / "eka2l1/xdg/data"))
    args.data = (args.data or Path(os.environ.get("EKA2L1_DATA", xdg / "EKA2L1/data"))).resolve()
    args.browser = args.browser.resolve(); args.reports = args.reports.resolve()
    if not os.environ.get("EKA2L1") or not os.environ.get("DISPLAY"):
        parser.error("source the existing EKA/XDG environment")
    if not args.browser.is_file() or not args.data.is_dir() or not args.reports.is_dir():
        parser.error("browser, installed data and report directory must exist")
    (work / "runs").mkdir(parents=True, exist_ok=True)
    if args.output:
        output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    else:
        output = Path(tempfile.mkdtemp(prefix="idb-quota-", dir=work / "runs"))
    token = args.run or uuid.uuid4().hex
    if not re.fullmatch(r"[A-Za-z0-9_-]{8,64}", token) or (args.run and args.phase != "cleanup"):
        parser.error("--run is a valid 8..64 character token for cleanup only")
    fixtures = Path(__file__).resolve().parent
    evidence_files = list(fixtures.glob("*.html")) + list(fixtures.glob("*.js")) + list(Path(__file__).resolve().parent.glob("*.py"))
    result = {"status": "FAIL", "suite": "idb-quota", "run": token, "phases": [], "origin": args.origin,
              "started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "browser_sha256": hashlib.sha256(args.browser.read_bytes()).hexdigest(),
              "emulator_sha256": hashlib.sha256(Path(os.environ["EKA2L1"]).read_bytes()).hexdigest(),
              "fixtures_sha256": {str(path.relative_to(fixtures.parent)): hashlib.sha256(path.read_bytes()).hexdigest() for path in evidence_files}}
    try:
        with network.exclusive_lock(args.data / ".jsc-run.lock"), network.exclusive_lock(args.data / ".browser-run.lock") as lock:
            for phase in (["cleanup"] if args.phase == "cleanup" else ["write", "read", "erased"]):
                result["phases"].append(run_phase(args, phase, token, output, work, lock))
        result["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "Interrupted"
    finally:
        result["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(result["status"] + ": " + str(output), flush=True)
    if result["status"] != "PASS": print(result.get("error"), flush=True)
    return result["status"] != "PASS"


if __name__ == "__main__":
    signal.signal(signal.SIGINT, network.interrupted)
    signal.signal(signal.SIGTERM, network.interrupted)
    raise SystemExit(main())

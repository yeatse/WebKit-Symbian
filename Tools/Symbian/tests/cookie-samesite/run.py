#!/usr/bin/env python3
"""Validate SameSite policy across two real browser lifetimes with isolated trust."""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import signal
import ssl
import struct
import subprocess
import time
import urllib.request
from urllib.parse import urlencode, urlsplit
import uuid

ROOT = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("cookie_runtime_support", ROOT.parent / "run-browser-network-tests.py")
network = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(network)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tls_preflight(ca, origin, other_port, http_port):
    parsed = urlsplit(origin)
    if parsed.scheme != "https" or parsed.hostname != "localhost" or parsed.path or parsed.query or parsed.fragment:
        raise ValueError("origin must be https://localhost:PORT without a path")
    token = "preflight_" + uuid.uuid4().hex
    context = ssl.create_default_context(cafile=str(ca))
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), urllib.request.HTTPSHandler(context=context))
    urls = [origin, f"https://127.0.0.1:{parsed.port or 443}",
            f"https://localhost:{other_port}", f"http://localhost:{http_port}"]
    evidence = []
    for base in urls:
        url = base + "/summary?" + urlencode({"run": token})
        with opener.open(url, timeout=8) as response:
            if response.status != 200 or response.url != url or json.load(response) != []:
                raise RuntimeError("Unexpected fixture preflight response: " + base)
        evidence.append({"origin": base, "status": "PASS", "certificate_and_hostname_verified": base.startswith("https:")})
    return evidence


def snapshot_database(data, output, label):
    # Copies are private run artifacts. Never replace the live database between
    # phases: doing so would invalidate the persistence test.
    source = data / "drives/c/Data/WebKit/cookies.db"
    result = {"exists": source.exists(), "restored_or_deleted": False}
    if source.exists():
        target = output / (label + "-cookies.db")
        shutil.copy2(source, target)
        result.update(bytes=source.stat().st_size, sha256=digest(source), snapshot=target.name)
    return result


def run_phase(args, phase, token, output, work, lock, validate):
    folder = output / phase
    folder.mkdir()
    total = 23 if phase == "initial" else 28
    wait = args.timeout if phase == "initial" else args.resume_timeout
    entry_path = "/start" if phase == "initial" else "/resume"
    url = args.origin + entry_path + "?" + urlencode({"run": token})
    final_url = args.origin + "/done?" + urlencode({"run": token, "phase": "nav" if phase == "initial" else "resume-b"})
    marker = f"PASS: cookie-samesite {total}/{total} ({total} outcomes)"
    environment = os.environ.copy()
    environment.update(WAIT=str(wait), EKA2L1_EXPECT_PASS=marker,
                       EKA2L1_BROWSER_LOCK_FD=str(lock.fileno()),
                       SYMBIAN_WEBKIT_WORK=str(work), EKA2L1_DATA=str(args.data))
    environment.pop("SCREENSHOT", None)
    previous = {p.resolve() for p in (work / "runs").glob("browser-*")}
    launcher = folder / "launcher.log"
    evidence = {"phase": phase, "url": url, "status": "FAIL"}
    process = None
    descendants = {}
    expired = False
    with network.adopt_orphaned_children() as excluded:
        try:
            with launcher.open("w") as stream:
                process = subprocess.Popen([str(network.BROWSER_RUNNER), str(args.browser), url],
                    env=environment, stdout=stream, stderr=subprocess.STDOUT,
                    start_new_session=True, pass_fds=(lock.fileno(),))
                deadline = time.monotonic() + wait + 45
                while process.poll() is None:
                    network.collect_descendants(descendants, excluded)
                    if time.monotonic() >= deadline:
                        expired = True
                        break
                    time.sleep(.1)
        finally:
            if process is not None:
                network.stop_process_tree(process, descendants, excluded)
    evidence["driver_exit"] = process.returncode
    report_path = args.reports / (token + ".json")
    if report_path.exists():
        shutil.copy2(report_path, folder / "report.json")
    matches = re.findall(r"^logs: (.+)$", network.read(launcher), re.M)
    if len(matches) == 1:
        run = Path(matches[0]).resolve()
        if run in previous or run.parent != (work / "runs").resolve() or not run.name.startswith("browser-"):
            raise RuntimeError("Driver reported stale or unrelated logs")
        evidence["source_run"] = str(run)
        for name in ("log.txt", "emulator.log", "browser.png", "curl.txt"):
            if (run / name).is_file():
                shutil.copy2(run / name, folder / name)
    (folder / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
    if expired or process.returncode != 0:
        raise RuntimeError(f"{phase}: driver failed ({process.returncode}), watchdog={expired}")
    if len(matches) != 1:
        raise RuntimeError("Driver did not identify exactly one fresh log directory")
    guest, host = network.read(folder / "log.txt"), network.read(folder / "emulator.log")
    if not guest or not host or network.PANIC.search(guest + "\n" + host):
        raise RuntimeError("Missing logs or guest panic/access violation")
    # WebCore attributes these messages to the external script, not the
    # document URL. Require both the exact script and its observed location.
    script_url = args.origin + "/case.js"
    console = re.findall(r"^console: " + re.escape(script_url) + r":\d+: (.*)$", guest, re.M)
    if (console.count(marker) != 1
            or console.count("COOKIE_FINAL_URL: " + final_url) != 1
            or "FAIL: cookie-samesite" in guest):
        raise RuntimeError("Missing unique final-page URL/success marker or explicit fixture failure")
    shot = folder / "browser.png"
    png = shot.read_bytes() if shot.is_file() else b""
    if len(png) < 33 or png[:8] != b"\x89PNG\r\n\x1a\n" or png[12:16] != b"IHDR" or not all(struct.unpack(">II", png[16:24])):
        raise RuntimeError("Missing or invalid browser screenshot")
    report = json.loads((folder / "report.json").read_text())
    checked = validate(report, token, phase, not_before_ns=args.not_before_ns)
    evidence.update(status="PASS", cumulative_checks=total, validator=checked,
                    console_script_url=script_url, final_document_url=final_url,
                    shutdown="observation completed and normal WM close with host exit 0")
    (folder / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("browser", type=Path)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--ca", type=Path, required=True)
    parser.add_argument("--tls", type=Path, help="Fixture server.crt/server.key directory; defaults beside CA")
    parser.add_argument("--output", type=Path, required=True, help="New evidence directory")
    parser.add_argument("--data", type=Path)
    parser.add_argument("--origin", default="https://localhost:8791")
    parser.add_argument("--other-port", type=int, default=8792)
    parser.add_argument("--http-port", type=int, default=8790)
    parser.add_argument("--timeout", type=network.positive_timeout, default=90)
    parser.add_argument("--resume-timeout", type=network.positive_timeout, default=30)
    parser.add_argument("--browser-sha256")
    parser.add_argument("--emulator-sha256")
    args = parser.parse_args()
    from validate import validate_report
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", network.SYMBIAN.parents[1] / "WebKitBuild/Symbian")).resolve()
    xdg = Path(os.environ.get("XDG_DATA_HOME", str(work / "eka2l1/xdg/data")))
    args.data = (args.data or Path(os.environ.get("EKA2L1_DATA", str(xdg / "EKA2L1/data")))).resolve()
    args.browser, args.reports, args.ca = args.browser.resolve(), args.reports.resolve(), args.ca.resolve()
    args.tls = (args.tls or args.ca.parent).resolve()
    host = Path(os.environ.get("EKA2L1", "")).resolve()
    if not os.environ.get("DISPLAY") or not os.environ.get("EKA2L1"):
        parser.error("Source the emulator environment and DISPLAY")
    if not all(p.is_file() for p in (args.browser, args.ca, host)) or not args.reports.is_dir() or not args.data.is_dir():
        parser.error("Missing browser, public CA, emulator, reports, or installed data")
    if not all((args.tls / name).is_file() for name in ("server.crt", "server.key")):
        parser.error("Missing fixture server certificate/private key")
    for path, expected in ((args.browser, args.browser_sha256), (host, args.emulator_sha256)):
        if expected and digest(path) != expected:
            parser.error("SHA-256 mismatch: " + str(path))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    (work / "runs").mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    result = {"status": "FAIL", "run": token, "phases": [],
              "browser_sha256": digest(args.browser), "emulator_sha256": digest(host),
              "test_ca_sha256": digest(args.ca), "started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "fixture_sha256": {p.name: digest(p) for p in ROOT.iterdir() if p.suffix in (".py", ".js", ".sh")}}
    server = None
    server_log = None
    try:
        with network.exclusive_lock(args.data / ".jsc-run.lock"), network.exclusive_lock(args.data / ".browser-run.lock") as lock:
            if (args.reports / (token + ".json")).exists():
                raise RuntimeError("Fresh token report already exists")
            registered = args.data / "drives/e/sys/bin/browser.exe"
            if registered.is_file() and digest(registered) != result["browser_sha256"]:
                raise RuntimeError("Registered E-drive image differs; stage the same candidate before this run")
            parsed_origin = urlsplit(args.origin)
            server_log = (output / "server.log").open("w")
            server = subprocess.Popen([os.sys.executable, str(ROOT / "server.py"), "--tls", str(args.tls),
                "--output", str(args.reports), "--https-port", str(parsed_origin.port or 443),
                "--other-port", str(args.other_port), "--http-port", str(args.http_port)],
                stdout=server_log, stderr=subprocess.STDOUT, start_new_session=True)
            ready_deadline = time.monotonic() + 10
            while "CHOOSE_NEW_RUN_ID" not in network.read(output / "server.log"):
                if server.poll() is not None or time.monotonic() >= ready_deadline:
                    raise RuntimeError("Fixture server did not become ready; see server.log")
                time.sleep(.1)
            result["preflight"] = tls_preflight(args.ca, args.origin, args.other_port, args.http_port)
            result["database_before"] = snapshot_database(args.data, output, "before")
            ca_target = args.data / "drives/c/resource/webkit/cacert.pem"
            result["release_ca_before_sha256"] = digest(ca_target)
            restoration = {"restored": False}
            try:
                with network.temporary_test_ca(ca_target, args.ca, restoration):
                    try:
                        args.not_before_ns = time.time_ns()
                        result["first_launch_bound_ns"] = args.not_before_ns
                        first, initial_report = run_phase(args, "initial", token, output, work, lock, validate_report)
                        result["phases"].append(first)
                        result["database_after_initial"] = snapshot_database(args.data, output, "initial")
                        second, resumed_report = run_phase(args, "resumed", token, output, work, lock, validate_report)
                        result["phases"].append(second)
                        if resumed_report["results"][:23] != initial_report["results"]:
                            raise RuntimeError("Initial outcomes changed or were reseeded during restart")
                        result["database_after_resume"] = snapshot_database(args.data, output, "resumed")
                    finally:
                        signal.signal(signal.SIGINT, signal.SIG_IGN)
                        signal.signal(signal.SIGTERM, signal.SIG_IGN)
            finally:
                result["test_ca_restored"] = restoration["restored"]
                result["release_ca_after_sha256"] = digest(ca_target)
            if not result["test_ca_restored"] or result["release_ca_after_sha256"] != result["release_ca_before_sha256"]:
                raise RuntimeError("Original CA bytes were not restored")
            if result["fixture_sha256"] != {p.name: digest(p) for p in ROOT.iterdir() if p.suffix in (".py", ".js", ".sh")}:
                raise RuntimeError("Fixture or runner changed during validation")
        result["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "Interrupted"
    finally:
        # A repeated cancellation cannot interrupt CA restoration or owned-server cleanup.
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        if server is not None:
            try:
                os.killpg(server.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(server.pid, signal.SIGKILL)
                server.wait()
            result["fixture_server_exit"] = server.returncode
        if server_log is not None:
            server_log.close()
        result["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(result["status"] + ": " + str(output), flush=True)
    if result["status"] != "PASS":
        print(result.get("error", "validation failed"), flush=True)
    return result["status"] != "PASS"


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, network.interrupted)
    signal.signal(signal.SIGINT, network.interrupted)
    raise SystemExit(main())

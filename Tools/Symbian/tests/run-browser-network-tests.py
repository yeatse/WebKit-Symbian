#!/usr/bin/env python3
"""Run the installed emulator's HTTP/TLS browser fixtures and verify fresh evidence."""
import argparse
from contextlib import contextmanager
import ctypes
import datetime
import fcntl
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import signal
import socket
import ssl
import stat
import subprocess
import tempfile
import time
from urllib.parse import urlencode
import uuid

SYMBIAN = Path(__file__).resolve().parents[1]
BROWSER_RUNNER = SYMBIAN / "eka2l1/run-browser.sh"
EXPECTED = {
    "network": (
        "fetch", "async XHR", "sync XHR", "chunked response", "data URI",
        "302 POST becomes GET", "303 POST becomes GET", "307 preserves POST",
        "401 async completes", "401 sync completes", "XHR cancellation", "fetch cancellation",
        "Cookie Path isolation", "redirect recalculates Cookie", "Cookie deletion",
        "cross-port Authorization removed", "cross-port Cookie stays host scoped",
        "WebSocket text", "WebSocket binary", "HTTP redirect to file rejected",
        "redirect to forbidden port rejected", "request works after failures", "Cookie cleanup readback",
    ),
    "tls": (
        "HTTPS trusted", "HTTPS wrong hostname", "HTTPS untrusted root",
        "WSS trusted", "WSS wrong hostname", "WSS untrusted root",
    ),
}
PANIC = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault", re.I)


def read(path):
    return path.read_text(errors="replace") if path.is_file() else ""


def probe_tls(port, hostname, trusted_certificate, expected_certificate):
    # Each context trusts only its explicit fixture certificate, never host roots.
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.load_verify_locations(cafile=str(trusted_certificate))
    expected = ssl.PEM_cert_to_DER_cert(expected_certificate.read_text())
    expected_sha256 = hashlib.sha256(expected).hexdigest()
    with socket.create_connection(("localhost", port), timeout=5) as connection:
        with context.wrap_socket(connection, server_hostname=hostname) as stream:
            actual_sha256 = hashlib.sha256(stream.getpeercert(binary_form=True)).hexdigest()
            if actual_sha256 != expected_sha256:
                raise RuntimeError(f"Port {port} served an unexpected certificate")
            stream.sendall(b"GET /ok HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            response = http.client.HTTPResponse(stream)
            response.begin()
            if response.status != 200 or response.read(3) != b"ok":
                raise RuntimeError(f"Port {port} did not serve the expected /ok fixture")
            return {"certificate_sha256": actual_sha256, "subject_alt_names": stream.getpeercert().get("subjectAltName", []),
                    "protocol": stream.version(), "http_status": response.status}


def check_tls_fixtures(ca, evidence, ports=None):
    """Prove reachability, certificate identity, and the specific negative causes."""
    ports = ports or {"good": 8767, "wrong": 8768, "untrusted": 8769}
    certificates = {name: ca.parent / f"{name}.crt" for name in ports}
    probes = (
        ("trusted endpoint", "good", "localhost", ca, ()),
        ("wrong-host endpoint identity", "wrong", "wrong.invalid", ca, ()),
        # Trust this leaf only in a disposable host context to prove its listener and identity.
        ("untrusted endpoint identity", "untrusted", "localhost", certificates["untrusted"], ()),
        ("hostname rejection", "wrong", "localhost", ca, (62,)),
        ("untrusted-root rejection", "untrusted", "localhost", ca, (18, 19, 20, 21)),
    )
    for name, endpoint, hostname, trust, expected_errors in probes:
        item = {"check": name, "host": "localhost", "port": ports[endpoint], "server_hostname": hostname,
                "certificate": str(certificates[endpoint]), "trust": str(trust), "status": "FAIL"}
        evidence.append(item)
        try:
            item.update(probe_tls(ports[endpoint], hostname, trust, certificates[endpoint]))
            if expected_errors:
                raise RuntimeError("Certificate verification unexpectedly succeeded")
        except ssl.SSLCertVerificationError as error:
            item.update({"error_type": type(error).__name__, "verify_code": error.verify_code,
                         "verify_message": error.verify_message})
            if error.verify_code not in expected_errors:
                raise RuntimeError(f"TLS preflight {name}: {error}") from error
        except Exception as error:
            item.update({"error_type": type(error).__name__, "error": str(error)})
            raise RuntimeError(f"TLS preflight {name}: {error}") from error
        item["status"] = "PASS"


@contextmanager
def exclusive_lock(path):
    with path.open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError(f"Another emulator run holds {path}") from error
        yield lock


@contextmanager
def temporary_test_ca(target, certificate, restoration=None):
    """Restore both the original bytes and the original absent-file state."""
    if target.is_symlink():
        raise RuntimeError("Refusing to alter a symlinked emulator CA bundle")
    root = certificate.read_bytes()
    ssl.create_default_context(cadata=root.decode("ascii"))
    if b"PRIVATE KEY" in root:
        raise RuntimeError("Expected a public test CA certificate")
    existed = target.exists()
    metadata = target.stat() if existed else None
    if metadata and (not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1):
        raise RuntimeError("Emulator CA bundle must be a private regular file, without hard-link aliases")
    original = target.read_bytes() if existed else None
    created_directories = []
    directory = target.parent
    while not directory.exists():
        created_directories.append(directory)
        directory = directory.parent
    try:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes((original or b"") + b"\n" + root + b"\n")
        yield
    finally:
        if existed:
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".restore-ca-", delete=False) as backup:
                temporary = Path(backup.name)
                try:
                    backup.write(original)
                    backup.flush()
                    os.fsync(backup.fileno())
                    os.fchmod(backup.fileno(), stat.S_IMODE(metadata.st_mode))
                    os.replace(temporary, target)
                finally:
                    temporary.unlink(missing_ok=True)
            if target.read_bytes() != original:
                raise RuntimeError("Emulator CA restoration did not preserve the original bytes")
            os.utime(target, ns=(metadata.st_atime_ns, metadata.st_mtime_ns))
        else:
            target.unlink(missing_ok=True)
        if restoration is not None:
            restoration["restored"] = True
        for directory in created_directories:
            try:
                directory.rmdir()
            except OSError:
                pass


def process_snapshot():
    """Track only our own descendants, including the emulator's separate session."""
    entries = {}
    for path in Path("/proc").glob("[0-9]*/stat"):
        try:
            fields = path.read_text().rsplit(")", 1)[1].split()
            entries[int(path.parent.name)] = (int(fields[1]), int(fields[2]), fields[19])
        except (OSError, ValueError, IndexError):
            pass
    return entries


@contextmanager
def adopt_orphaned_children():
    # run-browser.sh starts a separate emulator session. Adopt it if its driver dies.
    libc = ctypes.CDLL(None, use_errno=True)
    previous = ctypes.c_int()
    if libc.prctl(37, ctypes.byref(previous), 0, 0, 0) or libc.prctl(36, 1, 0, 0, 0):
        raise OSError(ctypes.get_errno(), "Cannot enable Linux child-subreaper cleanup")
    excluded = {pid for pid, (parent, _, _) in process_snapshot().items() if parent == os.getpid()}
    try:
        yield excluded
    finally:
        libc.prctl(36, previous.value, 0, 0, 0)


def collect_descendants(known, excluded=()):
    entries = process_snapshot()
    selected = {os.getpid()}
    selected.update(pid for pid, birth in known.items() if pid in entries and entries[pid][2] == birth)
    while True:
        children = {pid for pid, (parent, _, _) in entries.items() if parent in selected and pid not in excluded}
        if children <= selected:
            break
        selected.update(children)
    for pid in selected:
        if pid in entries and pid != os.getpid():
            known[pid] = entries[pid][2]


def stop_process_tree(process, known, excluded=()):
    collect_descendants(known, excluded)
    # The shell/Python driver receives TERM first so its own finally block runs.
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    # Allow the driver's 5-second WM helper, 10-second exit wait and fallback.
    deadline = time.monotonic() + 20
    while process.poll() is None and time.monotonic() < deadline:
        collect_descendants(known, excluded)
        time.sleep(0.05)
    collect_descendants(known, excluded)
    entries = process_snapshot()
    for pid, birth in known.items():
        if pid in entries and entries[pid][2] == birth:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    time.sleep(0.1)
    deadline = time.monotonic() + 5
    while True:
        # TERM handlers can spawn a new session before their process exits.
        collect_descendants(known, excluded)
        entries = process_snapshot()
        for pid, birth in known.items():
            if pid in entries and entries[pid][2] == birth:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        process.poll()
        for pid in known:
            if pid != process.pid:
                try:
                    os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    pass
        collect_descendants(known, excluded)
        entries = process_snapshot()
        remaining = [pid for pid, birth in known.items() if pid in entries and entries[pid][2] == birth]
        if not remaining and process.poll() is not None:
            return
        if time.monotonic() >= deadline:
            raise RuntimeError("Timed out cleaning this run's descendants: " + str(remaining))
        time.sleep(0.05)


def validate_report(report, suite, token):
    if report.get("suite") != suite or report.get("run") != token:
        raise ValueError("Report belongs to a different browser run")
    results = report.get("results")
    if not isinstance(results, list) or len(results) != len(EXPECTED[suite]):
        raise ValueError("Report has the wrong number of outcomes")
    names = [row.get("name") for row in results if isinstance(row, dict)]
    if len(names) != len(set(names)) or set(names) != set(EXPECTED[suite]):
        raise ValueError("Report has duplicate, missing, or unexpected test names")
    failures = [row for row in results if row.get("status") != "PASS"]
    if failures:
        raise ValueError("Guest failures: " + json.dumps(failures, ensure_ascii=False))


def find_report(directory, previous, suite, token):
    matches = []
    for path in directory.glob("*.json"):
        if path.name in previous:
            continue
        try:
            report = json.loads(path.read_text())
        except (OSError, ValueError):
            continue
        if isinstance(report, dict) and report.get("suite") == suite and report.get("run") == token:
            matches.append((path, report))
    if len(matches) != 1:
        raise RuntimeError(f"Expected one fresh {suite} report for {token}; found {len(matches)}")
    validate_report(matches[0][1], suite, token)
    return matches[0]


def run_suite(args, suite, output, work, lock):
    token = uuid.uuid4().hex
    total = len(EXPECTED[suite])
    marker = f"PASS: {suite} {total}/{total}"
    origin = getattr(args, "origins", {}).get(suite, "http://localhost:8765" if suite == "network" else "https://localhost:8767")
    query = {"autorun": 1, "run": token, **getattr(args, "queries", {}).get(suite, {})}
    url = f"{origin}/{suite}.html?" + urlencode(query)
    previous_reports = {path.name for path in args.reports.glob("*.json")}
    previous_runs = {path.resolve() for path in (work / "runs").glob("browser-*")}
    environment = os.environ.copy()
    environment.update({"WAIT": str(args.timeout), "EKA2L1_EXPECT_PASS": marker,
                        "EKA2L1_BROWSER_LOCK_FD": str(lock.fileno()),
                        "SYMBIAN_WEBKIT_WORK": str(work), "EKA2L1_DATA": str(args.data)})
    # Each observation must produce its own screenshot in the driver's run directory.
    environment.pop("SCREENSHOT", None)
    launcher = output / f"{suite}-launcher.log"
    process = None
    descendants = {}
    timed_out = False
    with adopt_orphaned_children() as excluded:
        try:
            with launcher.open("w") as stream:
                process = subprocess.Popen([str(BROWSER_RUNNER), str(args.browser), url],
                                           stdout=stream, stderr=subprocess.STDOUT, env=environment,
                                           start_new_session=True, pass_fds=(lock.fileno(),))
                # Include screenshot (15s), WM helper (5s), graceful exit (10s),
                # fallback termination (3s), and startup/collection overhead.
                deadline = time.monotonic() + args.timeout + 45
                while process.poll() is None:
                    collect_descendants(descendants, excluded)
                    if time.monotonic() >= deadline:
                        timed_out = True
                        break
                    time.sleep(0.2)
        finally:
            if process is not None:
                stop_process_tree(process, descendants, excluded)
    if timed_out:
        raise RuntimeError(f"{suite}: host watchdog expired; see {launcher}")
    if process.returncode:
        raise RuntimeError(f"{suite}: browser driver exited {process.returncode}; see {launcher}")
    match = re.search(r"^logs: (.+)$", read(launcher), re.M)
    if not match:
        raise RuntimeError(f"Browser driver did not identify this run's logs: {launcher}")
    run = Path(match.group(1)).resolve()
    if run in previous_runs or not run.is_relative_to((work / "runs").resolve()):
        raise RuntimeError("Browser driver reported a stale or unrelated log directory")
    guest = read(run / "log.txt")
    host = read(run / "emulator.log")
    if not guest or not host or PANIC.search(host + "\n" + guest):
        raise RuntimeError(f"Missing fresh logs or a guest panic/access violation: {run}")
    if marker not in guest or re.search(r"\bFAIL:", guest):
        raise RuntimeError(f"Missing success marker or guest failure: {run / 'log.txt'}")
    missing = [name for name in EXPECTED[suite]
               if not re.search(re.escape(f"PASS: {name}") + r"[ \t]*$", guest, re.M)]
    if missing:
        raise RuntimeError("Report is not corroborated by this run's console: " + ", ".join(missing))
    report_path, report = find_report(args.reports, previous_reports, suite, token)
    (output / f"{suite}-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    return {"suite": suite, "run": token, "passed": total, "logs": str(run),
            "report": str(report_path), "screenshot": str(run / "browser.png")}


def positive_timeout(value):
    number = float(value)
    if not 1 <= number <= 3600:
        raise argparse.ArgumentTypeError("Timeout must be between 1 and 3600 seconds")
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("browser", type=Path, help="Complete browser.exe with adjacent registration resources")
    parser.add_argument("--suite", choices=("network", "tls", "all"), default="all")
    parser.add_argument("--reports", type=Path, required=True, help="Existing local fixture server's report directory")
    parser.add_argument("--ca", type=Path, help="Public test ca.crt; required for TLS")
    parser.add_argument("--timeout", type=positive_timeout, default=180, help="Observation seconds per suite (default: 180)")
    parser.add_argument("--data", type=Path, help="Emulator data root; otherwise EKA2L1_DATA or XDG_DATA_HOME/EKA2L1/data")
    parser.add_argument("--output", type=Path, help="New result directory; must not exist")
    args = parser.parse_args(argv)
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", SYMBIAN.parents[1] / "WebKitBuild/Symbian")).resolve()
    if not args.data:
        xdg = Path(os.environ.get("XDG_DATA_HOME", work / "eka2l1/xdg/data"))
        args.data = Path(os.environ.get("EKA2L1_DATA", xdg / "EKA2L1/data"))
    args.data = args.data.resolve()
    args.browser = args.browser.resolve()
    args.reports = args.reports.resolve()
    suites = ("network", "tls") if args.suite == "all" else (args.suite,)
    if not os.environ.get("EKA2L1") or not os.environ.get("DISPLAY"):
        parser.error("Set EKA2L1 and DISPLAY; source the same XDG environment used to install firmware")
    if not args.browser.is_file() or not args.data.is_dir() or not args.reports.is_dir():
        parser.error("Browser image, installed emulator data, and fixture server report directory must exist")
    if "tls" in suites and (not args.ca or not args.ca.is_file()):
        parser.error("TLS requires --ca pointing to the running fixture server's public test CA")
    target = args.data / "drives/c/resource/webkit/cacert.pem"
    if not target.resolve().is_relative_to(args.data):
        parser.error("Emulator CA path resolves outside this emulator's data directory")
    output = args.output
    if output:
        output = output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        runs = work / "runs"
        runs.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="network-validation-", dir=runs))
    result = {"started": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "browser_sha256": hashlib.sha256(args.browser.read_bytes()).hexdigest(),
              "suites": [], "status": "FAIL"}
    try:
        with exclusive_lock(args.data / ".jsc-run.lock"), exclusive_lock(args.data / ".browser-run.lock") as lock:
            for suite in suites:
                if suite == "tls":
                    result["tls_preflight"] = []
                    check_tls_fixtures(args.ca, result["tls_preflight"])
                    restoration = {"restored": False}
                    try:
                        with temporary_test_ca(target, args.ca, restoration):
                            result["suites"].append(run_suite(args, suite, output, work, lock))
                    finally:
                        result["test_ca_restored"] = restoration["restored"]
                else:
                    result["suites"].append(run_suite(args, suite, output, work, lock))
        result["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "Interrupted"
    finally:
        result["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(f"{result['status']}: browser network validation; results: {output}", flush=True)
    if result["status"] != "PASS":
        print(result.get("error", "Validation failed"), flush=True)
        return 1
    return 0


def interrupted(signum, frame):
    # A second TERM must not interrupt process cleanup and CA restoration.
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    raise KeyboardInterrupt


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    raise SystemExit(main())

#!/usr/bin/env python3
"""Validate fresh browser graphics page outcomes and native EGL presentation evidence."""
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
import struct
import subprocess
import tempfile
import time
from urllib.parse import urlencode
import uuid

# Reuse the existing lock and bounded descendant-cleanup implementation without
# changing the network runner or installing a test CA for these local pages.
SPEC = importlib.util.spec_from_file_location("graphics_browser_support", Path(__file__).with_name("run-browser-network-tests.py"))
SUPPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPPORT)
SYMBIAN = Path(__file__).resolve().parents[1]
BROWSER_RUNNER = SYMBIAN / "eka2l1/run-browser.sh"
DRIVER_GRACE = 45
EXPECTED = {
    "compositing": (
        "translated layer geometry", "layer opacity", "overlapping layer hit test",
        "overflow clipping excludes transformed child", "remove compositing trigger",
        "ordinary root content update", "overflow scroller position",
        "layout resize retains layer geometry", "animation reached final transform",
        "multiple animation frames delivered",
    ),
    "webgl": (
        "context-and-capabilities", "new-buffer-is-cleared", "shader-texture-and-vertex-queries",
        "framebuffer-allocation-and-queries", "validation-and-invalid-shader",
        "resize-clears-and-retains-objects", "canvas-upload-flip-and-premultiply",
        "canvas-presentation-and-preservation", "context-loss-and-restoration",
    ),
}
MARKERS = {"compositing": "PASS: compositing-regression", "webgl": "WEBGL DONE PASS 9/9 passed; 0 failed"}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def ca_state(path):
    return {"exists": path.exists(), "sha256": sha256(path) if path.exists() else None}


def validate_evidence(suite, url, guest, host, screenshot):
    if not guest or not host or SUPPORT.PANIC.search(guest + "\n" + host):
        raise RuntimeError("Missing logs or guest panic/access violation")
    # URL includes a per-run nonce, so a copied old console cannot satisfy a new run.
    messages = re.findall(r"^console: " + re.escape(url) + r":\d+: (.*)$", guest, re.M)
    prefix = "PASS: " if suite == "compositing" else "WEBGL PASS "
    names = [message[len(prefix):] for message in messages if message.startswith(prefix)]
    if suite == "compositing":
        names = [name for name in names if name != "compositing-regression"]
        terminal = [message for message in messages if message.startswith("RESULT: compositing-regression")]
        if len(terminal) != 1 or not re.fullmatch(r"RESULT: compositing-regression passed=10 failed=0 frames=\d+", terminal[0]):
            raise RuntimeError("Missing or conflicting compositing 10/10 terminal result")
        if int(terminal[0].rsplit("=", 1)[1]) < 2 or messages.count(MARKERS[suite]) != 1:
            raise RuntimeError("Missing compositing completion or animation frames")
    else:
        terminal = [message for message in messages if message.startswith("WEBGL DONE ")]
        if terminal != [MARKERS[suite]]:
            raise RuntimeError("Missing or conflicting WebGL 9/9 terminal result")
    if len(names) != len(EXPECTED[suite]) or set(names) != set(EXPECTED[suite]):
        raise RuntimeError("Duplicate, missing, or unexpected page checks")
    if any(message.startswith(("FAIL:", "WEBGL FAIL ")) for message in messages):
        raise RuntimeError("Page reported a failure")
    if messages.index(terminal[0]) < max(messages.index(prefix + name) for name in names):
        raise RuntimeError("Page terminal result preceded its checks")
    evidence = guest + "\n" + host
    initialized = re.search(r"Symbian compositor: native EGL/GLES2 initialized, renderer=([^\r\n]+)", evidence)
    presents = list(re.finditer(r"Symbian compositor: native EGL/GLES2 presented ([1-9]\d*)x([1-9]\d*)", evidence))
    if not initialized or not presents or not any(item.start() > initialized.start() for item in presents):
        raise RuntimeError("Page passed without native EGL initialization and successful presentation")
    if re.search(r"Symbian compositor:[^\r\n]*(?:failed|software painting)", evidence, re.I):
        raise RuntimeError("Compositor failure or software fallback during accelerated acceptance")
    data = screenshot.read_bytes() if screenshot.is_file() else b""
    if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise RuntimeError("Missing or invalid PNG screenshot")
    width, height = struct.unpack(">II", data[16:24])
    if not width or not height:
        raise RuntimeError("Empty screenshot dimensions")
    return {"page_checks": len(names), "page_terminal": terminal[0],
            "native_renderer": initialized.group(1), "native_presentations": len(presents),
            "presented_sizes": [[int(item[1]), int(item[2])] for item in presents],
            "screenshot_size": [width, height]}


def run_suite(args, suite, output, work, lock, evidence):
    token = uuid.uuid4().hex
    url = "file:///C:/Data/WebKit/" + suite + ".html?" + urlencode({"autorun": 1, "run": token})
    previous = {path.resolve() for path in (work / "runs").glob("browser-*")}
    directory = output / suite
    directory.mkdir()
    launcher = directory / "launcher.log"
    evidence.update({"suite": suite, "run": token, "url": url, "status": "FAIL", "launcher": str(launcher)})
    environment = os.environ.copy()
    environment.update({"WAIT": str(args.timeout), "EKA2L1_EXPECT_PASS": MARKERS[suite],
                        "EKA2L1_BROWSER_LOCK_FD": str(lock.fileno()),
                        "SYMBIAN_WEBKIT_WORK": str(work), "EKA2L1_DATA": str(args.data)})
    environment.pop("SCREENSHOT", None)
    process = None
    descendants = {}
    expired = False
    with SUPPORT.adopt_orphaned_children() as excluded:
        try:
            with launcher.open("w") as stream:
                process = subprocess.Popen([str(BROWSER_RUNNER), str(args.browser), url],
                                           stdout=stream, stderr=subprocess.STDOUT, env=environment,
                                           start_new_session=True, pass_fds=(lock.fileno(),))
                deadline = time.monotonic() + args.timeout + DRIVER_GRACE
                while process.poll() is None:
                    SUPPORT.collect_descendants(descendants, excluded)
                    if time.monotonic() >= deadline:
                        expired = True
                        break
                    time.sleep(0.1)
        finally:
            if process is not None:
                SUPPORT.stop_process_tree(process, descendants, excluded)
    evidence["driver_exit"] = process.returncode
    # Keep any fresh evidence even when the driver reports an error.
    matches = re.findall(r"^logs: (.+)$", SUPPORT.read(launcher), re.M)
    if len(matches) == 1:
        run = Path(matches[0]).resolve()
        if run in previous or run.parent != (work / "runs").resolve() or not run.name.startswith("browser-"):
            raise RuntimeError("Browser driver reported a stale or unrelated log directory")
        evidence["source_run"] = str(run)
        for name in ("log.txt", "emulator.log", "browser.png", "curl.txt"):
            source = run / name
            if source.is_file():
                shutil.copy2(source, directory / name)
    if expired:
        raise RuntimeError(f"{suite}: host watchdog expired")
    if process.returncode:
        raise RuntimeError(f"{suite}: browser driver exited {process.returncode}")
    if len(matches) != 1:
        raise RuntimeError("Browser driver did not identify one fresh log directory")
    evidence.update(validate_evidence(suite, url, SUPPORT.read(directory / "log.txt"),
                                     SUPPORT.read(directory / "emulator.log"), directory / "browser.png"))
    evidence.update({"status": "PASS", "logs": str(directory), "screenshot": str(directory / "browser.png"),
                     "shutdown": "browser driver completed observation and graceful host shutdown"})


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("browser", type=Path, help="browser.exe with adjacent registration resources")
    parser.add_argument("--suite", choices=("all", *EXPECTED), default="all")
    parser.add_argument("--timeout", type=SUPPORT.positive_timeout, default=60, help="Observation seconds per page (default: 60)")
    parser.add_argument("--data", type=Path, help="Installed EKA data; defaults to EKA2L1_DATA or XDG data")
    parser.add_argument("--output", type=Path, help="New evidence directory; must not exist")
    parser.add_argument("--browser-sha256", help="Optional required browser identity")
    parser.add_argument("--emulator-sha256", help="Optional required emulator identity")
    args = parser.parse_args(argv)
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", SYMBIAN.parents[1] / "WebKitBuild/Symbian")).resolve()
    xdg = Path(os.environ.get("XDG_DATA_HOME", work / "eka2l1/xdg/data"))
    args.data = (args.data or Path(os.environ.get("EKA2L1_DATA", xdg / "EKA2L1/data"))).resolve()
    args.browser = args.browser.resolve()
    emulator = Path(os.environ.get("EKA2L1", "")).resolve()
    if not os.environ.get("EKA2L1") or not os.environ.get("DISPLAY"):
        parser.error("Set EKA2L1 and DISPLAY, with the installed firmware's XDG environment")
    if not args.browser.is_file() or not emulator.is_file() or not args.data.is_dir():
        parser.error("Browser, emulator executable and installed emulator data must exist")
    for name in ("browser_reg.rsc", "browser_loc.rsc"):
        if not (args.browser.parent / name).is_file():
            parser.error("Missing browser resource: " + name)
    browser_hash, emulator_hash = sha256(args.browser), sha256(emulator)
    if args.browser_sha256 and browser_hash != args.browser_sha256.lower():
        parser.error("Browser SHA256 does not match")
    if args.emulator_sha256 and emulator_hash != args.emulator_sha256.lower():
        parser.error("Emulator SHA256 does not match")
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        (work / "runs").mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="graphics-validation-", dir=work / "runs"))
    result = {"started": datetime.datetime.now(datetime.timezone.utc).isoformat(), "status": "FAIL", "suites": [],
              "browser_sha256": browser_hash, "emulator_sha256": emulator_hash,
              "evidence_scope": "Page checks and native EGL presentation are separate requirements. EKA dispatch and screenshots do not prove a physical GPU or a particular JIT tier."}
    try:
        with SUPPORT.exclusive_lock(args.data / ".jsc-run.lock"), SUPPORT.exclusive_lock(args.data / ".browser-run.lock") as lock:
            ca = args.data / "drives/c/resource/webkit/cacert.pem"
            before = ca_state(ca)
            result["ca_before"] = before
            try:
                for suite in EXPECTED if args.suite == "all" else (args.suite,):
                    evidence = {}
                    result["suites"].append(evidence)
                    run_suite(args, suite, output, work, lock, evidence)
            finally:
                result["ca_after"] = ca_state(ca)
                result["ca_unchanged"] = result["ca_after"] == before
                if not result["ca_unchanged"]:
                    raise RuntimeError("CA bundle changed during local-page validation")
        result["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "Interrupted"
    finally:
        result["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(f"{result['status']}: browser graphics validation; results: {output}", flush=True)
    if result["status"] != "PASS":
        print(result.get("error", "Validation failed"), flush=True)
        return 1
    return 0


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, SUPPORT.interrupted)
    signal.signal(signal.SIGINT, SUPPORT.interrupted)
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run HTTP media with fresh logs/reports, strict cleanup and temporary guest trust.

This verifies the page contract. Pixel/PCM capture is additional required evidence.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import tempfile

SPEC = importlib.util.spec_from_file_location("network_runner", Path(__file__).with_name("run-browser-network-tests.py"))
network = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(network)
HTTP_CASES = (
    "format rejection", "WAV playback controls", "MP3 playback", "MP4 video controls",
    "natural end notification", "invalid media error", "HTTP redirect playback",
    "HTTP missing media error", "source replacement during open", "cancel pending open",
    "progressive playback before completion", "stalled download resumes playback",
    "tail index waits for complete file", "truncated HTTP media error",
)
TLS_CASES = ("HTTPS trusted playback", "HTTPS wrong hostname rejected", "HTTPS untrusted root rejected")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("browser", type=Path)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True, help="Directory containing fixture manifest.json")
    parser.add_argument("--ca", type=Path, help="Public test CA enables the three TLS cases")
    parser.add_argument("--timeout", type=network.positive_timeout, default=180)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", network.SYMBIAN.parents[1] / "WebKitBuild/Symbian")).resolve()
    xdg = Path(os.environ.get("XDG_DATA_HOME", work / "eka2l1/xdg/data"))
    args.data = (args.data or Path(os.environ.get("EKA2L1_DATA", xdg / "EKA2L1/data"))).resolve()
    args.browser = args.browser.resolve(); args.reports = args.reports.resolve()
    if not os.environ.get("EKA2L1") or not os.environ.get("DISPLAY"):
        parser.error("Source the installed emulator's XDG environment and set EKA2L1/DISPLAY")
    manifest = args.fixtures / "manifest.json"
    if not args.browser.is_file() or not args.data.is_dir() or not args.reports.is_dir() or not manifest.is_file():
        parser.error("Browser, emulator data, reports and fixture manifest must exist")
    if args.ca and not args.ca.is_file(): parser.error("--ca must name the public fixture CA")
    fixtures=json.loads(manifest.read_text())
    for name, expected in fixtures["files"].items():
        fixture=(args.fixtures / name).resolve()
        if not fixture.is_relative_to(args.fixtures.resolve()): parser.error("Fixture escapes its directory")
        data=fixture.read_bytes()
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            parser.error("Fixture does not match manifest: " + name)
    target = args.data / "drives/c/resource/webkit/cacert.pem"
    if not target.resolve().is_relative_to(args.data): parser.error("Guest CA resolves outside emulator data")
    if args.output:
        output=args.output.resolve(); output.mkdir(parents=True,exist_ok=False)
    else:
        (work / "runs").mkdir(parents=True, exist_ok=True)
        output=Path(tempfile.mkdtemp(prefix="media-validation-",dir=work / "runs"))
    network.EXPECTED["media"] = HTTP_CASES + (TLS_CASES if args.ca else ())
    args.origins={"media":"http://localhost:8775"}
    args.queries={"media":{"tls":1,"tlsPort":8776}} if args.ca else {}
    result={"status":"FAIL","started":datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "browser_sha256":hashlib.sha256(args.browser.read_bytes()).hexdigest(),
            "fixtures":fixtures,"presentation_verified":False}
    host=Path(os.environ["EKA2L1"])
    if host.is_file():result["emulator_sha256"]=hashlib.sha256(host.read_bytes()).hexdigest()
    try:
        with network.exclusive_lock(args.data / ".jsc-run.lock"), network.exclusive_lock(args.data / ".browser-run.lock") as lock:
            if args.ca:
                result["tls_preflight"]=[]
                network.check_tls_fixtures(args.ca,result["tls_preflight"],{"good":8776,"wrong":8777,"untrusted":8778})
                restoration={"restored":False}
                try:
                    with network.temporary_test_ca(target,args.ca,restoration):
                        result["suite"]=network.run_suite(args,"media",output,work,lock)
                finally:result["test_ca_restored"]=restoration["restored"]
            else:result["suite"]=network.run_suite(args,"media",output,work,lock)
        result["status"]="PASS"
    except (Exception,KeyboardInterrupt) as error:
        result["error"]=str(error) or "Interrupted"
    finally:
        result["finished"]=datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
    print(f"{result['status']}: browser media page checks; results: {output}",flush=True)
    if result["status"] != "PASS": print(result.get("error","Validation failed"),flush=True)
    return result["status"] != "PASS"


if __name__ == "__main__":
    signal.signal(signal.SIGTERM,network.interrupted)
    signal.signal(signal.SIGINT,network.interrupted)
    raise SystemExit(main())

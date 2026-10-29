#!/usr/bin/env python3
"""Require actual LLInt/Baseline/DFG execution on QEMU's ARM1136 CPU."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

TESTS = Path(__file__).resolve().parents[1] / "tests"
sys.path.insert(0, str(TESTS))
from arm11_jit_common import PROFILE_OPTIONS, compare_results, validate_output


def validate_elf(path):
    readelf = shutil.which("arm-linux-gnueabi-readelf") or shutil.which("readelf")
    if not readelf:
        raise ValueError("readelf is required to verify the ARMv6 softfp image")
    attributes = subprocess.check_output([readelf, "-h", "-A", str(path)], text=True, timeout=15)
    required = (r"Class:\s+ELF32", r"Machine:\s+ARM\s*$", r"Tag_CPU_arch:\s+v6\s*$",
                r"Tag_ARM_ISA_use:\s+Yes", r"Tag_FP_arch:\s+VFPv2", r"Flags:.*soft-float ABI")
    if any(not re.search(pattern, attributes, re.M) for pattern in required):
        raise ValueError("The executable is not an ELF32 ARMv6/VFPv2 softfp image")
    if "Tag_ABI_VFP_args: VFP registers" in attributes or "Tag_THUMB_ISA_use: Thumb-2" in attributes:
        raise ValueError("The executable requires hard-float arguments or Thumb-2")
    return attributes


def run_command(command, timeout, logfile):
    started = time.monotonic()
    process = None
    try:
        with logfile.open("w") as output:
            process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                status = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired as error:
                raise RuntimeError(f"Guest exceeded {timeout} seconds") from error
    finally:
        if process is not None:
            # QEMU user-mode guest threads share this private process group.
            if process.poll() is None:
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
    if status:
        raise RuntimeError(f"Guest exited {status}")
    return time.monotonic() - started


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jsc", type=Path)
    parser.add_argument("--qemu", default=os.environ.get("QEMU_ARM", "qemu-arm"))
    parser.add_argument("--sysroot", type=Path, default=os.environ.get("ARM11_LINUX_SYSROOT", "/usr/arm-linux-gnueabi"))
    parser.add_argument("--profile", action="append", choices=tuple(PROFILE_OPTIONS), help="Repeat to select profiles; default: all three")
    parser.add_argument("--timeout", type=float, default=180, help="Seconds per profile")
    parser.add_argument("--output", type=Path, help="New result directory")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout <= 3600:
        parser.error("Timeout must be between 1 and 3600 seconds")
    args.jsc = args.jsc.resolve()
    args.sysroot = args.sysroot.resolve()
    qemu = shutil.which(args.qemu)
    if not qemu or not args.jsc.is_file() or not (args.sysroot / "lib/ld-linux.so.3").is_file():
        parser.error("QEMU, jsc and the soft-float ARM Linux loader must exist")
    profiles = args.profile or list(PROFILE_OPTIONS)
    if len(set(profiles)) != len(profiles):
        parser.error("Each profile may only be selected once")
    work = Path(os.environ.get("SYMBIAN_WEBKIT_WORK", TESTS.parents[2] / "WebKitBuild/Symbian"))
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        (work / "qemu/runs").mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="arm11-jit-", dir=work / "qemu/runs"))
    script = TESTS / "arm11-jit.js"
    record = {"started": datetime.datetime.now(datetime.timezone.utc).isoformat(), "cpu": "arm1136",
              "abi": "ARM state, ARMv6, VFPv2, softfp", "jsc": str(args.jsc),
              "jsc_sha256": hashlib.sha256(args.jsc.read_bytes()).hexdigest(),
              "test_sha256": hashlib.sha256(script.read_bytes()).hexdigest(),
              "qemu": str(Path(qemu).resolve()), "qemu_sha256": hashlib.sha256(Path(qemu).read_bytes()).hexdigest(),
              "sysroot": str(args.sysroot), "profiles": [], "status": "FAIL"}
    try:
        (output / "elf-attributes.txt").write_text(validate_elf(args.jsc))
        for mode in profiles:
            command = [qemu, "-cpu", "arm1136", "-L", str(args.sysroot),
                       "-U", "LD_LIBRARY_PATH", "-U", "LD_PRELOAD", str(args.jsc),
                       *PROFILE_OPTIONS[mode], str(script), "--", mode]
            entry = {"mode": mode, "command": command, "log": str(output / (mode + ".log")), "status": "FAIL"}
            record["profiles"].append(entry)
            try:
                entry["seconds"] = run_command(command, args.timeout, Path(entry["log"]))
                entry["result"] = validate_output(Path(entry["log"]).read_text(errors="replace"), mode)
                entry["status"] = "PASS"
            except Exception as error:
                entry["error"] = str(error)
            print(f"{entry['status']}: {mode}; log: {entry['log']}", flush=True)
        if any(entry["status"] != "PASS" for entry in record["profiles"]):
            raise RuntimeError("At least one requested execution profile failed")
        compare_results([entry["result"] for entry in record["profiles"]])
        record["status"] = "PASS"
    except (Exception, KeyboardInterrupt) as error:
        record["error"] = str(error) or "Interrupted"
    finally:
        record["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        (output / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"{record['status']}: ARM11 execution validation; results: {output}", flush=True)
    return 0 if record["status"] == "PASS" else 1


def interrupted(signum, frame):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    raise KeyboardInterrupt


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    raise SystemExit(main())

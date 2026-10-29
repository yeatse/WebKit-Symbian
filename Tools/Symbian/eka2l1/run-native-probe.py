#!/usr/bin/env python3
"""Run a finite native probe with fresh evidence and strict guest/host exits."""
import argparse
import datetime
import fcntl
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


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def stop(process):
    if process is None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", choices=("egl", "angle", "mmf", "aligned-pressure", "native-volume", "native-clipboard"), required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--image-sha256", required=True)
    parser.add_argument("--emulator", type=Path, default=os.environ.get("EKA2L1"))
    parser.add_argument("--emulator-sha256", required=True)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--screenshot-marker")
    parser.add_argument("--final-screenshot-marker", help="also capture after a final event such as native surface removal")
    parser.add_argument("--screenshots", type=int, default=1)
    parser.add_argument("--capture-audio", action="store_true")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if not args.emulator or args.timeout <= 0 or args.screenshots < 1:
        parser.error("emulator, positive timeout and screenshot count are required")
    image, emulator = args.image.resolve(), args.emulator.resolve()
    for path, expected in ((image, args.image_sha256), (emulator, args.emulator_sha256)):
        if digest(path) != expected:
            parser.error("input SHA-256 mismatch: " + str(path))
    arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    log_name, probe_name, exe_name = {
        "egl": ("egl-probe.log", "egl-window-probe", "egl_probe.exe"),
        "angle": ("angle-probe.log", "angle-probe", "angle_probe.exe"),
        "mmf": ("mmf-probe.log", "native-mmf", "mmf_probe.exe"),
        "aligned-pressure": ("aligned-pressure.log", "aligned-pressure", "aligned_pressure.exe"),
        "native-volume": ("volume-probe.log", "native-volume", "native_volume.exe"),
        "native-clipboard": ("clipboard-probe.log", "native-clipboard", "clipboard_probe.exe"),
    }[args.probe]
    data = Path(os.environ.get("EKA2L1_DATA", str(Path(os.environ["XDG_DATA_HOME"]) / "EKA2L1/data")))
    locks = []
    for name in (".jsc-run.lock", ".browser-run.lock"):
        lock = (data / name).open("a")
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        locks.append(lock)
    args.reports.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    run = Path(tempfile.mkdtemp(prefix=args.probe + "-" + stamp, dir=args.reports))
    c = data / "drives/c"
    (c / "sys/bin").mkdir(parents=True, exist_ok=True)
    (c / "Data/WebKit").mkdir(parents=True, exist_ok=True)
    guest_log = c / "Data/WebKit" / log_name
    if guest_log.exists():
        guest_log.rename(run / ("previous-" + log_name))
    shutil.copy2(image, c / "sys/bin" / exe_name)
    argument_string = " " + " ".join('"' + arg.replace("\\", "\\\\").replace('"', '\\"') + '"' for arg in arguments)
    if len(argument_string.encode()) >= 4095:
        parser.error("guest command line exceeds 4096 bytes")
    command = [str(emulator), "--device", os.environ.get("EKA2L1_DEVICE", "RM-707"), "--run", "C:\\sys\\bin\\" + exe_name, argument_string]
    # These host-side replacement DLLs determine MMF behavior independently
    # of the emulator executable. Do not include firmware files or host paths.
    patch_assets = {}
    for name in ("mediaclientaudio_general.dll", "mediaclientaudio.dll.map", "mediaclientvideo_v100.dll", "mediaclientvideo.dll.map"):
        asset = emulator.parent / "patch" / name
        if asset.is_file():
            patch_assets[name] = {"sha256": digest(asset), "bytes": asset.stat().st_size}
    host_log = run / "emulator.log"
    panic = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC", re.I)
    process = recorder = None
    failure = None
    captured = []
    final_screenshot = None
    capture_timing = {}
    next_shot = 0
    deadline = time.monotonic() + args.timeout

    def interrupted(signum, frame):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        raise KeyboardInterrupt

    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    try:
        with host_log.open("w") as output, (run / "audio.raw").open("wb") as pcm, (run / "audio-capture.log").open("w") as audio_log:
            if args.capture_audio:
                capture_timing["recorder_start_monotonic"] = time.monotonic()
                recorder = subprocess.Popen(["parec", "--device=symbian_media.monitor", "--format=s16le", "--rate=48000", "--channels=2", "--latency-msec=50", "--raw"], stdout=pcm, stderr=audio_log, start_new_session=True)
                ready_deadline = min(deadline, time.monotonic() + 3)
                while True:
                    if recorder.poll() is not None:
                        raise OSError("Pulse recorder exited before monitor readiness")
                    ready_bytes = (run / "audio.raw").stat().st_size
                    if ready_bytes >= 19200 and ready_bytes % 4 == 0:
                        capture_timing["first_pcm_ready_monotonic"] = time.monotonic()
                        capture_timing["ready_bytes"] = ready_bytes
                        break
                    if time.monotonic() >= ready_deadline:
                        raise OSError("Pulse monitor did not supply 100 ms of complete PCM within startup deadline")
                    time.sleep(.02)
            capture_timing["emulator_launch_monotonic"] = time.monotonic()
            process = subprocess.Popen(command, cwd=emulator.parent, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
            while process.poll() is None:
                now = time.monotonic()
                if panic.search(read(host_log)):
                    failure = "guest panic or access violation"
                    break
                if recorder is not None and recorder.poll() is not None:
                    failure = "Pulse recorder exited before the guest"
                    break
                if now >= deadline:
                    failure = "host deadline expired"
                    break
                if args.screenshot_marker and len(captured) < args.screenshots and now >= next_shot and args.screenshot_marker in read(guest_log):
                    shot = run / ("screenshot-%d.png" % (len(captured) + 1))
                    subprocess.run(["import", "-window", "root", str(shot)], check=True, timeout=min(10, max(.1, deadline - now)))
                    if not shot.is_file() or not shot.stat().st_size:
                        raise OSError("screenshot command produced no image")
                    captured.append(shot.name)
                    next_shot = time.monotonic() + 2
                if args.final_screenshot_marker and final_screenshot is None and args.final_screenshot_marker in read(guest_log):
                    shot = run / "screenshot-final.png"
                    subprocess.run(["import", "-window", "root", str(shot)], check=True, timeout=min(10, max(.1, deadline - time.monotonic())))
                    if not shot.is_file() or not shot.stat().st_size:
                        raise OSError("final screenshot command produced no image")
                    final_screenshot = shot.name
                time.sleep(.1)
            if failure is None and process.returncode != 0:
                failure = "emulator exited with status " + str(process.returncode)
            if recorder is not None and recorder.poll() is not None:
                failure = failure or "Pulse recorder exited before capture cleanup"
            if recorder is not None and failure is None:
                # Observe Stop after the guest has exited as well. Collect real
                # monitor samples rather than assuming host teardown took long
                # enough to include silence at the end of the recording.
                capture_timing["host_exit_observed_monotonic"] = time.monotonic()
                tail_start = (run / "audio.raw").stat().st_size
                capture_timing["tail_start_bytes"] = tail_start
                tail_deadline = min(deadline, time.monotonic() + 1)
                while True:
                    if recorder.poll() is not None:
                        raise OSError("Pulse recorder exited before post-guest PCM completed")
                    tail_bytes = (run / "audio.raw").stat().st_size - tail_start
                    if tail_bytes >= 38400 and tail_bytes % 4 == 0:
                        capture_timing["tail_ready_monotonic"] = time.monotonic()
                        capture_timing["tail_bytes"] = tail_bytes
                        break
                    if time.monotonic() >= tail_deadline:
                        raise OSError("Pulse monitor did not supply 200 ms of post-guest PCM within deadline")
                    time.sleep(.02)
    except KeyboardInterrupt:
        failure = "run interrupted"
    except (OSError, subprocess.SubprocessError) as error:
        failure = "host operation failed: " + str(error)
    finally:
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        stop(process)
        stop(recorder)
        capture_timing["capture_end_monotonic"] = time.monotonic()
        content = read(guest_log)
        (run / log_name).write_text(content)
    parsed = None
    exits = re.findall(r"(?:^| )" + re.escape(probe_name) + r" exiting ([-+]?\d+)$", content, re.M)
    try:
        results = re.findall(r"(?:^| )RESULT (\{[^\n]+\})", content, re.M)
        if len(results) != 1:
            raise ValueError("expected exactly one RESULT record")
        parsed = json.loads(results[0])
        if parsed.get("probe") != probe_name or parsed.get("failed") != 0 or parsed.get("passed", 0) <= 0:
            raise ValueError("probe RESULT reports failure or no checks")
        if args.probe == "native-volume" and (parsed.get("passed") != 10 or "NATIVE_VOLUME_RESULT 10/10 PASS" not in content):
            raise ValueError("native volume did not complete all ten checks")
        if args.probe == "native-clipboard":
            modes = {"inspect": 2, "write": 3, "read": 3, "empty-write": 3, "empty-read": 3}
            if (len(arguments) != 1 or arguments[0] not in modes
                    or parsed.get("mode") != arguments[0]
                    or parsed.get("passed") != modes[arguments[0]]
                    or parsed.get("payloadUTF16Units") != 20):
                raise ValueError("native clipboard arguments or check count mismatch")
        if exits != ["0"]:
            raise ValueError("expected exactly one successful guest exit marker")
        if not re.search(r"(?:^| )PASS(?::)? " + re.escape(probe_name), content, re.M):
            raise ValueError("missing probe PASS summary")
        if re.search(r"(?:^| )FAIL[: ]|GL ERROR|EGL ERROR", content, re.M):
            raise ValueError("guest log contains a failure")
        if args.probe == "egl" and (parsed.get("swaps"), parsed.get("pixelChecks")) != (3, 9):
            raise ValueError("EGL pixel/presentation checks incomplete")
        if args.probe == "egl":
            hold = int(arguments[arguments.index("--hold-ms") + 1]) if "--hold-ms" in arguments else 10000
            if parsed.get("holdMilliseconds") != hold or parsed.get("expectES3Rejection") is not ("--expect-es3-rejection" in arguments):
                raise ValueError("EGL effective guest arguments do not match the request")
        if args.screenshot_marker and len(captured) != args.screenshots:
            raise ValueError("requested observation screenshots missing")
        if args.final_screenshot_marker and final_screenshot is None:
            raise ValueError("requested final-event screenshot missing")
        if panic.search(read(host_log)):
            raise ValueError("guest panic or access violation")
        if args.capture_audio and (not (run / "audio.raw").stat().st_size or (run / "audio.raw").stat().st_size % 4):
            raise ValueError("recorded audio is empty or not complete stereo s16le frames")
    except (ValueError, TypeError) as error:
        failure = failure or str(error)
    report = {"status": "FAIL" if failure else "PASS", "failure": failure,
              "probe": args.probe, "image_sha256": args.image_sha256, "emulator_sha256": args.emulator_sha256,
              "emulator_patch_assets": patch_assets,
              "arguments": arguments, "host_exit": process.returncode if process else None,
              "host_pid": process.pid if process else None,
              "guest_exit": int(exits[0]) if len(exits) == 1 else None,
              "panic_or_access_violation": bool(panic.search(read(host_log))), "result": parsed,
              "guest_log": log_name, "screenshots": captured, "audio": "audio.raw" if args.capture_audio else None,
              "final_screenshot": final_screenshot,
              "audio_format": "s16le/48000Hz/stereo" if args.capture_audio else None,
              "audio_capture_timing": capture_timing if args.capture_audio else None}
    (run / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(content)
    print("Evidence: " + str(run))
    if failure:
        sys.exit("FAIL: " + failure)
    print("PASS: native probe checks and guest/host exits verified")


if __name__ == "__main__":
    main()

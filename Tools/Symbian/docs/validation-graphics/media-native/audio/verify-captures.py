#!/usr/bin/env python3
"""Decode committed captures and reproduce the recorded PCM checks."""

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    root = Path(__file__).resolve().parent
    manifest = json.loads((root / "manifest.json").read_text())
    analyzer = root / "analyze-native-audio.py"
    if sha256(analyzer.read_bytes()) != manifest["analysis_script_sha256"]:
        raise ValueError("Analysis script differs from the recorded version")
    results = []
    for capture in manifest["captures"]:
        source = root / capture["directory"]
        for name, expected in capture["files"].items():
            if sha256((source / name).read_bytes()) != expected:
                raise ValueError("Changed evidence: " + str(source / name))
        if "lossless_audio" not in capture:
            results.append({"capture": capture["directory"],
                            "verification": "metadata hashes only; original PCM is external"})
            continue
        decoded = subprocess.run(
            ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error",
             "-i", str(source / "audio.flac"), "-f", "s16le",
             "-c:a", "pcm_s16le", "-ar", "48000", "-ac", "2", "-"],
            check=True, capture_output=True).stdout
        if len(decoded) != capture["raw_bytes"] or sha256(decoded) != capture["raw_sha256"]:
            raise ValueError("Decoded capture differs from the original PCM")
        with tempfile.TemporaryDirectory(prefix="symbian-pcm-check-") as work:
            destination = Path(work)
            (destination / "audio.raw").write_bytes(decoded)
            for name in ("result.json", "mmf-probe.log"):
                shutil.copyfile(source / name, destination / name)
            subprocess.run([sys.executable, str(analyzer), str(destination),
                            "--frequency", str(capture["frequency_hz"])],
                           check=True, capture_output=True, text=True)
            actual = json.loads((destination / "audio-analysis.json").read_text())
        expected = json.loads((source / "audio-analysis.json").read_text())
        for key in ("status", "checks", "pcm", "audible_intervals_s",
                    "exact_zero_spans_s", "unexpected_silent_spans_over_1ms"):
            if actual[key] != expected[key]:
                raise ValueError("Analysis changed for " + capture["directory"] + ": " + key)
        results.append({"capture": capture["directory"],
                        "verification": "decoded PCM SHA and analysis match",
                        "pcm_status": actual["status"]})
    print(json.dumps({"status": "PASS", "captures": results}, indent=2))


if __name__ == "__main__":
    main()

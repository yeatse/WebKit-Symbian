#!/usr/bin/env python3
"""Reproduce archived audio observations, including recorded failures."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parent
    directories = [Path(value).resolve() for value in sys.argv[1:]] or sorted(
        directory for directory in root.glob("candidate-*")
        if (directory / "audio.flac").is_file())
    for source in directories:
        proof = json.loads((source / "audio-flac-verification.json").read_text())
        encoded = source / "audio.flac"
        assert hashlib.sha256(encoded.read_bytes()).hexdigest() == proof["flac_sha256"]
        with tempfile.TemporaryDirectory(prefix="browser-pcm-replay-") as temporary:
            work = Path(temporary)
            raw = work / "audio.raw"
            subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-i", str(encoded),
                            "-f", "s16le", "-acodec", "pcm_s16le", str(raw)], check=True)
            assert raw.stat().st_size == proof["pcm_bytes"]
            assert hashlib.sha256(raw.read_bytes()).hexdigest() == proof["pcm_sha256"]
            shutil.copyfile(source / "observation.json", work / "observation.json")
            for script in ["analyze-browser-control-audio.py", "archive-browser-audio.py"]:
                subprocess.run([sys.executable, str(root / script), str(work)],
                               stdout=subprocess.PIPE, check=True, text=True)
            for name in ["audio-control-analysis.json", "audio-mp3-observation.json"]:
                expected = json.loads((source / name).read_text())
                actual = json.loads((work / name).read_text())
                assert actual == expected, f"Observation changed: {source.name}/{name}"
        evidence = json.loads((source / "evidence.json").read_text())
        print(json.dumps({"archive": source.name, "reproduced": True,
                          "recorded_acceptance": evidence["acceptance"]}))


if __name__ == "__main__":
    main()

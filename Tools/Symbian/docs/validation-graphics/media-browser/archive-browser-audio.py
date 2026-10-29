#!/usr/bin/env python3
"""Observe the ordered MP3 tone and preserve complete PCM as lossless FLAC."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    raw_path = run / "audio.raw"
    raw = raw_path.read_bytes()
    observation = json.loads((run / "observation.json").read_text())
    analysis = json.loads((run / "audio-control-analysis.json").read_text())
    raw_hash = hashlib.sha256(raw).hexdigest()
    assert raw_hash == observation["audio"]["sha256"]
    assert observation["audio"]["format"] == "s16le/48000Hz/stereo"
    assert len(raw) % 4 == 0
    pcm = np.frombuffer(raw, dtype="<i2").reshape(-1, 2)
    samples = pcm.astype(np.float64)
    block = samples[:len(samples) // 480 * 480].reshape(-1, 480, 2)
    active = np.sqrt(np.mean(block * block, axis=(1, 2))) > 100
    edges = np.diff(np.r_[False, active, False].astype(np.int8))
    lower = next(row["observation_window_s"][1] for row in analysis["observations"]
                 if row["label"] == "audio.wav unmuted")
    upper = analysis["initial_playback_pcm_context"]["video.mp4"]["between_long_silences_s"][0]
    ranges = [(a / 100, b / 100) for a, b in zip(np.flatnonzero(edges == 1), np.flatnonzero(edges == -1))
              if b - a >= 60 and a / 100 > lower and b / 100 < upper]
    assert len(ranges) == 1, ranges
    begin, end = ranges[0]
    windows = []
    for step in range(math.ceil((begin + .02) * 4), math.floor((end - .02) * 4)):
        a, b = step * 12000, (step + 1) * 12000
        x = samples[a:b]
        power = np.abs(np.fft.rfft(x * np.hanning(len(x))[:, None], axis=0)) ** 2
        frequency = np.fft.rfftfreq(len(x), 1 / 48000)
        fraction = power[np.abs(frequency - 660) <= 8].sum(axis=0) / power.sum(axis=0)
        windows.append({"start_s": a / 48000, "end_s": b / 48000,
                        "dominant_hz": frequency[power.argmax(axis=0)].tolist(),
                        "expected_band_power_fraction": fraction.tolist(),
                        "rms_s16": np.sqrt(np.mean(x * x, axis=0)).tolist()})
    zeros = np.all(pcm[int((begin + .02) * 48000):int((end - .02) * 48000)] == 0, axis=1)
    transitions = np.diff(np.r_[False, zeros, False].astype(np.int8))
    longest_zero = max((b - a for a, b in zip(np.flatnonzero(transitions == 1),
                                             np.flatnonzero(transitions == -1))), default=0)
    checks = {"at_least_two_windows": len(windows) >= 2,
              "660hz_per_channel": all(min(w["expected_band_power_fraction"]) >= .95
                                       and min(w["rms_s16"]) > 100
                                       and all(abs(f - 660) <= 2 for f in w["dominant_hz"])
                                       for w in windows),
              "no_interior_exact_zero_at_least_1ms": bool(longest_zero < 48)}
    report = {"status": "PASS" if all(checks.values()) else "FAIL",
              "scope": "First MP3 playback ordered between WAV controls and video controls; PCM observation only",
              "pcm_sha256": raw_hash, "audible_range_s": [begin, end],
              "expected_frequency_hz": 660, "windows": windows, "checks": checks,
              "longest_interior_zero_frames": int(longest_zero),
              "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (run / "audio-mp3-observation.json").write_text(json.dumps(report, indent=2) + "\n")

    temporary = run / ".audio-lossless.flac"
    destination = run / "audio.flac"
    encode = ["ffmpeg", "-v", "error", "-nostdin", "-n", "-f", "s16le", "-ar", "48000", "-ac", "2",
              "-i", str(raw_path), "-c:a", "flac", "-compression_level", "12", "-threads", "1", str(temporary)]
    subprocess.run(encode, check=True)
    decode = ["ffmpeg", "-v", "error", "-nostdin", "-i", str(temporary), "-f", "s16le", "-acodec", "pcm_s16le", "-"]
    process = subprocess.Popen(decode, stdout=subprocess.PIPE)
    decoded_hash = hashlib.sha256()
    size = 0
    for data in iter(lambda: process.stdout.read(1024 * 1024), b""):
        decoded_hash.update(data)
        size += len(data)
    assert process.wait() == 0
    assert size == len(raw) and decoded_hash.hexdigest() == raw_hash
    assert not destination.exists()
    temporary.rename(destination)
    record = {"status": "PASS", "scope": "Entire capture, no cropping/resampling/normalization",
              "pcm_bytes": len(raw), "pcm_sha256": raw_hash,
              "decoded_pcm_bytes": size, "decoded_pcm_sha256": decoded_hash.hexdigest(),
              "flac_bytes": destination.stat().st_size,
              "flac_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
              "encode_command": encode, "decode_command": decode,
              "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (run / "audio-flac-verification.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"mp3_status": report["status"], "mp3_range_s": ranges[0],
                      "mp3_window_count": len(windows), "flac_bytes": record["flac_bytes"],
                      "decoded_pcm_sha256": raw_hash}, indent=2))


if __name__ == "__main__":
    main()

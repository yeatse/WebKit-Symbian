#!/usr/bin/env python3
"""Verify known MMF tones and the audible play/pause/resume/stop sequence."""
import argparse
import hashlib
import json
import re
from pathlib import Path

import numpy as np


def spans(mask):
    edges = np.diff(np.r_[False, mask, False].astype(np.int8))
    return [(int(a), int(b)) for a, b in
            zip(np.flatnonzero(edges == 1), np.flatnonzero(edges == -1))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--frequency", type=float, required=True)
    args = parser.parse_args()
    raw = (args.run / "audio.raw").read_bytes()
    assert raw and len(raw) % 4 == 0
    samples = np.frombuffer(raw, dtype="<i2").reshape(-1, 2)
    x = samples.astype(np.float64)
    rate = 48000
    duration = len(x) / rate
    log = (args.run / "mmf-probe.log").read_text()
    runner = json.loads((args.run / "result.json").read_text())
    phases = {}
    for phase, text in (("initial", "PLAY call initial"),
                        ("pause", "PAUSE call"),
                        ("resume", "PLAY call resume"),
                        ("stop", "STOP call")):
        found = re.search(r"MMF t=(\d+) " + re.escape(text), log)
        if found:
            phases[phase] = int(found[1]) / 1000
    block = rate // 100
    blocks = x[:len(x) // block * block].reshape(-1, block, 2)
    rms10 = np.sqrt(np.mean(blocks * blocks, axis=(1, 2)))
    active = [(a / 100, b / 100) for a, b in spans(rms10 >= 100)
              if b - a >= 20]
    zeros = [(a / rate, b / rate) for a, b in spans(np.all(samples == 0, axis=1))
             if b - a >= rate // 1000]
    pause_candidates = [(a, b) for a, b in zeros
                        if len(active) >= 2 and b - a >= .3
                        and abs(active[0][1] - a) < .02
                        and abs(active[1][0] - b) < .02]
    pause = max(pause_candidates, key=lambda ab: ab[1] - ab[0], default=None)
    tail = next(((a, b) for a, b in zeros if b == duration), None)
    spectra = []
    for index in range(len(x) // 12000):
        start, end = index * .25, (index + 1) * .25
        v = x[index * 12000:(index + 1) * 12000]
        power = np.abs(np.fft.rfft(v * np.hanning(len(v))[:, None], axis=0)) ** 2
        frequencies = np.fft.rfftfreq(len(v), 1 / rate)
        total = power.sum(axis=0)
        band = power[np.abs(frequencies - args.frequency) <= 8].sum(axis=0)
        row = {"start_s": start, "end_s": end,
               "rms_s16": np.sqrt(np.mean(v * v, axis=0)).tolist(),
               "dominant_hz": [float(frequencies[p.argmax()]) if p.sum() else None
                               for p in power.T],
               "expected_band_power_fraction": np.divide(
                   band, total, out=np.zeros_like(total), where=total > 0).tolist()}
        spectra.append(row)

    def tonal_rows(start, end):
        rows = [s for s in spectra if s["start_s"] >= start + .02
                and s["end_s"] <= end - .02]
        ok = len(rows) >= 2 and all(
            min(s["rms_s16"]) > 100
            and min(s["expected_band_power_fraction"]) >= .95
            and all(f is not None and abs(f - args.frequency) <= 2
                    for f in s["dominant_hz"]) for s in rows)
        return ok, rows

    first_ok, first_rows = tonal_rows(*active[0]) if len(active) >= 2 else (False, [])
    resumed_ok, resumed_rows = tonal_rows(*active[-1]) if active else (False, [])
    pause_difference = None
    offsets = {}
    if pause and "pause" in phases and "resume" in phases:
        pause_difference = (pause[1] - pause[0]) - (phases["resume"] - phases["pause"])
        offsets["pause"] = phases["pause"] - pause[0]
        offsets["resume"] = phases["resume"] - pause[1]
    if tail and "stop" in phases:
        offsets["stop"] = phases["stop"] - tail[0]
    spread = max(offsets.values()) - min(offsets.values()) if len(offsets) == 3 else None
    checks = {
        "guest_and_host_controls_passed": runner.get("status") == "PASS",
        "initial_interval_has_expected_tone": first_ok,
        "final_audible_interval_has_expected_tone": resumed_ok,
        "pause_silence_matches_guest_within_100ms": pause_difference is not None and abs(pause_difference) <= .1,
        "pause_resume_stop_align_with_one_offset_within_100ms": spread is not None and spread <= .1,
        "stop_followed_by_at_least_100ms_silence": tail is not None and tail[1] - tail[0] >= .1,
        "no_clipping": bool(np.max(np.abs(x)) < 32760),
    }
    unexpected = [dict(start_s=a, end_s=b, duration_s=b-a) for a, b in zeros
                  if (a, b) != pause and (a, b) != tail
                  and active and a > active[0][0] and b < active[-1][1]]
    checks["no_unexpected_silence_over_1ms"] = not unexpected
    report = {
        "status": "PASS" if all(checks.values()) else "FAIL",
        "scope": "Known-frequency tone and audible control sequence; continuity observations are separate",
        "analysis_script": str(Path(__file__).resolve()),
        "analysis_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "numpy_version": np.__version__, "expected_frequency_hz": args.frequency,
        "pcm": {"format": "s16le", "rate_hz": rate, "channels": 2,
                "frames": len(x), "duration_s": duration,
                "sha256": hashlib.sha256(raw).hexdigest(),
                "peak_s16_per_channel": np.max(np.abs(x), axis=0).tolist(),
                "stereo_max_difference_s16": float(np.max(np.abs(x[:,0] - x[:,1])))},
        "guest_phase_times_s": phases, "audible_intervals_s": active,
        "exact_zero_spans_s": zeros,
        "pause_duration_difference_s": pause_difference,
        "fitted_guest_minus_pcm_offsets_s": offsets,
        "offset_spread_s": spread,
        "timing_limit": "PCM capture lacks a shared monotonic timestamp; offsets are fitted from the observed transitions, not independently measured latency",
        "checks": checks,
        "unexpected_silent_spans_over_1ms": unexpected,
        "continuity_without_unexpected_silence": not unexpected,
        "initial_tone_windows": len(first_rows), "resumed_tone_windows": len(resumed_rows),
        "spectral_method": "250ms Hann FFT per channel; 4Hz bins; expected +/-8Hz band; >=95% spectral power, dominant frequency +/-2Hz and RMS>100 s16 required inside both audible intervals",
        "windows": spectra,
    }
    (args.run / "audio-analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("status", "audible_intervals_s", "checks", "unexpected_silent_spans_over_1ms")}, indent=2))


if __name__ == "__main__":
    main()

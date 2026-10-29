#!/usr/bin/env python3
"""Diagnostic PCM observations for the browser's named CAPTURE intervals.

This does not certify the API suite or precisely synchronize guest time. The
capture-ready and capture-end observations only estimate the PCM clock origin.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def spans(mask):
    edges = np.diff(np.r_[False, mask, False].astype(np.int8))
    return list(zip(np.flatnonzero(edges == 1), np.flatnonzero(edges == -1)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    observation = json.loads((args.run / "observation.json").read_text())
    raw = (args.run / "audio.raw").read_bytes()
    assert raw and len(raw) % 4 == 0
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == observation["audio"]["sha256"]
    pcm = np.frombuffer(raw, dtype="<i2").reshape(-1, 2)
    samples = pcm.astype(np.float64)
    rate = 48000
    duration = len(samples) / rate
    timing = observation["audio"]["capture_timing"]
    origin = timing["first_pcm_ready_monotonic"] - timing["ready_bytes"] / (rate * 4)
    end_origin = timing["tail_ready_monotonic"] - duration
    zeros = [{"start_s": a / rate, "end_s": b / rate,
              "duration_ms": (b - a) * 1000 / rate}
             for a, b in spans(np.all(pcm == 0, axis=1)) if b - a >= rate / 1000]
    blocks = samples[:len(samples) // 480 * 480].reshape(-1, 480, 2)
    rms10 = np.sqrt(np.mean(blocks * blocks, axis=(1, 2)))
    active = [{"start_s": a / 100, "end_s": b / 100}
              for a, b in spans(rms10 > 100) if b - a >= 8]

    rows = []
    for event in observation["events"]:
        label = event["label"]
        expected = 440 if label.startswith("audio.wav ") else 880
        stage = label.rsplit(" ", 1)[1]
        marker = event["observed_monotonic"] - origin
        # A fixed guard avoids interpreting the first-observed console stamp as
        # an exact transition. This window is an observation, not a latency test.
        begin, end = marker + .1, marker + .6
        first, last = int(begin * rate), int(end * rate)
        assert 0 <= first < last <= len(samples)
        segment = samples[first:last]
        silence_expected = stage in ("paused", "muted")
        windows = []
        for offset in (0, 12000):
            x = segment[offset:offset + 12000]
            assert len(x) == 12000
            power = np.abs(np.fft.rfft(x * np.hanning(len(x))[:, None], axis=0)) ** 2
            frequency = np.fft.rfftfreq(len(x), 1 / rate)
            total = power.sum(axis=0)
            band = power[np.abs(frequency - expected) <= 8].sum(axis=0)
            windows.append({
                "start_s": first / rate + offset / rate,
                "rms_s16": np.sqrt(np.mean(x * x, axis=0)).tolist(),
                "dominant_hz": [float(frequency[p.argmax()]) if p.sum() else None for p in power.T],
                "expected_band_power_fraction": np.divide(band, total, out=np.zeros_like(total), where=total > 0).tolist(),
            })
        gaps = [span for span in zeros if span["start_s"] < end and span["end_s"] > begin]
        tone_ok = all(min(w["rms_s16"]) > 100
                      and min(w["expected_band_power_fraction"]) >= .95
                      and all(f is not None and abs(f - expected) <= 2 for f in w["dominant_hz"])
                      for w in windows)
        rows.append({
            "label": label, "expected_hz": expected,
            "marker_estimated_pcm_s": marker, "observation_window_s": [begin, end],
            "exact_zero": bool(np.all(segment == 0)),
            "expected_silence": silence_expected,
            "expected_tone_spectrum": tone_ok,
            "extra_exact_zero_spans_over_1ms": [] if silence_expected else gaps,
            "observation_passed": bool(np.all(segment == 0)) if silence_expected else tone_ok and not gaps,
            "rms_s16": np.sqrt(np.mean(segment * segment, axis=0)).tolist(),
            "windows": windows,
        })
    amplitude_ratios = {}
    initial_context = {}
    for name in ("audio.wav", "video.mp4"):
        initial = next(r for r in rows if r["label"] == name + " playing")
        final = next(r for r in rows if r["label"] == name + " unmuted")
        amplitude_ratios[name] = [b / a for a, b in zip(initial["rms_s16"], final["rms_s16"])]
        marker = initial["marker_estimated_pcm_s"]
        previous = [s for s in zeros if s["duration_ms"] >= 200 and s["end_s"] < marker]
        following = [s for s in zeros if s["duration_ms"] >= 200 and s["start_s"] > marker]
        if previous and following:
            begin, end = previous[-1]["end_s"], following[0]["start_s"]
            initial_context[name] = {
                "between_long_silences_s": [begin, end],
                "following_long_silence_s": [following[0]["start_s"], following[0]["end_s"]],
                "interior_short_silences": [dict(s, seconds_after_audible_start=s["start_s"] - begin,
                                                seconds_of_audio_after_gap=end - s["end_s"])
                                           for s in zeros if s["start_s"] > begin and s["end_s"] < end],
                "meaning": "PCM geometry independent of console transition timestamps; a short interior hole followed by more tone is distinct from the following sustained pause.",
            }
    report = {
        "status": "PARTIAL_WITH_FAILURES" if not all(r["observation_passed"] for r in rows) else "PARTIAL_OBSERVATIONS_ONLY",
        "whole_suite_status": observation["status"],
        "whole_suite_passed": observation["status"] == "PASS",
        "scope": "Only recorded named control windows; whole-suite status is copied from the runner, not established by this PCM diagnostic",
        "analysis_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "numpy_version": np.__version__,
        "pcm": {"sha256": digest, "format": "s16le", "rate_hz": rate, "channels": 2, "duration_s": duration,
                "peak_s16": np.max(np.abs(samples), axis=0).tolist(), "clipping": bool(np.max(np.abs(samples)) >= 32760)},
        "timing": {"estimated_origin_monotonic": origin, "end_estimated_origin_monotonic": end_origin,
                   "origin_difference_s": end_origin - origin,
                   "limit": "Host console timestamps are delayed observations; PCM origin is estimated from file delivery, not hardware timestamps. No exact guest latency or requested pause duration is claimed."},
        "spectral_method": "Two 250ms Hann windows per stage, 4Hz bins, expected +/-8Hz power >=95%, dominant +/-2Hz and RMS>100 per channel; active windows also reject exact-zero gaps >=1ms. Observation window is host CAPTURE estimate +100..600ms.",
        "observations": rows, "unmuted_to_initial_rms_ratio": amplitude_ratios,
        "initial_playback_pcm_context": initial_context,
        "all_exact_zero_spans_over_1ms": zeros, "audible_intervals_10ms_rms_gt100": active,
    }
    (args.run / "audio-control-analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"status": report["status"], "whole_suite_status": report["whole_suite_status"],
                      "observations": [{"label": r["label"], "observation_passed": r["observation_passed"],
                                        "extra_exact_zero_spans_over_1ms": r["extra_exact_zero_spans_over_1ms"]} for r in rows],
                      "unmuted_to_initial_rms_ratio": amplitude_ratios}, indent=2))


if __name__ == "__main__":
    main()

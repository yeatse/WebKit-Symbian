#!/usr/bin/env python3
"""Independent fixed-window PCM observations, never whole-run acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

import numpy as np

LABELS = ['click-start', 'native-running', 'click-suspend', 'native-suspended',
          'click-resume', 'native-resumed', 'click-close', 'quiet']
RATE = 48000
THRESHOLDS = {'window_after_marker_seconds': [.3, .8],
              'minimum_marker_to_next_click_seconds': 1.2,
              'maximum_pcm_origin_spread_seconds': .15,
              'running_minimum_rms_s16': 100, 'expected_hz': 997,
              'peak_frequency_tolerance_hz': 3, 'band_half_width_hz': 10,
              'minimum_expected_band_power_fraction': .95,
              'maximum_peak_exclusive_s16': 32760,
              'maximum_exact_zero_gap_seconds': .001,
              'silence_maximum_rms_s16': 2}


def require(value, reason):
    if not value:
        raise ValueError(reason)


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def longest_zero_gap(x):
    longest = 0
    for channel in x.T:
        edges = np.diff(np.r_[False, channel == 0, False].astype(np.int8))
        starts, ends = np.flatnonzero(edges == 1), np.flatnonzero(edges == -1)
        if len(starts):
            longest = max(longest, int(np.max(ends - starts)))
    return longest


def segment_metrics(x):
    require(x.ndim == 2 and x.shape[1] == 2 and len(x) >= RATE // 2 and np.isfinite(x).all(), 'invalid PCM window')
    z = x.astype(np.float64)
    power = np.abs(np.fft.rfft(z * np.hanning(len(z))[:, None], axis=0)) ** 2
    freq = np.fft.rfftfreq(len(z), 1 / RATE)
    total = power.sum(axis=0)
    band = power[np.abs(freq - THRESHOLDS['expected_hz']) <= THRESHOLDS['band_half_width_hz']].sum(axis=0)
    return {'frames': len(z), 'rms_s16': np.sqrt(np.mean(z * z, axis=0)).tolist(),
            'peak_s16': np.max(np.abs(z), axis=0).tolist(),
            'dominant_hz': [float(freq[p.argmax()]) if p.sum() else None for p in power.T],
            'expected_band_power_fraction': np.divide(band, total, out=np.zeros(2), where=total > 0).tolist(),
            'exact_zero': bool(np.all(z == 0)), 'maximum_exact_zero_gap_seconds': longest_zero_gap(z) / RATE}


def classify(x, running):
    metrics = segment_metrics(x)
    if running:
        passed = (min(metrics['rms_s16']) > THRESHOLDS['running_minimum_rms_s16']
                  and max(metrics['peak_s16']) < THRESHOLDS['maximum_peak_exclusive_s16']
                  and all(f is not None and abs(f - THRESHOLDS['expected_hz']) <= THRESHOLDS['peak_frequency_tolerance_hz'] for f in metrics['dominant_hz'])
                  and min(metrics['expected_band_power_fraction']) >= THRESHOLDS['minimum_expected_band_power_fraction']
                  and metrics['maximum_exact_zero_gap_seconds'] < THRESHOLDS['maximum_exact_zero_gap_seconds'])
    else:
        passed = max(metrics['rms_s16']) <= THRESHOLDS['silence_maximum_rms_s16']
    return {'observation_passed': bool(passed), 'expected': '997Hz-running' if running else 'quiet-rms<=2', **metrics}


def analyze(raw, capture, marks):
    require(capture.get('suite') == 'classic-webaudio-pcm' and capture.get('version') == 1, 'capture identity')
    require(capture.get('status') == 'CAPTURE_STOPPED_REQUIRES_ANALYSIS' and capture.get('execute_requested') is True, 'capture not normally stopped')
    require(capture.get('stop_reason') == 'root_signal' and capture.get('forced_cleanup') is False and capture.get('deadline_expired') is False, 'capture cleanup/deadline')
    require(capture.get('pacat_returncode') in (0, -2) and capture.get('received_stop_signal') in (2, 15), 'capture child exit')
    require(capture.get('server') == 'unix:/workspace/symbian-media-work/runtime/native' and capture.get('source') == 'symbian_media.monitor', 'capture source')
    require(capture.get('format') == 's16le' and capture.get('sample_rate') == RATE and capture.get('channels') == 2, 'capture format')
    require(len(raw) > 0 and len(raw) % 4 == 0 and len(raw) == capture.get('bytes') and hashlib.sha256(raw).hexdigest() == capture.get('sha256'), 'PCM identity/alignment')
    origin = capture.get('estimated_origin_range_monotonic')
    require(type(origin) is list and len(origin) == 2 and all(finite(t) for t in origin) and 0 <= origin[1] - origin[0] <= THRESHOLDS['maximum_pcm_origin_spread_seconds'], 'PCM timing uncertainty')
    require(marks.get('suite') == 'classic-webaudio-pcm' and marks.get('version') == 1 and re.fullmatch(r'wa-[0-9a-f]{32}', marks.get('run', '')), 'marks identity')
    rows = marks.get('marks')
    require(type(rows) is list and len(rows) == len(LABELS) and [r.get('label') for r in rows] == LABELS, 'marker order')
    times = [r.get('observed_monotonic') for r in rows]
    require(all(finite(t) for t in times) and all(a < b for a, b in zip(times, times[1:])), 'marker times')
    for a, b in [(1, 2), (3, 4), (5, 6)]:
        require(times[b] - times[a] >= THRESHOLDS['minimum_marker_to_next_click_seconds'], 'insufficient steady-state hold')
    require(finite(capture.get('stopped_monotonic')) and capture['stopped_monotonic'] - times[7] >= THRESHOLDS['minimum_marker_to_next_click_seconds'], 'insufficient final quiet hold')
    pcm = np.frombuffer(raw, dtype='<i2').reshape(-1, 2)
    block = pcm[:len(pcm) // 480 * 480].astype(np.float64).reshape(-1, 480, 2)
    activity = np.min(np.sqrt(np.mean(block * block, axis=1)), axis=1) > 100
    edges = np.diff(np.r_[False, activity, False].astype(np.int8))
    activity_intervals = [{'start_seconds': int(lo) / 100, 'end_seconds': int(hi) / 100}
                          for lo, hi in zip(np.flatnonzero(edges == 1), np.flatnonzero(edges == -1))]
    observations = []
    for index, running in [(1, True), (3, False), (5, True), (7, False)]:
        # Use the UNION across every delivery-derived origin estimate. This does
        # not choose a convenient offset, prove fixed queue delay or infer latency.
        begin = times[index] + .3 - origin[1]
        end = times[index] + .8 - origin[0]
        lo, hi = math.floor(begin * RATE), math.ceil(end * RATE)
        require(0 <= lo < hi <= len(pcm), 'window not fully captured')
        observations.append({'label': LABELS[index], 'pcm_window_seconds': [lo / RATE, hi / RATE],
                             'marker_monotonic': times[index], **classify(pcm[lo:hi], running)})
    return {'status': 'PCM_OBSERVATIONS_COMPLETE' if all(r['observation_passed'] for r in observations) else 'PCM_OBSERVATIONS_FAILED',
            'whole_run_verified': False, 'timing_alignment_verified': False,
            'run': marks['run'], 'thresholds': THRESHOLDS,
            'pcm': {'sha256': capture['sha256'], 'bytes': len(raw), 'duration_seconds': len(pcm) / RATE,
                    'format': 's16le', 'sample_rate': RATE, 'channels': 2},
            'estimated_origin_range_monotonic': origin, 'observations': observations,
            'activity_intervals_10ms_stereo_rms_gt100': activity_intervals,
            'timing_limit': 'Host delivery estimates select windows; their spread cannot bound an unknown constant queue delay. These observations do not prove exact synchronization, guest latency, input trust, API completion, host health or restoration.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    try:
        report = analyze((a.output / 'audio.raw').read_bytes(), json.loads((a.output / 'capture.json').read_text()), json.loads((a.output / 'marks.json').read_text()))
    except (ValueError, TypeError, KeyError, OSError) as error:
        report = {'status': 'PCM_ANALYSIS_REJECTED', 'whole_run_verified': False, 'reason': str(error)}
    report['analyzer_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    report['numpy_version'] = np.__version__
    (a.output / 'analysis.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print(json.dumps({'status': report['status'], 'whole_run_verified': False, 'output': str(a.output)}))
    raise SystemExit(0 if report['status'] == 'PCM_OBSERVATIONS_COMPLETE' else 1)


if __name__ == '__main__':
    main()

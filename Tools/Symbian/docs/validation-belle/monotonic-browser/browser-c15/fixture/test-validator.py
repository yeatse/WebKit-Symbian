#!/usr/bin/env python3
"""Host parsing contracts only; synthesized records do not prove a browser pass."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('clock_validator', Path(__file__).with_name('validate.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
RUN = '0123456789abcdef0123456789abcdef'
URL = 'http://localhost:8804/suite.js'


def report(deltas=None):
    if deltas is None:
        deltas = [46.875] * 8  # Valid coarse native tick; no 10ms requirement.
    timers = [16]
    for delta in deltas:
        timers.append(timers[-1] + delta)
    return {'run': RUN, 'timerCount': 8, 'requestedDelayMS': 40, 'syncSamples': [15.625] * 64,
            'timerSamples': timers, 'deltas': deltas, 'totalMS': sum(deltas),
            'subsecondYields': sum(0 < delta < 750 for delta in deltas)}


def log(data=None, ready_run=RUN, final_run=RUN, final_kind='DONE PASS', url=URL):
    if data is None:
        data = report()
    ready = {'run': ready_run, 'timerCount': 8, 'requestedDelayMS': 40}
    return f'console: {url}:1: SYMBIAN CLOCK {ready_run} READY '+json.dumps(ready)+'\n'+f'console: {url}:2: SYMBIAN CLOCK {final_run} {final_kind} '+json.dumps(data)+'\n'


class Contracts(unittest.TestCase):
    def check_failure(self, text):
        with self.assertRaises((ValueError, TypeError, KeyError)):
            module.validate(text, URL, RUN)

    def test_coarse_tick_valid_samples(self):
        self.assertEqual(module.validate(log(), URL, RUN)['positive_subsecond_yields'], 8)

    def test_two_large_vm_scheduling_delays_allowed(self):
        self.assertEqual(module.validate(log(report([60] * 6 + [1200, 1400])), URL, RUN)['positive_subsecond_yields'], 6)

    def test_every_yield_one_second_rejected(self):
        self.check_failure(log(report([1000] * 8)))

    def test_backward_and_zero_progress_rejected(self):
        self.check_failure(log(report([40] * 7 + [-1])))
        self.check_failure(log(report([40] * 7 + [0])))

    def test_wrong_token_and_url_rejected(self):
        self.check_failure(log(ready_run='f' * 32))
        self.check_failure(log(url='http://localhost:8805/suite.js'))

    def test_duplicate_missing_and_failure_results_rejected(self):
        text = log()
        self.check_failure(text + text.splitlines(keepends=True)[1])
        self.check_failure(text.splitlines(keepends=True)[0])
        self.check_failure(log(final_kind='DONE FAIL'))

    def test_forged_report_and_nonfinite_samples_rejected(self):
        data = report(); data['totalMS'] = 1
        self.check_failure(log(data))
        data = report(); data['timerSamples'][2] = float('nan')
        self.check_failure(log(data))
        data = report(); data['syncSamples'][0] = True
        self.check_failure(log(data))

    def test_requested_contract_change_rejected(self):
        data = report(); data['requestedDelayMS'] = 1000
        self.check_failure(log(data))
        data = report(); data['timerCount'] = 1
        self.check_failure(log(data))


if __name__ == '__main__':
    unittest.main(verbosity=2)

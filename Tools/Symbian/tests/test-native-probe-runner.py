#!/usr/bin/env python3
"""Host fixtures for finite-probe failure handling; these never run EKA."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

RUNNER = Path(__file__).resolve().parents[1] / 'eka2l1/run-native-probe.py'
FAKE = '''#!/usr/bin/python3
import json,os,pathlib,time
mode=os.environ['FAKE_MODE'];probe=os.environ.get('FAKE_PROBE','angle')
if mode=='timeout':time.sleep(20)
if mode=='host-failure':raise SystemExit(7)
if mode=='panic':print('KERN-EXEC guest panicked',flush=True);raise SystemExit(0)
name='angle-probe' if probe=='angle' else 'native-mmf'
log='angle-probe.log' if probe=='angle' else 'mmf-probe.log'
p=pathlib.Path(os.environ['XDG_DATA_HOME'])/'EKA2L1/data/drives/c/Data/WebKit'/log
prefix='' if probe=='angle' else 'MMF t=100 '
lines=[]
if mode!='missing-result':lines.append(prefix+'RESULT '+json.dumps({'probe':name,'passed':5,'failed':1 if mode=='failed-check' else 0}))
lines.append(prefix+('PASS: ' if probe=='angle' else 'PASS ')+name+' 5/5')
if mode!='missing-exit':lines.append(prefix+name+' exiting '+('1' if mode=='guest-failure' else '0'))
if mode=='mixed-negative-exit':lines.append(prefix+name+' exiting -1')
p.write_text('\\n'.join(lines)+'\\n')
'''


class NativeRunner(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.emulator = self.root / 'fake-emulator'
        self.emulator.write_text(FAKE)
        self.emulator.chmod(0o755)
        self.image = self.root / 'probe.exe'
        self.image.write_bytes(b'host fixture only')
        self.data = self.root / 'xdg/EKA2L1/data'
        self.data.mkdir(parents=True)
        self.env = dict(os.environ, XDG_DATA_HOME=str(self.root/'xdg'))
        self.env.pop('EKA2L1_DATA', None)

    def tearDown(self):
        self.temp.cleanup()

    def run_fixture(self, mode, probe='angle', extra=()):
        command = [sys.executable, str(RUNNER), '--probe', probe,
                   '--image', str(self.image), '--image-sha256', hashlib.sha256(self.image.read_bytes()).hexdigest(),
                   '--emulator', str(self.emulator), '--emulator-sha256', hashlib.sha256(self.emulator.read_bytes()).hexdigest(),
                   '--reports', str(self.root/'reports'), '--timeout', '.4', *extra]
        proc = subprocess.run(command, env=dict(self.env, FAKE_MODE=mode, FAKE_PROBE=probe), text=True, capture_output=True, timeout=6)
        reports = list((self.root/'reports').glob('*/result.json'))
        self.assertEqual(len(reports), 1, proc.stdout+proc.stderr)
        return proc, json.loads(reports[0].read_text())

    def test_angle_success(self):
        proc, report = self.run_fixture('success')
        self.assertEqual(proc.returncode, 0, proc.stdout+proc.stderr)
        self.assertEqual(report['status'], 'PASS')
        self.assertEqual(report['guest_exit'], 0)
        self.assertEqual(report['host_exit'], 0)

    def test_mmf_summary_without_colon(self):
        proc, report = self.run_fixture('success', 'mmf')
        self.assertEqual(proc.returncode, 0, proc.stdout+proc.stderr)
        self.assertEqual(report['result']['probe'], 'native-mmf')

    def test_host_nonzero_is_failure(self):
        proc, report = self.run_fixture('host-failure')
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(report['host_exit'], 7)
        self.assertIn('status 7', report['failure'])

    def test_deadline_is_failure(self):
        proc, report = self.run_fixture('timeout')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('deadline', report['failure'])

    def test_panic_is_failure(self):
        proc, report = self.run_fixture('panic')
        self.assertNotEqual(proc.returncode, 0)
        self.assertTrue(report['panic_or_access_violation'])

    def test_missing_result_is_failure(self):
        proc, report = self.run_fixture('missing-result')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('exactly one RESULT', report['failure'])

    def test_missing_guest_exit_is_failure(self):
        proc, report = self.run_fixture('missing-exit')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('exit marker', report['failure'])

    def test_failed_check_is_failure(self):
        proc, report = self.run_fixture('failed-check')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('RESULT reports failure', report['failure'])

    def test_mixed_negative_exit_is_failure(self):
        proc, report = self.run_fixture('mixed-negative-exit')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('exactly one successful guest exit', report['failure'])

    def test_empty_pcm_is_failure(self):
        recorder = self.root / 'parec'
        recorder.write_text('#!/usr/bin/python3\nimport time\ntime.sleep(20)\n')
        recorder.chmod(0o755)
        self.env['PATH'] = str(self.root) + os.pathsep + self.env['PATH']
        proc, report = self.run_fixture('success', extra=('--capture-audio',))
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('recorded audio is empty', report['failure'])

    def test_guest_nonzero_is_failure(self):
        proc, report = self.run_fixture('guest-failure')
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(report['guest_exit'], 1)


if __name__ == '__main__':
    unittest.main()

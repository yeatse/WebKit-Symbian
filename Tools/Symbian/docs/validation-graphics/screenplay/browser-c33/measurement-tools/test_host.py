#!/usr/bin/env python3
"""Pure host/synthetic checks. Never launches a guest or opens a display."""
import ast
import copy
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'src'))
sys.path.insert(0, str(ROOT / 'fixture'))
import browser_support as support
import check as page_check


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


examples = load(ROOT / 'fixture/test_host.py', 'public_page_examples')
sandbox = load(ROOT / 'os_sandbox.py', 'prepared_browser_sandbox')


class PublicLog(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='screenplay-public-log-test-')
        self.directory = Path(self.temporary.name)
        self.log = self.directory / 'log.txt'
        self.reader = support.FreshPageLog(self.log, examples.RUN)

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, text):
        with self.log.open('ab') as stream:
            stream.write(text.encode('ascii'))
        self.reader.poll()

    def test_public_records_only_and_fixed_native_counts(self):
        self.write('UNRELATED_PRIVATE_VALUE_SENTINEL\n12:34:56.789 ScreenPlay: surface submitted 360x524\n')
        self.write(examples.wrapped(examples.example()))
        self.assertIsNone(self.reader.error)
        self.assertEqual(self.reader.report['page_status'], 'PAGE_REQUESTS_PASS')
        self.assertEqual(self.reader.screenplay_submitted, 1)
        self.assertEqual(self.reader.screenplay_fallbacks, 0)
        self.assertNotIn('SENTINEL', self.reader.public_text())
        self.assertNotIn('360x524', self.reader.public_text())

    def test_partial_lines_do_not_create_early_evidence(self):
        text = examples.wrapped(examples.example())
        self.write(text[:103])
        self.assertIsNone(self.reader.report)
        self.write(text[103:-1])
        self.assertIsNone(self.reader.report)
        self.write('\n')
        self.assertEqual(self.reader.report['page_status'], 'PAGE_REQUESTS_PASS')

    def test_existing_log_is_refused(self):
        self.log.write_text('old log')
        with self.assertRaises(support.Refusal):
            support.FreshPageLog(self.log, examples.RUN)

    def test_symlink_is_refused(self):
        target = self.directory / 'other'
        target.write_text('owned synthetic target')
        self.log.symlink_to(target)
        self.reader.poll()
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')

    def test_replaced_log_is_refused(self):
        self.write('first\n')
        self.log.rename(self.directory / 'old')
        self.log.write_text('replacement\n')
        self.reader.poll()
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')

    def test_truncated_log_is_refused(self):
        self.write('first synthetic line\n')
        self.log.write_bytes(b'')
        self.reader.poll()
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')

    def test_stale_record_is_refused_without_echo(self):
        self.write(examples.wrapped(examples.example()).replace(examples.RUN, 'b' * 32))
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')
        self.assertEqual(self.reader.rows, [])

    def test_unknown_field_is_refused_before_retention(self):
        rows = examples.example()
        rows[0]['unknown'] = 'PRIVATE_VALUE_SENTINEL'
        self.write(examples.wrapped(rows))
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')
        self.assertEqual(self.reader.rows, [])

    def test_record_after_done_is_refused(self):
        text = examples.wrapped(examples.example())
        self.write(text)
        self.write(text.splitlines()[0] + '\n')
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')

    def test_fallback_is_counted_not_discarded(self):
        self.write('12:34:56.789 ScreenPlay: initialization unavailable (-5), using bitmap\n')
        self.write('12:34:56.790 ScreenPlay: frame failed (-1), using bitmap\n')
        self.assertEqual(self.reader.screenplay_submitted, 0)
        self.assertEqual(self.reader.screenplay_fallbacks, 2)
        self.assertIsNone(self.reader.error)

    def test_unknown_native_tag_is_refused(self):
        self.write('12:34:56.789 ScreenPlay: UNKNOWN_PRIVATE_VALUE_SENTINEL\n')
        self.assertEqual(self.reader.error, 'FRESH_PUBLIC_PAGE_RECORD_REJECTED')
        self.assertNotIn('SENTINEL', self.reader.error)


class ControllerWait(unittest.TestCase):
    def run_wait(self, result, identity, callback, repeats=1):
        class Process:
            args = ['owned-synthetic-process']
            calls = 0
            def wait(self, timeout):
                self.calls += 1
                if self.calls <= repeats:
                    raise subprocess.TimeoutExpired(self.args, timeout)
                return 0
        root = {'root_pid': 100, 'root_starttime': 200, 'owned_pgid': 100}
        with patch.object(sandbox.time, 'monotonic', return_value=1):
            return sandbox.wait_with_controller(Process(), 60, result, identity, callback, root)

    def test_hook_receives_only_verified_owned_context(self):
        result = {'guard_ok': True, 'child_pid_associated': True, 'audit_ok': False}
        captured = []
        code = self.run_wait(result, {'namespace_pid': 2, 'pidns_inode': 300}, captured.append)
        self.assertEqual(code, 0)
        self.assertEqual(captured, [{'root_pid': 100, 'root_starttime': 200, 'owned_pgid': 100,
            'guard_ok': True, 'child_pid_associated': True, 'namespace_pid': 2, 'pidns_inode': 300}])
        self.assertFalse(result['audit_ok'])
        self.assertEqual(result['controller_error_count'], 0)

    def test_missing_guard_or_association_never_calls_hook(self):
        for guard, associated in ((False, True), (True, False), (False, False)):
            with self.subTest(guard=guard, associated=associated):
                captured = []
                self.run_wait({'guard_ok': guard, 'child_pid_associated': associated},
                    {'namespace_pid': 2, 'pidns_inode': 300}, captured.append)
                self.assertEqual(captured, [])

    def test_invalid_namespace_identity_never_calls_hook(self):
        for identity in ({}, {'namespace_pid': True, 'pidns_inode': 300},
                         {'namespace_pid': 2, 'pidns_inode': 0}):
            captured = []
            self.run_wait({'guard_ok': True, 'child_pid_associated': True}, identity, captured.append)
            self.assertEqual(captured, [])

    def test_exception_disables_hook_without_rewriting_audit(self):
        calls = []
        def failing(_):
            calls.append(True)
            raise RuntimeError('PRIVATE_VALUE_SENTINEL')
        result = {'guard_ok': True, 'child_pid_associated': True, 'audit_ok': False}
        code = self.run_wait(result, {'namespace_pid': 2, 'pidns_inode': 300}, failing, repeats=4)
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), 1)
        self.assertEqual(result['controller_error_count'], 1)
        self.assertFalse(result['audit_ok'])


class NormalClose(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='screenplay-controller-test-')
        directory = Path(self.temporary.name)
        self.log = directory / 'log.txt'
        self.page = support.FreshPageLog(self.log, examples.RUN)
        self.log.write_text(examples.wrapped(examples.example()))
        self.wm = ROOT / 'bin/owned-wm-close'
        self.roi = ROOT / 'bin/owned-color-rois'
        self.controller = support.BrowserController(self.page, ':95', '/owned/synthetic/host',
            self.wm, support.sha(self.wm), self.roi, support.sha(self.roi),
            {'origin_x': 0, 'origin_y': 0, 'scale_numerator': 1, 'scale_denominator': 1, 'proof_sha256': 'a' * 64})
        class FakeProc:
            OwnershipError = RuntimeError
            @staticmethod
            def map_owned_namespace_pid(*_, **__):
                return {'pid': 501, 'starttime': 600}
            @staticmethod
            def stat(_):
                raise FileNotFoundError
        self.controller.proc = FakeProc
        self.calls = []
        self.context = {'guard_ok': True, 'child_pid_associated': True, 'root_pid': 100,
            'root_starttime': 200, 'owned_pgid': 100, 'namespace_pid': 2, 'pidns_inode': 300}
        self.closed_sandbox = {'stopped': True, 'exit_code': 0, 'forced_cleanup': False, 'collectors_live': 0}

    def tearDown(self):
        self.temporary.cleanup()

    def helper(self, executable, arguments):
        self.calls.append((executable, arguments))
        if executable == str(self.roi):
            return 0, {'version': 1, 'run': examples.RUN, 'status': 'PUBLIC_COLOR_ROIS_MATCH',
                'window_owner_verified': True,
                'patches': [{'name': name, 'pixel_count': 1, 'mismatch_count': 0} for name in support.ROI_NAMES],
                'pixel_count': len(support.ROI_NAMES), 'mismatch_count': 0}
        return 0, {'wm_close_sent': True, 'window_owner_verified': True}

    def drive(self):
        with patch.object(support.time, 'monotonic', side_effect=(1, 2, 3, 4)):
            for _ in range(4):
                self.controller.tick(self.context)

    def test_single_close_and_independent_exit(self):
        self.controller.helper = self.helper
        self.drive()
        self.assertEqual([call[0] for call in self.calls], [str(self.roi), str(self.wm)])
        report = self.controller.finish(self.closed_sandbox)
        self.assertTrue(report['normal_close_sent'])
        self.assertTrue(report['owned_host_exit_independently_observed'])
        self.assertEqual(report['public_roi_report']['status'], 'PUBLIC_COLOR_ROIS_MATCH')
        self.assertFalse(report['screenplay_native_path_verified'])

    def test_successful_send_does_not_prove_exit(self):
        self.controller.helper = self.helper
        self.drive()
        report = self.controller.finish(dict(self.closed_sandbox, stopped=False))
        self.assertTrue(report['normal_close_sent'])
        self.assertFalse(report['owned_host_exit_independently_observed'])

    def test_roi_failure_still_closes_once_and_retains_failure(self):
        def helper(executable, arguments):
            code, report = self.helper(executable, arguments)
            if executable == str(self.roi):
                report['patches'][0]['mismatch_count'] = 1
            return code, report
        self.controller.helper = helper
        self.drive()
        report = self.controller.finish(self.closed_sandbox)
        self.assertEqual(report['controller_error_code'], 'PUBLIC_ROI_OBSERVATION_REJECTED')
        self.assertTrue(report['normal_close_sent'])
        self.assertIsNone(report['public_roi_report'])
        self.assertEqual(len(self.calls), 2)

    def test_close_rejection_has_no_second_send(self):
        def helper(executable, arguments):
            code, report = self.helper(executable, arguments)
            return (2, {'wm_close_sent': False, 'error_code': 'OWNER_OR_WINDOW_UNPROVEN'}) if executable == str(self.wm) else (code, report)
        self.controller.helper = helper
        self.drive()
        report = self.controller.finish(self.closed_sandbox)
        self.assertTrue(report['normal_close_attempted'])
        self.assertFalse(report['normal_close_sent'])
        self.assertEqual(report['controller_error_code'], 'OWNED_WINDOW_NORMAL_CLOSE_UNPROVEN')
        self.assertEqual(len(self.calls), 2)

    def test_unverified_guard_never_reaches_x_helpers(self):
        self.controller.helper = self.helper
        self.context['guard_ok'] = False
        self.drive()
        self.assertEqual(self.calls, [])


class StaticAndPins(unittest.TestCase):
    def test_roi_table_matches_public_fixture_exactly(self):
        source = (ROOT / 'src/owned-color-rois.cpp').read_text()
        rows = re.findall(r'\{"([a-z0-9_]+)",([0-9,]+)\}', source)
        actual = {name: [int(v) for v in values.split(',')] for name, values in rows}
        manifest = json.loads((ROOT / 'fixture/manifest.json').read_text())
        expected = {p['name']: p['roi'] + p['expected_rgb'] + [p['per_channel_tolerance']]
            for p in manifest['screenshot_coordinates']['patches'] if p['name'] != 'webgl_magenta'}
        self.assertEqual(actual, expected)
        self.assertEqual(tuple(actual), support.ROI_NAMES)

    def test_no_full_window_or_screen_capture(self):
        source = (ROOT / 'src/owned-color-rois.cpp').read_text()
        self.assertEqual(source.count('XGetImage('), 1)
        self.assertIn('onRoot.x,onRoot.y,onRoot.w,onRoot.h', source)
        self.assertNotIn('XWriteBitmapFile', source)
        self.assertNotIn('_NET_WM_NAME', source)
        self.assertNotIn('XFetchName', source)

    def test_fixed_candidate33_public_pins(self):
        candidate = Path('/workspace/symbian-belle-work/candidate33')
        pins = json.loads((ROOT / 'candidate-files.json').read_text())
        self.assertEqual(len(pins), 6)
        for name, pin in pins.items():
            support.verify_file(candidate / name, pin)

    def test_patch_allowlist_excludes_raw_rom_copy_sources(self):
        pins = json.loads((ROOT / 'patch-assets.json').read_text())
        self.assertEqual(len(pins), 6)
        self.assertTrue(all(name.startswith('mediaclient') for name in pins))
        self.assertFalse(any('goommonitor' in name or 'avkonfep' in name for name in pins))
        for name, pin in pins.items():
            support.verify_file(Path('/workspace/EKA2L1-validation/build/bin/patch') / name, pin)

    def test_preservation_and_audit_controls_remain(self):
        source = (ROOT / 'os_sandbox.py').read_text()
        for required in ("'--ro-bind','/','/'", "'-o','/proc/self/fd/'", "'--unshare-net'",
                         "if any(patch.iterdir()):raise ValueError('patch not empty')",
                         "result['audit_capture_ok']and not result['unexpected_peers']and result['denied_mutations']==0 and result['unsupported_async']==0"):
            self.assertIn(required, source)
        self.assertIn("if controller_hook is None:result['exit_code']=process.wait(timeout=timeout)", source)
        guard = (ROOT / 'sandbox_guard.py').read_text()
        for required in ("phase='ROOT_READONLY'", "phase='EMPTY_PATCH'", "phase='USERNS_DISABLED'", "'namespace_pid':os.getpid()"):
            self.assertIn(required, guard)

    def test_all_delivered_python_syntax(self):
        for path in list((ROOT / 'src').glob('*.py')) + list((ROOT).glob('*.py')):
            ast.parse(path.read_text(), filename=path.name)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Synthetic protocol boundary checks, not native graphics/runtime evidence."""
import json
from pathlib import Path
import tempfile
import unittest
import validate

TOKEN = 'owned-screenplay-synthetic-01'
IDENTITY = {'suite': validate.SUITE, 'version': 2, 'run': TOKEN}

def available():
    cycles = []
    for index in range(2):
        row = {**IDENTITY, 'cycle': index, **dict.fromkeys(validate.CYCLE_RCS, 0), **dict.fromkeys(validate.CYCLE_BOOLS, True)}
        row.update({'second_available_rc': -3, 'timed_out': False})
        cycles.append(row)
    result = {**IDENTITY, **dict.fromkeys(validate.RESULT_RCS, 0), **dict.fromkeys(validate.RESULT_BOOLS, False), 'status': 'AVAILABLE', 'cycles_attempted': 2, 'cycles_completed': 2, 'submissions': 4, 'opaque_window': True}
    for key in validate.SUCCESS_RESULT_BOOLS:
        result[key] = True
    return cycles, result

def encode(cycles, result):
    lines = ['BEGIN ' + json.dumps(IDENTITY)]
    lines += ['CYCLE ' + json.dumps(row) for row in cycles]
    lines += ['RESULT ' + json.dumps(result), 'screenplay-capability exiting 0']
    return '\n'.join(lines) + '\n'

class RecordBoundaryTests(unittest.TestCase):
    def reject(self, mutate):
        cycles, result = available()
        mutate(cycles, result)
        with self.assertRaises((ValueError, TypeError, KeyError, AttributeError)):
            validate.validate(encode(cycles, result), TOKEN)

    def test_available_remains_record_only(self):
        result = validate.validate(encode(*available()), TOKEN)
        self.assertEqual(result['capability'], 'AVAILABLE')
        self.assertTrue(result['record_only'])
        self.assertTrue(result['requires_outer_health_and_restore'])
        self.assertFalse(result['display_verified'])
        self.assertFalse(result['phone_verified'])

    def test_available_need_not_remain_pending(self):
        cycles, result = available()
        cycles[0]['first_pending_before_second'] = False
        self.assertEqual(validate.validate(encode(cycles, result), TOKEN)['capability'], 'AVAILABLE')

    def test_final_completion_may_be_none_or_cancel(self):
        cycles, result = available()
        cycles[0]['second_available_rc'] = 0
        self.assertEqual(validate.validate(encode(cycles, result), TOKEN)['cycles_completed'], 2)

    def test_fresh_token_required(self):
        with self.assertRaises(ValueError):
            validate.validate(encode(*available()), 'owned-stale-token-02')

    def test_unknown_private_fields_rejected(self):
        for key in ['surface_id', 'native_address', 'stride', 'offset', 'device', 'configuration', 'raw_log']:
            with self.subTest(key=key):
                self.reject(lambda c, r: r.update({key: 'forbidden-field'}))

    def test_unknown_cycle_fields_rejected(self):
        self.reject(lambda c, r: c[0].update({'framebuffer': 'forbidden-field'}))

    def test_display_and_performance_cannot_be_inferred(self):
        for key in ['display_verified', 'vsync_verified', 'performance_verified']:
            with self.subTest(key=key):
                self.reject(lambda c, r: r.update({key: True}))

    def test_duplicate_json_fields_rejected(self):
        text = encode(*available()).replace('"cycle": 0,', '"cycle": 0, "cycle": 0,', 1)
        with self.assertRaises(ValueError):
            validate.validate(text, TOKEN)

    def test_boolean_is_not_native_status(self):
        self.reject(lambda c, r: r.update({'rc': False}))

    def test_integer_is_not_boolean(self):
        self.reject(lambda c, r: c[0].update({'layout_valid': 1}))

    def test_boolean_is_not_count(self):
        self.reject(lambda c, r: r.update({'cycles_completed': True}))

    def test_wrong_suite_and_version_rejected(self):
        self.reject(lambda c, r: r.update({'suite': 'another-suite'}))
        self.reject(lambda c, r: c[1].update({'version': 1}))

    def test_nonfinite_json_rejected(self):
        text = encode(*available()).replace('"rc": 0', '"rc": NaN', 1)
        with self.assertRaises(ValueError):
            validate.validate(text, TOKEN)

    def test_missing_completion_rejected(self):
        with self.assertRaises(ValueError):
            validate.validate(encode(*available()).replace('screenplay-capability exiting 0', ''), TOKEN)

    def test_extra_text_rejected(self):
        with self.assertRaises(ValueError):
            validate.validate(encode(*available()) + 'extra\n', TOKEN)

    def test_incomplete_cycle_count_rejected(self):
        self.reject(lambda c, r: c.pop())

    def test_swapped_or_duplicated_cycles_rejected(self):
        self.reject(lambda c, r: c.reverse())
        self.reject(lambda c, r: c[1].update({'cycle': 0}))

    def test_pixel_result_needs_mapping(self):
        self.reject(lambda c, r: c[0].update({'layout_valid': False}))

    def test_exact_two_buffers_required(self):
        self.reject(lambda c, r: c[0].update({'exact_two_buffers': False}))

    def test_owned_binding_required(self):
        self.reject(lambda c, r: c[0].update({'background_owned': False}))

    def test_opaque_window_precondition_required(self):
        self.reject(lambda c, r: r.update({'opaque_window': False}))

    def test_actual_window_key_color_required(self):
        self.reject(lambda c, r: c[0].update({'overlay_uses_key_color': False}))

    def test_two_successful_submits_required(self):
        self.reject(lambda c, r: c[0].update({'submit1_rc': -5}))
        self.reject(lambda c, r: r.update({'submissions': 3}))

    def test_availability_must_succeed(self):
        self.reject(lambda c, r: c[0].update({'first_available_rc': -3}))

    def test_no_release_without_drain(self):
        self.reject(lambda c, r: c[0].update({'notifications_drained': False}))

    def test_remove_and_unregister_required(self):
        self.reject(lambda c, r: c[1].update({'detached': False}))
        self.reject(lambda c, r: c[1].update({'unregistered': False}))

    def test_native_close_failure_cannot_be_available(self):
        self.reject(lambda c, r: c[0].update({'close_surface_rc': -2}))
        self.reject(lambda c, r: r.update({'manager_closed': False}))

    def test_negative_completion_is_not_hidden(self):
        self.reject(lambda c, r: c[0].update({'second_available_rc': -14}))

    def test_startup_unavailable_is_not_available(self):
        _, result = available()
        result.update({'status': 'UNAVAILABLE', 'rc': -5, 'manager_rc': -5, 'update_rc': -18, 'manager_closed': False, 'update_session_closed': False, 'cycles_attempted': 0, 'cycles_completed': 0, 'submissions': 0})
        observed = validate.validate(encode([], result), TOKEN)
        self.assertEqual(observed['capability'], 'UNAVAILABLE')
        self.assertEqual(observed['native_status'], -5)

    def test_initial_create_failure_preserves_cleanup(self):
        _, result = available()
        cycle = {**IDENTITY, 'cycle': 0, **dict.fromkeys(validate.CYCLE_RCS, -18), **dict.fromkeys(validate.CYCLE_BOOLS, False)}
        cycle.update({'rc': -5, 'create_rc': -5, 'notifications_drained': True})
        result.update({'status': 'UNAVAILABLE', 'rc': -5, 'cycles_attempted': 1, 'cycles_completed': 0, 'submissions': 0})
        self.assertEqual(validate.validate(encode([cycle], result), TOKEN)['capability'], 'UNAVAILABLE')

    def test_submit_failure_can_cancel_and_clean(self):
        cycles, result = available()
        cycle = cycles[0]
        cycle.update({'rc': -20, 'submit1_rc': -20, 'first_available_rc': -3, 'two_buffers_submitted': False, 'available_after_second': False})
        result.update({'status': 'UNAVAILABLE', 'rc': -20, 'cycles_attempted': 1, 'cycles_completed': 0, 'submissions': 1})
        self.assertEqual(validate.validate(encode([cycle], result), TOKEN)['capability'], 'UNAVAILABLE')

    def test_pending_status_is_failed_retention(self):
        cycles, result = available()
        cycle = cycles[0]
        cycle.update({'rc': -33, 'first_available_rc': -18, 'second_available_rc': -18, 'available_after_second': False, 'notifications_drained': False, 'timed_out': True, 'mapping_closed': False, 'surface_closed': False, 'unregistered': False, 'close_surface_rc': -18})
        result.update({'status': 'UNAVAILABLE', 'rc': -33, 'cycles_attempted': 1, 'cycles_completed': 0, 'submissions': 2, 'retained_pending_requests': True, 'timed_out': True})
        for key in validate.SUCCESS_RESULT_BOOLS:
            result[key] = False
        observed = validate.validate(encode([cycle], result), TOKEN)
        self.assertEqual(observed['capability'], 'UNAVAILABLE')
        self.assertTrue(observed['retained_pending_requests'])

    def test_oversize_and_symlink_files_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ordinary = root / 'ordinary.txt'
            ordinary.write_text(encode(*available()))
            self.assertEqual(validate.read_public(ordinary), ordinary.read_text())
            alias = root / 'alias.txt'
            alias.symlink_to(ordinary)
            with self.assertRaises(OSError):
                validate.read_public(alias)
            ordinary.write_bytes(b'x' * 32769)
            with self.assertRaises(ValueError):
                validate.read_public(ordinary)

    def test_oversize_and_invalid_token_rejected(self):
        with self.assertRaises(ValueError):
            validate.validate('x' * 32769, TOKEN)
        with self.assertRaises(ValueError):
            validate.validate(encode(*available()), 'bad token')

if __name__ == '__main__':
    unittest.main()

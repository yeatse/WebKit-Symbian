#!/usr/bin/env python3
"""Validate a bounded public ScreenPlay API record; never certify its origin."""
import argparse
import json
import os
from pathlib import Path
import re
import stat

SUITE = 'screenplay-capability'
IDENTITY = {'suite', 'version', 'run'}
RESULT_RCS = set('rc ws_rc fbs_rc screen_rc group_rc window_rc graphics_rc manager_rc update_rc'.split())
RESULT_COUNTS = {'cycles_attempted', 'cycles_completed', 'submissions'}
RESULT_BOOLS = set('manager_closed update_session_closed window_closed window_group_closed ws_closed fbs_disconnected opaque_window retained_pending_requests timed_out display_verified vsync_verified performance_verified'.split())
RESULT_FIELDS = IDENTITY | RESULT_RCS | RESULT_COUNTS | RESULT_BOOLS | {'status'}
CYCLE_RCS = set('rc create_rc info_rc map_rc offsets_rc register_rc configuration_rc set_rc get_rc sync_rc submit0_rc submit1_rc first_available_rc second_available_rc detach_rc finish_rc close_surface_rc'.split())
CYCLE_BOOLS = set('exact_two_buffers layout_valid pixels_verified background_owned overlay_uses_key_color first_pending_before_second two_buffers_submitted available_after_second detached notifications_drained mapping_closed unregistered surface_closed timed_out'.split())
CYCLE_FIELDS = IDENTITY | CYCLE_RCS | CYCLE_BOOLS | {'cycle'}
SUCCESS_BOOLS = CYCLE_BOOLS - {'first_pending_before_second', 'timed_out'}
SUCCESS_RESULT_BOOLS = {'manager_closed', 'update_session_closed', 'window_closed', 'window_group_closed', 'ws_closed', 'fbs_disconnected'}


def strict_json(text):
    def pairs(rows):
        out = {}
        for key, value in rows:
            if key in out:
                raise ValueError('duplicate field')
            out[key] = value
        return out

    def constant(_):
        raise ValueError('nonfinite value')

    return json.loads(text, object_pairs_hook=pairs, parse_constant=constant)


def typed_record(record, fields, identity, bools, rcs):
    if not isinstance(record, dict) or set(record) != fields:
        raise ValueError('public field whitelist')
    if any(record[key] != value for key, value in identity.items()) or type(record['version']) is not int:
        raise ValueError('fresh record identity')
    if any(type(record[key]) is not bool for key in bools):
        raise ValueError('boolean fields')
    if any(type(record[key]) is not int or not -(2**31) <= record[key] <= 0 for key in rcs):
        raise ValueError('native status fields')


def validate(text, token):
    if not isinstance(token, str) or not re.fullmatch(r'[A-Za-z0-9_-]{8,64}', token):
        raise ValueError('run token')
    if not isinstance(text, str) or len(text.encode()) > 32768:
        raise ValueError('bounded public record')
    lines = text.splitlines()
    if not 3 <= len(lines) <= 5 or not lines[0].startswith('BEGIN ') or not lines[-2].startswith('RESULT ') or lines[-1] != 'screenplay-capability exiting 0':
        raise ValueError('fixed normal completion')
    identity = {'suite': SUITE, 'version': 2, 'run': token}
    begin = strict_json(lines[0][6:])
    if begin != identity or type(begin.get('version')) is not int:
        raise ValueError('begin identity')
    result = strict_json(lines[-2][7:])
    typed_record(result, RESULT_FIELDS, identity, RESULT_BOOLS, RESULT_RCS)
    if any(type(result[key]) is not int or not 0 <= result[key] <= 4 for key in RESULT_COUNTS):
        raise ValueError('aggregate counts')
    if not 0 <= result['cycles_completed'] <= result['cycles_attempted'] <= 2:
        raise ValueError('cycle count')
    if result['cycles_attempted'] != len(lines) - 3:
        raise ValueError('missing/extra cycle record')
    if result['status'] not in ['AVAILABLE', 'UNAVAILABLE'] or (result['status'] == 'AVAILABLE') != (result['rc'] == 0):
        raise ValueError('capability consistency')
    if any(result[key] for key in ['display_verified', 'vsync_verified', 'performance_verified']):
        raise ValueError('unsupported display/performance claim')
    cycles = []
    for index, line in enumerate(lines[1:-2]):
        if not line.startswith('CYCLE '):
            raise ValueError('cycle framing')
        row = strict_json(line[6:])
        typed_record(row, CYCLE_FIELDS, identity, CYCLE_BOOLS, CYCLE_RCS)
        if type(row['cycle']) is not int or row['cycle'] != index:
            raise ValueError('cycle ordering')
        if index + 1 < result['cycles_attempted'] and row['rc']:
            raise ValueError('continuation after failed cycle')
        if row['layout_valid'] and (not row['exact_two_buffers'] or any(row[key] for key in ['create_rc', 'info_rc', 'map_rc', 'offsets_rc'])):
            raise ValueError('layout prerequisites')
        if row['pixels_verified'] and not row['layout_valid']:
            raise ValueError('pixel prerequisites')
        if row['background_owned'] and (not row['pixels_verified'] or any(row[key] for key in ['register_rc', 'configuration_rc', 'set_rc', 'get_rc'])):
            raise ValueError('background binding prerequisites')
        if row['background_owned'] and not result['opaque_window']:
            raise ValueError('opaque window precondition')
        if row['overlay_uses_key_color'] and not row['background_owned']:
            raise ValueError('key color before binding')
        submitted = row['submit0_rc'] == 0 and row['submit1_rc'] == 0
        if row['two_buffers_submitted'] != submitted:
            raise ValueError('two buffer submission count')
        if row['first_pending_before_second'] and row['submit0_rc']:
            raise ValueError('pending before submit')
        if row['available_after_second'] and (not submitted or row['first_available_rc'] != 0):
            raise ValueError('availability completion')
        if row['detached'] and (row['set_rc'] != 0 or row['detach_rc'] != 0):
            raise ValueError('detach prerequisites')
        if row['unregistered'] and row['register_rc'] != 0:
            raise ValueError('registration cleanup')
        if row['surface_closed'] != (row['close_surface_rc'] == 0):
            raise ValueError('surface cleanup')
        if row['mapping_closed'] and row['map_rc'] != 0:
            raise ValueError('mapping cleanup')
        if not row['notifications_drained'] and (not row['timed_out'] or not result['retained_pending_requests']):
            raise ValueError('pending request storage')
        if row['rc'] == 0:
            if any(row[key] != 0 for key in CYCLE_RCS - {'second_available_rc'}) or row['second_available_rc'] not in [0, -3]:
                raise ValueError('successful native API statuses')
            if not all(row[key] for key in SUCCESS_BOOLS) or row['timed_out']:
                raise ValueError('successful cycle cleanup')
        cycles.append(row)
    if result['cycles_completed'] != sum(row['rc'] == 0 for row in cycles):
        raise ValueError('completed cycle aggregate')
    submissions = sum(int(row['submit0_rc'] == 0) + int(row['submit1_rc'] == 0) for row in cycles)
    if result['submissions'] != submissions:
        raise ValueError('submission aggregate')
    retained = result['retained_pending_requests']
    if retained and (not result['timed_out'] or not any(not row['notifications_drained'] for row in cycles)):
        raise ValueError('retention reason')
    if result['timed_out'] != any(row['timed_out'] for row in cycles):
        raise ValueError('timeout aggregate')
    close_pairs = [('ws_rc', 'ws_closed'), ('fbs_rc', 'fbs_disconnected'), ('group_rc', 'window_group_closed'), ('window_rc', 'window_closed'), ('manager_rc', 'manager_closed'), ('update_rc', 'update_session_closed')]
    for status, closed in close_pairs:
        if result[closed] != (result[status] == 0 and not retained):
            raise ValueError('owned resource cleanup')
    if result['cycles_attempted'] and any(result[key] for key in RESULT_RCS - {'rc'}):
        raise ValueError('cycle without window/session')
    if result['status'] == 'AVAILABLE':
        if result['cycles_attempted'] != 2 or result['cycles_completed'] != 2 or result['submissions'] != 4 or retained or result['timed_out']:
            raise ValueError('complete two-size lifecycle required')
        if not result['opaque_window'] or not all(result[key] for key in SUCCESS_RESULT_BOOLS) or any(result[key] for key in RESULT_RCS):
            raise ValueError('complete native cleanup required')
    return {
        'parse_status': 'PASS', 'run': token, 'capability': result['status'],
        'native_status': result['rc'], 'cycles_completed': result['cycles_completed'],
        'submissions': result['submissions'],
        'native_api_statuses': {key: result[key] for key in sorted(RESULT_RCS)},
        'cycles': cycles, 'retained_pending_requests': retained,
        'timed_out': result['timed_out'],
        'display_verified': False, 'vsync_verified': False,
        'performance_verified': False, 'phone_verified': False,
        'record_only': True, 'requires_outer_health_and_restore': True,
    }


def read_public(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_size > 32768:
            raise ValueError('bounded regular public file')
        value = os.read(descriptor, 32769)
        if len(value) > 32768:
            raise ValueError('record grew')
        return value.decode('utf-8')
    finally:
        os.close(descriptor)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--run', required=True)
    arguments = parser.parse_args()
    try:
        value = validate(read_public(arguments.log), arguments.run)
    except (ValueError, TypeError, KeyError, AttributeError, OSError):
        print(json.dumps({'parse_status': 'FAIL', 'display_verified': False, 'phone_verified': False}))
        return 1
    print(json.dumps(value))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

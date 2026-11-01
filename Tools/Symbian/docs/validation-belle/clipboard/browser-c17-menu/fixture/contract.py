"""Strict clipboard fixture identity/outcome contract; independent of native UI."""
import math
import re

CASES = (
    'trusted Unicode multiline copy and native paste',
    'trusted partial cut and native paste',
    'contenteditable copy and plain native paste',
    'DOM paste remains denied with a trusted click',
    'password copy and cut cannot replace clipboard',
    'readonly cut preserves text and clipboard',
    'native Copy and native Paste',
    'native Cut and native Paste',
    'native SelectAll then Copy and Paste',
)


def identity(report):
    if not isinstance(report, dict) or report.get('suite') != 'belle-clipboard':
        raise ValueError('wrong suite')
    run, mode = report.get('run'), report.get('mode')
    if not isinstance(run, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,80}', run):
        raise ValueError('invalid run token')
    if mode not in ('keyboard', 'menu'):
        raise ValueError('invalid input mode')
    return run, mode


def validate_shape(report):
    identity(report)
    rows = report.get('cases')
    if not isinstance(rows, list) or [r.get('name') if isinstance(r, dict) else None for r in rows] != list(CASES):
        raise ValueError('missing, duplicate or reordered cases')
    if any(row.get('status') not in ('PASS', 'FAIL') for row in rows):
        raise ValueError('invalid outcome')
    passed = sum(row['status'] == 'PASS' for row in rows)
    if any(type(report.get(key)) is not int for key in ('passed', 'failed', 'total')):
        raise ValueError('counts must be integers')
    if (report['passed'], report['failed'], report['total']) != (passed, len(CASES) - passed, len(CASES)):
        raise ValueError('summary and individual outcomes disagree')
    if report.get('status') != ('PASS' if passed == len(CASES) else 'FAIL'):
        raise ValueError('incorrect overall status')
    if not isinstance(report.get('origin'), str) or len(report['origin']) > 256:
        raise ValueError('invalid origin')
    elapsed = report.get('elapsed_ms')
    if type(elapsed) not in (int, float) or not math.isfinite(elapsed) or not 0 <= elapsed <= 480000:
        raise ValueError('invalid overall deadline')
    for row in rows:
        elapsed = row.get('elapsed_ms')
        if type(elapsed) not in (int, float) or not math.isfinite(elapsed) or not 0 <= elapsed <= 140000:
            raise ValueError('invalid case deadline')
        if not isinstance(row.get('events'), list) or len(row['events']) > 128 or not isinstance(row.get('observations'), dict):
            raise ValueError('missing or unbounded observations')
        if any(not isinstance(event, dict) for event in row['events']):
            raise ValueError('invalid event observation')


def validate(report, run, mode, origin):
    validate_shape(report)
    if identity(report) != (run, mode) or report['origin'] != origin:
        raise ValueError('wrong run, input mode or origin')
    if report['status'] != 'PASS':
        raise ValueError('page reported failure')
    for number, row in enumerate(report['cases']):
        if row.get('trustedClick') is not True:
            raise ValueError('case was not started by trusted input')
        paste = [e for e in row['events'] if e.get('type') == 'paste']
        inputs = [e for e in row['events'] if e.get('type') == 'input']
        if number == 3:
            if paste or inputs or row['observations'].get('domPasteDenied') is not True:
                raise ValueError('DOM paste denial did not remain a separate negative case')
            continue
        if len(paste) != 1 or paste[0].get('trusted') is not True or paste[0].get('plainTextMatches') is not True or paste[0].get('files') != 0:
            raise ValueError('positive case requires one trusted exact native paste')
        types = paste[0].get('types')
        if not isinstance(types, list) or not types or any(not isinstance(t, str) or t.lower() not in ('text/plain', 'text/plain;charset=utf-8') for t in types):
            raise ValueError('unexpected paste types')
        if not any(e.get('trusted') is True and e.get('inputType') == 'insertFromPaste' for e in inputs):
            raise ValueError('missing normal input event')
        if mode == 'keyboard' and not any(e.get('type') == 'keydown' and e.get('trusted') is True and e.get('key') == 'v' for e in row['events']):
            raise ValueError('keyboard paste lacks actual control-key evidence')

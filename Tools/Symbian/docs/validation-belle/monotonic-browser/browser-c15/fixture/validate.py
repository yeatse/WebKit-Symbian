#!/usr/bin/env python3
"""Strictly validate one fresh-token ordinary browser clock/timer log."""
import argparse
import json
import math
from pathlib import Path
import re


def require(value, message):
    if not value:
        raise ValueError(message)


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def validate(browser, script_url, run):
    require(re.fullmatch('[a-f0-9]{32}', run), 'invalid run token')
    require(not re.search(r'access violation|KERN-EXEC|USER-EXEC|\bPANIC\b|panicked|SIGSEGV|segmentation fault', browser, re.I), 'guest failure marker')
    # Match only actual page console output from this exact script URL. Other
    # source URLs, wrong tokens, extra results and truncated lines cannot pass.
    pattern = re.compile(r'^console: ' + re.escape(script_url)
        + r':\d+: SYMBIAN CLOCK ([a-f0-9]{32}) (READY|DONE PASS|DONE FAIL) (.*)$', re.M)
    events = []
    for match in pattern.finditer(browser):
        require(match[1] == run, 'wrong/stale console token')
        data = json.loads(match[3])
        require(type(data) is dict and data.get('run') == run, 'wrong/stale payload token')
        events.append((match[2], data, match.start()))
    require([event[0] for event in events] == ['READY', 'DONE PASS'], 'unique ordered READY/DONE PASS without failure required')
    ready, report = events[0][1], events[1][1]
    for data in (ready, report):
        require(type(data.get('timerCount')) is int and data['timerCount'] == 8, 'expected eight yields')
        require(type(data.get('requestedDelayMS')) is int and data['requestedDelayMS'] == 40, 'expected requested 40ms timers')
    sync, timers = report.get('syncSamples'), report.get('timerSamples')
    require(type(sync) is list and len(sync) == 64 and type(timers) is list and len(timers) == 9, 'missing clock samples')
    values = sync + timers
    require(all(number(value) and value >= 0 for value in values), 'invalid clock samples')
    require(all(a <= b for a, b in zip(values, values[1:])), 'backward clock')
    deltas = [b - a for a, b in zip(timers, timers[1:])]
    require(all(value > 0 for value in deltas), 'no positive progress')
    short = sum(0 < value < 750 for value in deltas)
    require(short >= 6, 'fewer than six positive subsecond timer yields')
    total = timers[-1] - timers[0]
    require(0 < total < 6000, 'timer sequence exceeded bounded 6s window')
    require(type(report.get('deltas')) is list and len(report['deltas']) == 8, 'missing delta report')
    require(all(number(a) and abs(a - b) <= 1e-6 for a, b in zip(report['deltas'], deltas)), 'reported deltas differ from samples')
    require(number(report.get('totalMS')) and abs(report['totalMS'] - total) <= 1e-6, 'reported elapsed differs from samples')
    require(type(report.get('subsecondYields')) is int and report['subsecondYields'] == short, 'subsecond count differs from samples')
    return {'status': 'PASS', 'run': run, 'timer_yields': 8, 'positive_subsecond_yields': short,
            'total_ms': total, 'timer_deltas_ms': deltas,
            'scope': 'Page clock/timer checks only; actual image pins and normal host exit require outer runner evidence'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--browser-log', required=True, type=Path)
    p.add_argument('--script-url', required=True)
    p.add_argument('--run', required=True)
    a = p.parse_args()
    try:
        value = validate(a.browser_log.read_text(errors='replace'), a.script_url, a.run)
        print(json.dumps(value, indent=2))
        return 0
    except (ValueError, OSError, TypeError, KeyError) as error:
        print(json.dumps({'status': 'FAIL', 'error': str(error)}, indent=2))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

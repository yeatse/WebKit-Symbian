#!/usr/bin/env python3
"""Require the immutable HTTP report to match the fresh guest console result."""
import argparse
import json
from pathlib import Path
from contract import validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--guest-log', type=Path, required=True)
    parser.add_argument('--run', required=True)
    parser.add_argument('--mode', choices=('keyboard', 'menu'), required=True)
    parser.add_argument('--origin', required=True)
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    validate(report, args.run, args.mode, args.origin)
    results = []
    for line in args.guest_log.read_text(errors='replace').splitlines():
        prefix, marker, data = line.partition('RESULT: belle-clipboard ')
        if marker:
            results.append(json.loads(data))
    if results != [report]:
        raise ValueError('require one fresh console report identical to saved HTTP report')
    if 'FAIL:' in args.guest_log.read_text(errors='replace'):
        raise ValueError('guest log contains a failure')
    print(json.dumps({'status': 'PASS', 'suite': 'belle-clipboard', 'run': args.run, 'mode': args.mode, 'cases': 9,
                      'scope': 'page/report contract only; native action ledger and normal guest/host exits remain required'}))


if __name__ == '__main__':
    main()

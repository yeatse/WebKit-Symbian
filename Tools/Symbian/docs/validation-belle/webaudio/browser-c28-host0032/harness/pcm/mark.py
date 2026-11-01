#!/usr/bin/env python3
"""Record a Root observation now; this cannot prove trusted host input."""
import argparse
import json
from pathlib import Path
import re
import time

LABELS = ['click-start', 'native-running', 'click-suspend', 'native-suspended',
          'click-resume', 'native-resumed', 'click-close', 'quiet']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--run', required=True)
    p.add_argument('--label', choices=LABELS, required=True)
    a = p.parse_args()
    if not re.fullmatch(r'wa-[0-9a-f]{32}', a.run):
        p.error('invalid fresh fixture token')
    if not (a.output / 'capture.json').is_file():
        p.error('capture output required')
    path = a.output / 'marks.json'
    value = json.loads(path.read_text()) if path.exists() else {
        'suite': 'classic-webaudio-pcm', 'version': 1, 'run': a.run,
        'provenance': 'root-host-observations-not-independent-input-proof', 'marks': []}
    if value['run'] != a.run or len(value['marks']) >= len(LABELS) or LABELS[len(value['marks'])] != a.label:
        p.error('wrong token, duplicate or out-of-order observation')
    value['marks'].append({'label': a.label, 'observed_monotonic': time.monotonic()})
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')
    temp.replace(path)
    print(json.dumps(value['marks'][-1]))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Anonymous stdin framing for public CI stdout; no raw-log persistence."""
import json
from pathlib import Path
import subprocess
import sys
import termios
import tty

root = Path(__file__).resolve().parent
fd = sys.stdin.fileno()
previous = termios.tcgetattr(fd)
print('READY_FOR_PUBLIC_CI_LOG', flush=True)
tty.setraw(fd)
try:
    count = int(sys.stdin.buffer.readline())
    assert 0 < count <= 64 * 1024 * 1024
    data = sys.stdin.buffer.read(count)
    assert len(data) == count
finally:
    termios.tcsetattr(fd, termios.TCSANOW, previous)
result = subprocess.run([sys.executable, str(root / 'check-ci-log.py'), str(root)],
                        input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if result.returncode == 0:
    print(result.stdout.decode('utf-8'))
else:
    report = {'status': 'FAIL_CI_EVIDENCE_CHECK', 'exit_code': result.returncode,
              'assertion_error': b'AssertionError' in result.stderr, 'raw_log_persisted': False}
    (root / 'ci-evidence-check-failure.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
raise SystemExit(result.returncode)

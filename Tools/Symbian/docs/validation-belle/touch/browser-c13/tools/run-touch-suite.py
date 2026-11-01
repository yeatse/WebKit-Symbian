#!/usr/bin/env python3
"""Observe one-finger DOM behavior through calibrated, real native input."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

base = Path('/workspace/symbian-belle-work/p1-validation')
root = Path('/workspace/WebKit-Symbian-graphics')
out = base / 'touch-sequences-c11-stable-viewport'
out.mkdir(exist_ok=False)
image = Path('/workspace/symbian-belle-work/candidate11/browser.exe')
host = Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
guest = Path(os.environ['XDG_DATA_HOME']) / 'EKA2L1/data/drives/c/Data/WebKit/log.txt'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(image) == '20f58cef43d6fa4252546cb337940bfa609ba7ef71d0059ef1336bd126ef5288'
assert sha(host) == '0d2714494b322684872c0fc036c2f206b8514e54d021dc2ec1e3182af5855b98'
fixture = root / 'Tools/Symbian/tests/belle-touch/index.html'
(out / 'fixture.html').write_bytes(fixture.read_bytes())
result = {'status': 'RUNNING', 'browser_sha256': sha(image), 'host_sha256': sha(host),
          'fixture_sha256': sha(fixture), 'content_rect': [270, 77, 360, 532],
          'calibration_image_sha256': sha(base / 'touch-tap-c11/calibration.png'), 'cases': []}
for case in ('prevent-start', 'prevent-move', 'pan', 'pointer-pan', 'none', 'capture', 'navigation'):
    run = uuid.uuid4().hex
    directory = out / case
    directory.mkdir()
    end_case = 'landing' if case == 'navigation' else case
    env = os.environ.copy()
    env.update(EKA2L1=str(host), ICU_DIR='/workspace/symbian-idna-work/icu',
               SYMBIAN_WEBKIT_WORK=str(directory), WAIT='90', EKA2L1_COMPLETION_GRACE='5',
               EKA2L1_EXPECT_PASS='SYMBIAN TOUCH ' + end_case + ' DONE PASS')
    url = f'http://localhost:8783/?case={case}&run={run}'
    row = {'case': case, 'run': run, 'url': url, 'status': 'FAIL'}
    process = None
    with (directory / 'driver.log').open('w') as log:
        try:
            process = subprocess.Popen(['bash', str(root / 'Tools/Symbian/eka2l1/run-browser.sh'), str(image), url], env=env, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 45
            ready = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError('Browser runner exited before fixture READY')
                text = guest.read_text(errors='replace') if guest.exists() else ''
                marker = 'SYMBIAN TOUCH ' + case + ' READY '
                for line in text.splitlines():
                    if marker in line:
                        value = json.loads(line.split(marker, 1)[1])
                        if value.get('run') == run:
                            ready = value
                if ready:
                    break
                time.sleep(.2)
            assert ready, 'Missing fresh READY'
            pid = int(subprocess.check_output(['pgrep', '-x', 'eka2l1_qt'], text=True).strip())
            assert Path(f'/proc/{pid}/exe').resolve() == host.resolve()
            ancestor = pid
            while ancestor != process.pid and ancestor > 1:
                ancestor = int(Path(f'/proc/{ancestor}/stat').read_text().split(') ', 1)[1].split()[1])
            assert ancestor == process.pid, 'Emulator does not belong to this runner'
            pad, viewport = ready['pad'], ready['viewport']
            assert viewport['width'] == 360, 'Device-width fixture viewport has not settled'
            scale = 360 / viewport['width']
            assert pad['height'] > 90 and pad['width'] > 30
            start = [pad['left'] + pad['width'] / 2, pad['top'] + pad['height'] - 30]
            finish = [start[0], pad['top'] + 30]
            if case == 'capture':
                finish = [pad['left'] + pad['width'] + 5, start[1]]
            def mapped(point):
                x, y = point
                assert 0 < x < viewport['width'] and 0 < y < viewport['height']
                assert x * scale < 360 and y * scale < 532
                return [str(round(270 + x * scale)), str(round(77 + y * scale))]
            command = [sys.executable, str(base / 'drive-touch.py'), '--pid', str(pid), '--guest-log', str(guest),
                       '--case', case, '--run', run, '--start', *mapped(start), '--output', str(directory / 'input')]
            if case not in ('prevent-start', 'navigation'):
                command += ['--end', *mapped(finish)]
            row.update(ready=ready, input_command=command)
            with (directory / 'input-driver.log').open('w') as output:
                action = subprocess.run(command, env=env, stdout=output, stderr=subprocess.STDOUT, timeout=60)
            row['input_exit'] = action.returncode
            row['launcher_exit'] = process.wait(timeout=100)
            assert row['input_exit'] == row['launcher_exit'] == 0, 'Native input or launcher failed'
            row['status'] = 'PASS'
        except BaseException as error:
            row['error'] = str(error)
            if process and process.poll() is None:
                # Let the existing bounded runner collect evidence and close.
                try:
                    row['launcher_exit'] = process.wait(timeout=110)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    row['launcher_exit'] = process.wait(timeout=20)
                    row['forced_runner_stop'] = True
        result['cases'].append(row)
        result['status'] = 'RUNNING' if row['status'] == 'PASS' else 'FAIL'
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(case + ': ' + row['status'], flush=True)
        if row['status'] != 'PASS':
            sys.exit(1)
result['status'] = 'PASS'
result['scope'] = 'Seven real one-finger sequences; excludes physical multitouch and independent native focus cancellation'
result['fixture_unchanged'] = sha(fixture) == result['fixture_sha256']
assert result['fixture_unchanged']
(out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')

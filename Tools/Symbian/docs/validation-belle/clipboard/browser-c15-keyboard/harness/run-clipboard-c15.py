#!/usr/bin/env python3
"""Observe browser editing while privately preserving the native clipboard."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--run', required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--calibration', action='store_true')
p.add_argument('--mode', choices=('keyboard', 'menu'), default='keyboard')
a = p.parse_args()
assert re.fullmatch('[A-Za-z0-9_-]{1,80}', a.run)
a.output.mkdir(parents=True, exist_ok=False, mode=0o700)
data = Path(os.environ['XDG_DATA_HOME']) / 'EKA2L1/data'
locks = []
for name in ('.jsc-run.lock', '.browser-run.lock'):
    f = (data / name).open('a')
    fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
    locks.append(f)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
image = Path('/workspace/symbian-belle-work/candidate15/browser.exe')
host = Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
assert sha(image) == '473679d5c12d7dfeb057955ebd2ec0291cdd5db4654b6a3a2c2dba7c31fb995e'
assert sha(host) == '277db2b3e44c69d4b860b2f76cf194178e820556671ad32f4891fcae2cb4c2ff'
store = data / 'drives/d/System/Data/Clpboard.cbd'
before = {'existed': store.exists(), 'parent_existed': store.parent.exists()}
backup = a.output / 'original-native-store.private'
if before['existed']:
    s = store.stat()
    before.update(mode=stat.S_IMODE(s.st_mode), mtime_ns=s.st_mtime_ns, atime_ns=s.st_atime_ns, sha256=sha(store))
    shutil.copy2(store, backup)
    os.chmod(backup, 0o600)
(a.output / 'preservation-before.json').write_text(json.dumps(before, indent=2) + '\n')
root = Path('/workspace/WebKit-Symbian-graphics')
expected = 'PASS: trusted Unicode multiline copy and native paste' if a.calibration else 'STORED: belle-clipboard ' + a.run
env = os.environ.copy()
env.update(EKA2L1=str(host), ICU_DIR='/workspace/symbian-idna-work/icu', SYMBIAN_WEBKIT_WORK=str(a.output),
           WAIT='90' if a.calibration else '500', SCREENSHOT='null:', EKA2L1_COMPLETION_GRACE='5',
           EKA2L1_EXPECT_PASS=expected, EKA2L1_BROWSER_LOCK_FD=str(locks[1].fileno()))
url = f'http://localhost:8802/index.html?run={a.run}&mode={a.mode}'
result = {'run': a.run, 'mode': a.mode, 'calibration_only': a.calibration, 'url': url,
          'browser_sha256': sha(image), 'host_sha256': sha(host), 'status': 'FAIL'}
code = None
try:
    with (a.output / 'driver.log').open('w') as log:
        child = subprocess.run(['bash', str(root / 'Tools/Symbian/eka2l1/run-browser.sh'), str(image), url],
                               env=env, pass_fds=tuple(f.fileno() for f in locks), stdout=log, stderr=subprocess.STDOUT)
        code = child.returncode
finally:
    if before['existed']:
        store.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(backup, store)
        os.chmod(store, before['mode'])
        os.utime(store, ns=(before['atime_ns'], before['mtime_ns']))
        restored = sha(store) == before['sha256'] and store.stat().st_mtime_ns == before['mtime_ns'] and stat.S_IMODE(store.stat().st_mode) == before['mode']
        os.utime(store, ns=(before['atime_ns'], before['mtime_ns']))
    else:
        store.unlink(missing_ok=True)
        if not before['parent_existed'] and store.parent.exists() and not list(store.parent.iterdir()):
            store.parent.rmdir()
        restored = not store.exists() and (before['parent_existed'] or not store.parent.exists())
    result.update(launcher_exit=code, original_clipboard_restored=restored)
    result['status'] = ('CALIBRATION_PARTIAL' if a.calibration else 'OBSERVATION_COMPLETE_PENDING_PAGE_VALIDATOR') if code == 0 and restored else 'FAIL'
    (a.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    assert restored, 'Original clipboard state could not be restored'
print(json.dumps(result, indent=2))
sys.exit(code if code is not None else 1)

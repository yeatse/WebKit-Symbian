#!/usr/bin/env python3
"""Exercise actual interface installation only in owned external prefixes."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
SDK = Path('/workspace/symbian-sdk')
SCRIPT = ROOT / 'overlay/Tools/Symbian/scripts/rebuild-sdk-screenplay.py'
TOOL = Path('/workspace/symbian-work/symbian-native/cpp/symbian/tools/symbian-native')

def snapshot():
    files = sorted(SDK.glob('lib/**/libsymbian_guest_runtime*.a'))
    files += [SDK / 'lib/libsymbian_guest_runtime.a', SDK / 'cmake/SymbianApp.cmake']
    files += sorted(Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/patches/symbian-native').glob('*.patch'))
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}

before = snapshot()
trial = ROOT / 'installer-trial'
assert not trial.exists()
trial.mkdir()
prefix = trial / 'sdk'
prefix.mkdir()
for relative in ['VERSION', 'cmake/SymbianApp.cmake',
                 'share/symbian/native/sources.json',
                 'include/native/graphics/surface.h']:
    target = prefix / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SDK / relative, target)
(prefix / 'bin').symlink_to(SDK / 'bin', target_is_directory=True)

reports = []
def run(*extra, expected=0):
    command = [sys.executable, str(SCRIPT), '--sdk', str(prefix),
               '--converter', str(TOOL), '--work', str(trial / 'work'), *extra]
    result = subprocess.run(command, capture_output=True, text=True)
    (trial / ('command-' + str(len(reports)) + '.log')).write_text(
        result.stdout + result.stderr)
    assert result.returncode == expected, result.stderr
    reports.append({'arguments': list(extra), 'exit': result.returncode})
    return result

run('--check-only', expected=1)
assert not (prefix / 'proxies').exists()
first = json.loads(run().stdout)
assert first['files_added'] == 12 and first['app_include_added'] is True
second = json.loads(run().stdout)
assert second['files_added'] == 0 and second['app_include_added'] is False
run('--check-only')
header = prefix / 'include/native/graphics/surfacemanager.h'
original = header.read_bytes()
header.write_bytes(original + b'\n// changed by negative control\n')
run(expected=1)
assert header.read_bytes() != original
header.write_bytes(original)
metadata = prefix / 'share/symbian/native/sources.json'
source_before = metadata.read_bytes()
sources = json.loads(source_before)
sources['sources']['graphics']['revision'] = '0' * 40
metadata.write_text(json.dumps(sources))
run(expected=1)
metadata.write_bytes(source_before)
run('--check-only')
assert snapshot() == before
(ROOT / 'installer-check-result.json').write_text(json.dumps({
    'status': 'PASS_EXTERNAL_INSTALL_IDEMPOTENCE_AND_REJECTION',
    'commands': reports, 'first_install': first, 'second_install': second,
    'shared_prefix_and_preceding_patches_unchanged': before,
    'native_target_executed': False,
}, indent=2) + '\n')
print(json.dumps({'status': 'PASS', 'commands': len(reports),
                  'original_inputs_unchanged': len(before), 'guest_run': False}))

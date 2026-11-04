#!/usr/bin/env python3
"""Verify the bounded patch in an owned temporary tree, never the checkout."""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, help='read only the 20 listed before files from this checkout')
args = parser.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

manifest = json.loads((ROOT / 'manifest.json').read_text())
patch = ROOT / manifest['patch']
assert sha(patch) == manifest['patch_sha256']
files = manifest['files']
assert len(files) == 20 and len({row['path'] for row in files}) == 20
for row in files:
    relative = Path(row['path'])
    assert not relative.is_absolute() and '..' not in relative.parts
numstat = subprocess.run(['git', 'apply', '--numstat', str(patch)], check=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True).stdout
assert {line.split('\t', 2)[2] for line in numstat.splitlines()} == {row['path'] for row in files}
source = args.source.resolve() if args.source else ROOT / 'base'
with tempfile.TemporaryDirectory(prefix='screenplay-scoped-replay-') as temporary:
    replay = Path(temporary)
    for row in files:
        path = source / row['path']
        before = row['before']
        assert not path.is_symlink()
        if before is None:
            assert not path.exists()
        else:
            assert path.is_file() and sha(path) == before['sha256']
            dest = replay / row['path']
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
    for extra_args in [['--check'], []]:
        subprocess.run(['git', 'apply', *extra_args, str(patch)], cwd=replay, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert {str(path.relative_to(replay)) for path in replay.rglob('*') if path.is_file()} == {row['path'] for row in files}
    for row in files:
        assert sha(replay / row['path']) == row['after']['sha256']
    for extra_args in [['--reverse', '--check'], ['--reverse']]:
        subprocess.run(['git', 'apply', *extra_args, str(patch)], cwd=replay, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    for row in files:
        path = replay / row['path']
        assert (sha(path) if path.is_file() else None) == (row['before']['sha256'] if row['before'] else None)
print(json.dumps({'status': 'PASS_SCOPED_SOURCE_REPLAY', 'paths': 20,
                  'patch_sha256': sha(patch), 'forward_exact': True,
                  'reverse_exact': True, 'checkout_modified': False,
                  'guest_executed': False, 'display_verified': False}))

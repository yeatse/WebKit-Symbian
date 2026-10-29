#!/usr/bin/env python3
"""Check the real generic-loop setter against an in-flight native callback."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'Source/WTF/wtf/generic/RunLoopGeneric.cpp'


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    text = args.source.read_text()
    extracted = '\n\n'.join(function(text, signature) for signature in (
        'void RunLoop::setWakeUpCallback(', 'void RunLoop::wakeUpWithLock()', 'void RunLoop::wakeUp()'))
    (args.output / 'production.inc').write_text(extracted + '\n')
    fixture = Path(__file__).with_name('wakeup.cpp')
    command = [*shlex.split(args.cxx), '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread',
               '-I'+str(args.output), str(fixture), '-o', str(args.output/'probe')]
    subprocess.run(command, check=True, timeout=30)
    completed = subprocess.run([str(args.output/'probe')], capture_output=True, text=True, timeout=10)
    (args.output/'log.txt').write_text(completed.stdout + completed.stderr)
    result = {'status': 'PASS' if completed.returncode == 0 else 'FAIL', 'exit': completed.returncode,
              'scope': 'actual setter/wakeup functions, host lock and callback-lifetime ordering; not a guest shutdown test',
              'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
              'extracted_sha256': hashlib.sha256(extracted.encode()).hexdigest(), 'compile_command': command}
    (args.output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(completed.stdout, end='')
    return completed.returncode


if __name__ == '__main__':
    raise SystemExit(main())

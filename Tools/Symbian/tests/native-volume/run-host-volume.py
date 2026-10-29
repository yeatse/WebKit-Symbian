#!/usr/bin/env python3
"""Compile only the RFs Volume host helper regression; never build/run EKA2L1."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--eka-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root, output = args.eka_root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).with_name('test-host-volume.cpp')
    helper = root / 'src/emu/services/include/services/fs/host_volume.h'
    binary = output / 'test-host-volume'
    command = shlex.split(os.environ.get('CXX', 'c++')) + [
        '-std=c++20', '-Wall', '-Wextra', '-Werror',
        '-I' + str(root / 'src/emu/services/include'),
        '-I' + str(root / 'src/emu/utils/include'), str(source), '-o', str(binary)]
    report = {'status': 'FAIL', 'scope': 'host helper; no emulator or IPC/guest execution',
              'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, helper)},
              'compile_command': command}
    try:
        compiled = subprocess.run(command, capture_output=True, text=True)
        (output / 'compile.log').write_text(compiled.stdout + compiled.stderr)
        compiled.check_returncode()
        tested = subprocess.run([str(binary)], capture_output=True, text=True)
        (output / 'host-test.log').write_text(tested.stdout + tested.stderr)
        tested.check_returncode()
        marker = re.findall(r'^RESULT: host-volume (\d+)/(\d+)$', tested.stdout, re.M)
        expected = 21 if Path('/dev/shm').is_dir() else 20
        if len(marker) != 1 or marker[0] != (str(expected), str(expected)):
            raise RuntimeError('Missing exact complete host regression result')
        report['helper_checks'] = expected
        report['second_filesystem_fixture'] = Path('/dev/shm').is_dir()
        if os.name == 'posix':
            with tempfile.TemporaryDirectory(prefix='eka-volume-permission-') as temporary:
                parent = Path(temporary)
                parent.chmod(0o755)
                child_binary = parent / 'probe'
                shutil.copyfile(binary, child_binary)
                child_binary.chmod(0o755)
                private = parent / 'private'
                private.mkdir()
                mapped = private / 'mapped'
                mapped.mkdir()
                private.chmod(0)

                def drop_privileges():
                    os.setgroups([])
                    os.setgid(65534)
                    os.setuid(65534)

                try:
                    tested = subprocess.run([str(child_binary), '--permission', str(mapped)],
                        capture_output=True, text=True,
                        preexec_fn=drop_privileges if os.geteuid() == 0 else None)
                    (output / 'permission-test.log').write_text(tested.stdout + tested.stderr)
                    tested.check_returncode()
                    report['actual_permission_check'] = 'PASS'
                finally:
                    private.chmod(0o700)
        else:
            report['actual_permission_check'] = 'NOT_RUN: POSIX permission fixture'
        report['status'] = 'PASS'
    except Exception as error:
        report['error'] = str(error)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Restore and run this fixed 20-test upstream sample on QEMU ARM1136."""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


def sha256(path):
    with path.open('rb') as source:
        digest = hashlib.sha256()
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
        return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', required=True, type=Path,
                        help='WebKit Git checkout containing the pinned upstream commit')
    parser.add_argument('--work', required=True, type=Path, help='scratch/output directory')
    parser.add_argument('--qemu', required=True, type=Path, help='qemu-arm executable')
    parser.add_argument('--sysroot', required=True, type=Path, help='ARM Linux runtime sysroot')
    parser.add_argument('--jsc', required=True, type=Path, help='ARMv6 softfp JSC Linux ELF')
    args = parser.parse_args()
    evidence = Path(__file__).resolve().parent
    sources = json.loads((evidence / 'sources.json').read_text())
    run_id = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    output = args.work.resolve() / run_id
    (output / 'logs').mkdir(parents=True)

    for item in sources['tests']:
        source = sources['upstream_commit'] + ':' + item['original_path']
        data = subprocess.check_output(['git', '-C', str(args.checkout), 'show', source])
        blob = subprocess.check_output(
            ['git', '-C', str(args.checkout), 'rev-parse', source], text=True).strip()
        if blob != item['git_blob'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise RuntimeError('Upstream source identity mismatch: ' + item['original_path'])
        destination = output / item['original_path']
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)

    # All guest threads share one CPU, while the default concurrent JIT stays enabled.
    cpu = min(os.sched_getaffinity(0))
    os.sched_setaffinity(0, {cpu})
    qemu, sysroot, jsc = args.qemu.resolve(), args.sysroot.resolve(), args.jsc.resolve()
    command = [str(qemu), '-cpu', 'arm1136', '-L', str(sysroot),
               '-U', 'LD_LIBRARY_PATH', '-U', 'LD_PRELOAD', str(jsc)]
    summary = {
        'run_id': run_id, 'upstream_commit': sources['upstream_commit'],
        'jsc_sha256_before': sha256(jsc), 'qemu_sha256': sha256(qemu),
        'guest_cpu': 'arm1136', 'host_cpu_affinity': [cpu], 'test_parallelism': 1,
        'per_process_timeout_seconds': 30, 'jsc_option_overrides': [], 'tests': [],
    }

    def save():
        (output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')

    def run(arguments, log_name):
        log_path = output / 'logs' / log_name
        started = time.monotonic()
        timed_out = False
        with log_path.open('wb') as log:
            proc = subprocess.Popen(command + arguments, cwd=output, stdout=log,
                                    stderr=subprocess.STDOUT, start_new_session=True)
            try:
                returncode = proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                timed_out = True
                os.killpg(proc.pid, signal.SIGKILL)
                returncode = proc.wait()
            finally:
                if proc.poll() is None:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
        return {'returncode': returncode, 'timed_out': timed_out,
                'elapsed_seconds': round(time.monotonic() - started, 4),
                'log': 'logs/' + log_name, 'log_sha256': sha256(log_path),
                'log_bytes': log_path.stat().st_size}

    flags = ['useJIT', 'useBaselineJIT', 'useDFGJIT', 'useFTLJIT', 'useConcurrentJIT',
             'numberOfDFGCompilerThreads', 'useRegExpJIT', 'useSamplingProfiler']
    preflight = ('let o=jscOptions(); print(JSON.stringify({'
                 + ','.join(flag + ':o.' + flag for flag in flags)
                 + ',testLoopCount:testLoopCount}));')
    summary['preflight'] = run(['-e', preflight], 'preflight.log')
    save()
    if summary['preflight']['returncode'] or summary['preflight']['timed_out']:
        raise RuntimeError('Profile preflight failed; see ' + str(output))
    actual = json.loads((output / 'logs/preflight.log').read_text())
    summary['default_options'] = actual
    expected = {'useJIT': 1, 'useBaselineJIT': 1, 'useDFGJIT': 1, 'useFTLJIT': 0,
                'useConcurrentJIT': 1, 'useRegExpJIT': 0, 'useSamplingProfiler': 0,
                'testLoopCount': 10000}
    summary['profile_matches'] = (all(actual[k] == v for k, v in expected.items())
                                  and actual['numberOfDFGCompilerThreads'] > 0)
    save()
    if not summary['profile_matches']:
        raise RuntimeError('Different default profile; see ' + str(output))

    started = time.monotonic()
    for item in sources['tests']:
        result = {'original_path': item['original_path']}
        result.update(run([item['original_path']], Path(item['original_path']).name + '.log'))
        result['status'] = ('TIMEOUT' if result['timed_out'] else
                            'PASS' if result['returncode'] == 0 else 'FAIL')
        summary['tests'].append(result)
        save()
        print(result['status'] + ': ' + item['original_path'], flush=True)
    summary['elapsed_seconds'] = round(time.monotonic() - started, 4)
    summary['jsc_sha256_after'] = sha256(jsc)
    summary['image_unchanged'] = summary['jsc_sha256_before'] == summary['jsc_sha256_after']
    summary['passed'] = sum(result['status'] == 'PASS' for result in summary['tests'])
    summary['failed'] = len(summary['tests']) - summary['passed']
    save()
    print(f"RESULT: {summary['passed']}/{len(summary['tests'])} PASS; {output}")
    return 0 if not summary['failed'] and summary['image_unchanged'] else 1


if __name__ == '__main__':
    sys.exit(main())

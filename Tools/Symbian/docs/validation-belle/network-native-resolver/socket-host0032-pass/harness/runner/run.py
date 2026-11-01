#!/usr/bin/env python3
"""Prepare only by default; execute one fixed normal-API literal baseline."""

import argparse
import contextlib
import datetime
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import subprocess
import sys
import time
import uuid


HERE = Path(__file__).resolve().parent
FIXTURE = HERE.parent
DATA = Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
BASE = DATA.parent
QT_CONFIG = Path('/workspace/eka-validation/xdg/config/EKA2L1')
WORKER_GUEST = r'C:\sys\bin\socket_default_contract.exe'
LOG_GUEST = r'C:\Data\WebKit\socket-default-contract.log'
TIMEOUT = 60
INPUTS = {
    'worker': (FIXTURE / 'build/e32/socket_default_contract.exe',
               'f23faaf665a03ec0858efab2c77655d9bf1cdb1f97e2aec3f0dc6539ed1fd6b1'),
    'host': (Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt'),
             '692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'),
    'fep': (Path('/workspace/EKA2L1-validation/build/bin/patch/avkonfep_general.dll'),
            'fd6092f2ad61d39ccb47475229c367f5494fe68a8bd2bd8d0e04dc1bafcdc7ae'),
    'validator': (FIXTURE / 'validate.py',
                  '8048a6d6a3405af63ba4266b503ea977da11339993de4ee984764b8af3076c08'),
    'preservation': (Path('/workspace/symbian-cookie-store-runtime-c21-work/runtime_support.py'),
                     '0deded8b62c096774b94ebbc2bfc86a9df51ef6b95a7dee7e1e23f7d7ba46a9b'),
    'configuration': (Path('/workspace/symbian-cookie-store-runtime-c21-work/config_preservation.py'),
                      'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0'),
    'close_window': (Path('/workspace/symbian-fep-secret-runtime-work/docs-candidate/validation-belle/fep-secret/browser-c19-host0029/harness/eka2l1/close-emulator-window.py'),
                     'd4bf431805790e88c120b58ab8d6eb4bd9cece6c5eaeb61d0fd98d0a326f9d4f'),
}
# No panic exception, including CookieStore's narrowly scoped shutdown exception.
FATAL = re.compile(
    r'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|'
    r'\bSIGSEGV\b|\bSIGABRT\b|\bfatal\b|Main loop exited with exception|'
    r'Unable to launch process|Thread .* (?:forcefully killed|terminated peacefully) '
    r'with category:.*exit code: -?[1-9][0-9]*', re.I)
PRIMARY_ZERO = re.compile(
    r'^.*\[Kernel\]: Thread Main forcefully killed with category: '
    r'(?:Kill|None|) and exit code: 0\s*$', re.M)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def identity(path):
    path = Path(path)
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return {'path': str(path), 'bytes': path.stat().st_size,
            'sha256': digest.hexdigest()}


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def checked_inputs():
    result = {}
    for name, (path, expected) in INPUTS.items():
        require(path.is_file() and not path.is_symlink(), name + ' input missing or symlinked')
        value = identity(path)
        require(value['sha256'] == expected, name + ' pinned input changed')
        result[name] = value
    return result


def read_log(path, maximum):
    require(path.is_file() and not path.is_symlink(), 'fresh owned log missing')
    require(path.stat().st_size <= maximum, 'owned log exceeds finite bound')
    with path.open('rb') as stream:
        data = stream.read(maximum + 1)
    require(len(data) <= maximum, 'owned log grew beyond finite bound')
    return data


def check_fatal(host, guest=b''):
    require(not FATAL.search(host.decode('utf-8', errors='replace')),
            'host/target fatal marker; host exit0 cannot override this')
    require(not FATAL.search(guest.decode('utf-8', errors='replace')),
            'guest fatal marker')


def completion_gate(result, host, guest, validator):
    """Require both normal completion evidence and complete restoration."""
    check_fatal(host, guest)
    require(type(result.get('host_exit')) is int and result['host_exit'] == 0
            and result.get('natural_host_exit') is True,
            'natural CLI host exit0 required')
    require(result.get('forced_cleanup') is False and result.get('wm_close_sent') is False
            and result.get('deadline_expired') is False,
            'timeout or cleanup can never establish PASS')
    require(result.get('restoration', {}).get('guest', {}).get('restored') is True
            and result.get('restoration', {}).get('configuration', {}).get('restored') is True,
            'all original files/directories/configuration must be restored')
    require(result.get('inputs_unchanged') is True, 'pinned inputs changed during run')
    report = validator.validate(guest, result['run'], 'socket-open')
    require(report['status'] == 'LOG_CONTRACT_PASS_PENDING_TARGET_EXIT'
            and report['cases'] == 4, 'original fixture validator did not pass')
    primary_zero = PRIMARY_ZERO.findall(host.decode('utf-8', errors='replace'))
    require(primary_zero, 'positive existing Kernel Main exit-code0 trace required; host0 alone is insufficient')
    return {'status': 'PASS', 'cases': 4, 'fixture': report,
            'kernel_primary_zero_records': len(primary_zero),
            'guest_completion': 'fresh 4-case exiting0 plus existing Kernel Main Kill/None exit-code0 trace, natural CLI host0 and no fatal markers',
            'exit_evidence_limit': 'Kernel names the primary thread Main, not the executable; the single CLI target and whole-host fatal scan are required jointly',
            'scope': 'Fixed literal API baseline only; no AAAA DNS, TCP/IPv6 connectivity or ROM implementation repair claim'}


def no_host():
    found = subprocess.run(['pgrep', '-x', 'eka2l1_qt'], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=3)
    require(found.returncode == 1, 'an EKA host is active or process check failed')


def safe_ancestors(path):
    for ancestor in (path, *path.parents):
        require(not ancestor.is_symlink(), 'symlink preservation ancestor rejected')


def execute(output, token, inputs):
    require(Path(os.environ.get('XDG_DATA_HOME', '/invalid')).resolve() == BASE.parent,
            'source /workspace/eka-validation/env.sh')
    require(Path(os.environ.get('XDG_CONFIG_HOME', '/invalid')).resolve() == QT_CONFIG.parent,
            'expected isolated XDG_CONFIG_HOME')
    require(os.environ.get('DISPLAY'), 'root-owned DISPLAY is required')
    require(os.environ.get('PULSE_SERVER') == 'unix:/workspace/symbian-media-work/runtime/native',
            'source /workspace/symbian-media-work/env.sh')
    for path in (DATA, BASE / 'config.yml', BASE / 'bindings', QT_CONFIG):
        safe_ancestors(path)
    output.mkdir(mode=0o700)
    result = {'status': 'FAIL', 'suite': 'network-native-contract-literals', 'run': token,
              'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'inputs': inputs, 'wrapper': identity(Path(__file__)),
              'deadline_seconds': TIMEOUT, 'deadline_expired': False,
              'forced_cleanup': False, 'wm_close_sent': False, 'natural_host_exit': False,
              'restoration': {'guest': {}, 'configuration': {}},
              'prerequisite_integrated': True, 'screenshots_taken': False,
              'server_started': False, 'private_data_archived': False}
    support = module(INPUTS['preservation'][0], 'literal_preservation')
    configuration = module(INPUTS['configuration'][0], 'literal_configuration')
    validator = module(INPUTS['validator'][0], 'original_literal_validator')
    process = saved = configurations = host_stream = None
    guest_path = None
    fresh_profile = False
    previous_handlers = {}
    host_log = output / 'emulator.log'
    worker_log = output / 'socket-default-contract.log'
    errors = []
    locks = []

    def interrupted(signum, _frame):
        raise RuntimeError('wrapper interrupted by signal ' + str(signum))

    for sig in (signal.SIGINT, signal.SIGTERM):
        previous_handlers[sig] = signal.signal(sig, interrupted)
    try:
        with contextlib.ExitStack() as stack:
            for name in ('.jsc-run.lock', '.browser-run.lock'):
                lock_path = DATA / name
                require(not lock_path.is_symlink(), 'symlink runtime lock rejected')
                lock = stack.enter_context(lock_path.open('a'))
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                locks.append(lock)
            no_host()
            require(checked_inputs() == inputs, 'input changed before isolation')
            saved = support.SavedFiles(DATA, token, result['restoration']['guest'])
            configurations = configuration.PrivateConfigurations(
                BASE / ('.literal-config-originals-' + token),
                [(BASE / 'config.yml', False), (BASE / 'bindings', True), (QT_CONFIG, True)],
                result['restoration']['configuration'])
            try:
                saved.reserve(support.resolve_guest(DATA, r'C:\Data\WebKit'), directory=True)
                fresh_profile = True
                destination = saved.reserve(support.resolve_guest(DATA, WORKER_GUEST))
                shutil.copyfile(INPUTS['worker'][0], destination)
                require(identity(destination)['sha256'] == INPUTS['worker'][1], 'worker deployment differs')
                configurations.reserve_all()
                guest_path = support.resolve_guest(DATA, LOG_GUEST)
                require(not guest_path.exists(), 'worker log must be absent in fresh profile')
                require(checked_inputs() == inputs, 'input changed before launch')
                command = [str(INPUTS['host'][0]), '--device', 'RM-707', '--run',
                           WORKER_GUEST, ' --run ' + token]
                result['command'] = command
                host_stream = host_log.open('xb')
                limit = resource.getrlimit(resource.RLIMIT_CORE)
                try:
                    resource.setrlimit(resource.RLIMIT_CORE, (0, limit[1]))
                    process = subprocess.Popen(command, cwd=INPUTS['host'][0].parent,
                        stdout=host_stream, stderr=subprocess.STDOUT, start_new_session=True,
                        pass_fds=tuple(lock.fileno() for lock in locks))
                finally:
                    resource.setrlimit(resource.RLIMIT_CORE, limit)
                result['host_pid'] = process.pid
                started = time.monotonic()
                deadline = started + TIMEOUT
                result['launched_monotonic'] = started
                result['deadline_monotonic'] = deadline
                while process.poll() is None:
                    if time.monotonic() >= deadline:
                        result['deadline_expired'] = True
                        raise RuntimeError('60-second outer guest/host deadline expired')
                    check_fatal(read_log(host_log, 8 * 1024 * 1024))
                    time.sleep(.1)
                result['host_exit'] = process.returncode
                result['natural_host_exit'] = True
                require(process.returncode == 0, 'host exited nonzero')
                host_stream.close()
                host_stream = None
                guest = read_log(guest_path, 128 * 1024)
                check_fatal(read_log(host_log, 8 * 1024 * 1024), guest)
                result['fixture_validation'] = validator.validate(guest, token, 'socket-open')
            finally:
                for sig in previous_handlers:
                    signal.signal(sig, signal.SIG_IGN)
                if process is not None and process.poll() is None:
                    result['forced_cleanup'] = True
                    result['wm_close_attempted'] = True
                    try:
                        require(checked_inputs() == inputs, 'input changed before own-window cleanup')
                        close = subprocess.run([sys.executable, str(INPUTS['close_window'][0]), str(process.pid)],
                            capture_output=True, timeout=4)
                        result['own_window_close_exit'] = close.returncode
                        result['wm_close_sent'] = close.returncode == 0
                        # Do not retain unrelated window names or helper output.
                        require(close.returncode == 0, 'own-PID WM close failed')
                    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
                        errors.append(str(error))
                    try:
                        process.wait(timeout=6)
                    except subprocess.TimeoutExpired:
                        errors.append('owned host remains alive: originals kept private; root cleanup required')
                if process is not None:
                    result['host_exit'] = process.poll()
                if host_stream:
                    host_stream.close()
                stopped = process is None or process.poll() is not None
                result['owned_host_stopped_before_restore'] = stopped
                if stopped:
                    try:
                        no_host()
                    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
                        stopped = False
                        errors.append(str(error))
                if fresh_profile and guest_path and guest_path.is_file():
                    try:
                        worker_log.write_bytes(read_log(guest_path, 128 * 1024))
                    except (OSError, RuntimeError) as error:
                        errors.append(str(error))
                if stopped:
                    for transaction in (configurations, saved):
                        try:
                            transaction.restore()
                        except (OSError, RuntimeError) as error:
                            errors.append(str(error))
                else:
                    errors.append('no restoration while an EKA host is live')
                    result['private_originals_retained'] = True
                try:
                    result['inputs_unchanged'] = checked_inputs() == inputs
                except (OSError, RuntimeError) as error:
                    result['inputs_unchanged'] = False
                    errors.append(str(error))
                result['runtime_locks_inherited_by_host'] = process is not None
                result['runtime_locks_released'] = process is None or process.poll() is not None
            require(not errors, '; '.join(errors))
            result['acceptance'] = completion_gate(result, read_log(host_log, 8 * 1024 * 1024),
                                                    read_log(worker_log, 128 * 1024), validator)
            result['status'] = 'PASS'
    except (Exception, KeyboardInterrupt) as error:
        result['error'] = str(error) or 'interrupted'
        result['status'] = 'FAIL'
    finally:
        for sig, handler in previous_handlers.items():
            signal.signal(sig, handler)
        if errors:
            result['cleanup_or_restoration_errors'] = errors
        result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        for name, path in (('host_log', host_log), ('worker_log', worker_log)):
            if path.is_file():
                result[name] = identity(path)
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--output', required=True, type=Path, help='Fresh absolute external directory')
    args = parser.parse_args()
    try:
        safe_ancestors(args.output)
        output = args.output.resolve()
        require(args.output.is_absolute() and not output.exists(), 'fresh absolute output required')
        require(not output.is_relative_to(BASE.parent.parent), 'output must be outside all emulator state')
        safe_ancestors(output)
        inputs = checked_inputs()
        token = 'socket-' + uuid.uuid4().hex
        if not args.execute:
            print(json.dumps({'status': 'PREPARED_NOT_EXECUTED', 'future_run': token,
                'future_output': str(output), 'inputs': inputs, 'deadline_seconds': TIMEOUT,
                'mode': 'socket-open', 'cases': 4, 'prerequisite_integrated': True}, indent=2))
            return 0
        result = execute(output, token, inputs)
        print(json.dumps({'status': result['status'], 'run': token,
                          'output': str(output), 'error': result.get('error')}, indent=2))
        return 0 if result['status'] == 'PASS' else 1
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

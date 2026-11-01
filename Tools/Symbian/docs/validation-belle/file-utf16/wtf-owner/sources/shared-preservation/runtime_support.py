#!/usr/bin/env python3
"""External P1 runtime support. Preparation does not launch EKA or edit guest files."""
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
import stat
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parent
REPO = Path('/workspace/WebKit-Symbian-graphics')
DATA = Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
HOST = Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
HOST_SHA = '07af513045bf26e200f7d711c682e219ee4c7cecc9e30f2fa3b08732fc1012e3'
PANIC = re.compile(r'access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault|SIGSEGV', re.I)

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def identity(path):
    path = Path(path)
    if not path.exists():
        return {'path': str(path), 'exists': False}
    meta = path.stat()
    result = {'path': str(path), 'exists': True, 'mode': stat.S_IMODE(meta.st_mode)}
    if path.is_file():
        result.update(bytes=meta.st_size, sha256=sha256(path))
    elif path.is_dir():
        # Preserve private directory by rename, without inventorying its contents.
        result.update(directory=True, device=meta.st_dev, inode=meta.st_ino, mtime_ns=meta.st_mtime_ns)
    else:
        raise RuntimeError('Expected a regular file or directory: ' + str(path))
    return result

def read(path):
    path = Path(path)
    return path.read_text(errors='replace') if path.is_file() else ''

def add_common_arguments(parser, default_timeout=90):
    parser.add_argument('--browser', type=Path, required=True)
    parser.add_argument('--browser-sha256', required=True)
    parser.add_argument('--host', type=Path, default=HOST)
    parser.add_argument('--host-sha256', required=True)
    parser.add_argument('--data', type=Path, default=DATA)
    parser.add_argument('--output', type=Path, required=True, help='Fresh absolute evidence directory')
    parser.add_argument('--timeout', type=float, default=default_timeout)
    parser.add_argument('--execute', action='store_true', help='Actually launch EKA; default only checks inputs')

def resolve_guest(data, target):
    require(re.fullmatch(r'[Cc]:[\\/][^:]+', target) is not None, 'Only scoped C-drive paths are accepted')
    parts = target[3:].replace('\\', '/').split('/')
    require(all(p and p not in ('.', '..') for p in parts), 'Invalid guest path')
    current = Path(data) / 'drives' / 'c'
    require(current.is_dir() and not current.is_symlink(), 'C drive must be a real directory')
    for part in parts:
        exact = current / part
        if exact.exists() or exact.is_symlink():
            current = exact
        elif current.is_dir():
            matches = [p for p in current.iterdir() if p.name.casefold() == part.casefold()]
            require(len(matches) <= 1, 'Ambiguous guest spelling: ' + str(exact))
            current = matches[0] if matches else exact
        else:
            current = exact
        require(not current.is_symlink(), 'Refusing symlinked guest path: ' + str(current))
    return current

class SavedFiles:
    """Rename originals within DATA; restore exact original files/directories."""
    def __init__(self, data, token, report):
        self.data = Path(data).resolve()
        self.backup = self.data / ('.p1-originals-' + token)
        self.backup.mkdir(mode=0o700)
        self.entries, self.created_parents, self.report = [], [], report
        report.update(backup_directory=str(self.backup), files=self.entries)

    def journal(self):
        (self.backup / 'restore-journal.json').write_text(json.dumps(self.report, indent=2) + '\n')

    def safe(self, target):
        target = Path(target)
        require(target.is_relative_to(self.data / 'drives/c'), 'Destination outside C drive')
        current = target
        while current != self.data:
            require(not current.is_symlink(), 'Refusing symlink: ' + str(current))
            current = current.parent
        return target

    def isolated_by_parent(self, target):
        return any(e['directory'] and Path(target).is_relative_to(Path(e['target'])) for e in self.entries)

    def reserve(self, target, directory=False):
        target = self.safe(target)
        require(not any(target.is_relative_to(Path(e['target'])) for e in self.entries), 'Destination already reserved')
        before = identity(target)
        if before['exists']:
            require(target.is_dir() if directory else target.is_file(), 'Unexpected destination kind')
            if not directory:
                require(target.stat().st_nlink == 1, 'Refusing hard-linked original')
        parents, parent = [], target.parent
        while not parent.exists():
            parents.append(parent)
            parent = parent.parent
        require(parent.is_dir(), 'Destination ancestor is not a directory')
        for parent in reversed(parents):
            parent.mkdir(mode=0o700)
            self.created_parents.append(parent)
        saved = self.backup / str(len(self.entries))
        self.entries.append({'target': str(target), 'before': before, 'backup': str(saved),
                             'directory': directory, 'restored': False})
        self.journal()
        if before['exists']:
            os.replace(target, saved)
        if directory:
            target.mkdir(mode=0o700)
        return target

    def restore(self):
        failures = []
        for entry in reversed(self.entries):
            target, saved = Path(entry['target']), Path(entry['backup'])
            try:
                self.safe(target)
                if entry['before']['exists']:
                    if not saved.exists():
                        # A failed/interrupted rename can leave the untouched original in place.
                        require(identity(target) == entry['before'], 'Original backup missing: ' + str(saved))
                        entry.update(after=identity(target), restored=True)
                        continue
                if target.exists():
                    if target.is_dir():
                        shutil.rmtree(target)
                    else:
                        target.unlink()
                if entry['before']['exists']:
                    os.replace(saved, target)
                after = identity(target)
                require(after == entry['before'], 'Restored identity differs: ' + str(target))
                entry.update(after=after, restored=True)
            except (OSError, RuntimeError) as error:
                entry['restore_error'] = str(error)
                failures.append(str(error))
        for parent in reversed(self.created_parents):
            try:
                parent.rmdir()
            except OSError:
                pass
        if not failures:
            (self.backup / 'restore-journal.json').unlink(missing_ok=True)
            self.backup.rmdir()
        else:
            self.journal()
        self.report['restored'] = not failures
        require(not failures, 'Restoration incomplete; originals remain in private backup: ' + '; '.join(failures))

class RuntimeSession:
    def __init__(self, args, suite, extra_inputs):
        self.args, self.output, self.data = args, args.output.resolve(), args.data.resolve()
        self.token = uuid.uuid4().hex
        self.host_log = self.output / 'emulator.log'
        self.process = self.deadline = self.log_stream = self.saved = None
        self.watches, self.inputs, self.locks = {}, {}, []
        self.result = {'status': 'FAIL', 'suite': suite, 'token': self.token,
            'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'forced_cleanup': False, 'normal_host_exit': False,
            'scope': 'Normal browser functionality; no diagnostic instrumentation or synthetic hooks',
            'inputs': self.inputs, 'restoration': {}, 'logs': {}}
        require(20 <= args.timeout <= 180, 'Timeout must be between 20 and 180 seconds')
        require(args.output.is_absolute() and not self.output.exists(), 'Use a fresh absolute --output directory')
        require(self.data == DATA, 'Runner is scoped to the recorded EKA DATA directory')
        require(not self.output.is_relative_to(self.data), 'Evidence output must be outside guest DATA')
        require(args.host_sha256 == HOST_SHA, 'Expected current C21 prepared host identity')
        for name, path, expected in (('browser', args.browser, args.browser_sha256), ('host', args.host, args.host_sha256)):
            require(re.fullmatch('[0-9a-f]{64}', expected) is not None, 'Expected lowercase SHA-256')
            path = path.resolve()
            require(not path.is_relative_to(self.data), 'Use external build inputs, not installed guest binaries')
            require(path.is_file() and sha256(path) == expected, name + ' SHA-256 mismatch')
            self.inputs[name] = identity(path)
        args.browser, args.host = args.browser.resolve(), args.host.resolve()
        # A browser and its UI resources are one candidate. Never borrow older resources.
        args.resource_dir = args.browser.parent
        pins = json.loads((ROOT / 'pinned-inputs.json').read_text())['files']
        for name in ('browser_reg.rsc', 'browser_loc.rsc', 'browser_ui.rsc'):
            path = args.resource_dir / name
            require(path.is_file() and path.stat().st_size == pins[name]['bytes']
                    and sha256(path) == pins[name]['sha256'], name + ' same-candidate resource identity mismatch')
            self.inputs[name] = identity(path)
        for name, path in extra_inputs.items():
            path = Path(path).resolve()
            require(not path.is_relative_to(self.data), 'Use external fixture/helper inputs')
            require(path.is_file(), 'Missing input: ' + str(path))
            if name != 'ui_config':
                require(name in pins and sha256(path) == pins[name]['sha256'], 'Pinned input mismatch: ' + name)
            self.inputs[name] = identity(path)
        self.result.update(runner_sha256=sha256(ROOT / 'runtime_support.py'), pins_sha256=sha256(ROOT / 'pinned-inputs.json'))

    def guest(self, target):
        return resolve_guest(self.data, target)

    @property
    def browser_log(self):
        return self.guest(r'C:\Data\WebKit\log.txt')

    def deploy(self, source, target):
        source, target = Path(source), self.guest(target)
        pinned = [item for item in self.inputs.values() if item['path'] == str(source.resolve())]
        require(pinned and identity(source.resolve()) == pinned[0], 'Deployment source is not an unchanged pinned input')
        if self.saved.isolated_by_parent(target):
            require(not target.exists(), 'Cannot overwrite this run\'s file')
            target.parent.mkdir(parents=True, exist_ok=True)
        else:
            self.saved.reserve(target)
        shutil.copy2(source, target)
        require(sha256(source) == sha256(target), 'Deployment bytes differ')
        self.result.setdefault('deployed', []).append({'source': identity(source), 'target': identity(target)})
        return target

    def fresh(self, target):
        target = self.guest(target)
        if self.saved.isolated_by_parent(target):
            require(not target.exists(), 'Fresh file already exists')
            target.parent.mkdir(parents=True, exist_ok=True)
        else:
            self.saved.reserve(target)
        return target

    def watch(self, target, output_name):
        require(Path(output_name).name == output_name and output_name not in self.watches, 'Log output name must be unique')
        self.watches[output_name] = self.guest(target)
        return self.watches[output_name]

    def remaining(self):
        require(self.deadline is not None, 'Host has not been launched')
        remaining = self.deadline - time.monotonic()
        require(remaining > 0, 'Total runtime deadline expired')
        return remaining

    def check_panic(self):
        bad = [str(p) for p in (self.host_log, *self.watches.values()) if PANIC.search(read(p))]
        require(not bad, 'Panic/access violation observed in ' + ', '.join(bad))

    def launch(self, guest_exe, arguments=''):
        require(self.process is None, 'Only one EKA instance is allowed')
        self.guest(guest_exe)
        self.verify_inputs()
        require(len(arguments.encode()) < 4095, 'Guest argument string too long')
        command = [str(self.args.host), '--device', 'RM-707', '--run', guest_exe]
        if arguments:
            command.append(arguments)
        self.log_stream = self.host_log.open('xb')
        self.deadline = time.monotonic() + self.args.timeout
        core_limit = resource.getrlimit(resource.RLIMIT_CORE)
        try:
            # Inherit disabled core dumps without preexec_fn (the HTTP fixture has a thread).
            resource.setrlimit(resource.RLIMIT_CORE, (0, core_limit[1]))
            self.process = subprocess.Popen(command, cwd=self.args.host.parent, stdout=self.log_stream,
                stderr=subprocess.STDOUT, start_new_session=True,
                pass_fds=tuple(lock.fileno() for lock in self.locks))
        finally:
            resource.setrlimit(resource.RLIMIT_CORE, core_limit)
        self.result.update(command=command, host_pid=self.process.pid,
            launched_monotonic=time.monotonic(), deadline_monotonic=self.deadline)
        return self.process

    def wait_until(self, predicate, timeout, label):
        end = min(self.deadline, time.monotonic() + timeout)
        while time.monotonic() < end:
            self.check_panic()
            value = predicate()
            if value:
                return value
            require(self.process.poll() is None, 'Host exited before ' + label)
            time.sleep(.1)
        raise RuntimeError('Timed out waiting for ' + label)

    def run_command(self, command, timeout=10):
        limit = min(timeout, self.remaining()) if self.deadline else timeout
        require(0 < limit <= 180, 'Invalid subprocess deadline')
        try:
            result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=limit)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError('Subprocess deadline expired: ' + str(command[0])) from error
        self.result.setdefault('subprocesses', []).append({'command': [str(x) for x in command],
            'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        require(result.returncode == 0, 'Subprocess failed: ' + str(command[0]))
        return result

    def screenshot(self, name):
        require(Path(name).name == name and name.endswith('.png'), 'Invalid screenshot name')
        path = self.output / name
        require(not path.exists(), 'Screenshot must be fresh')
        helper = REPO / 'Tools/Symbian/tests/belle-ui/native-interact.py'
        expected = json.loads((ROOT / 'pinned-inputs.json').read_text())['files']['native_input']['sha256']
        require(sha256(helper) == expected, 'Native input helper changed')
        spec = importlib.util.spec_from_file_location('p1_native_input', helper)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        device = module.XInput(self.process.pid)
        try:
            geometry = device.check()
            self.run_command(['import', '-window', hex(device.window), str(path)], timeout=4)
        finally:
            device.close()
        from PIL import Image, ImageStat
        with Image.open(path) as image:
            image.load()
            require(image.width >= 200 and image.height >= 200, 'Screenshot too small')
            extrema, deviation = image.convert('RGB').getextrema(), ImageStat.Stat(image.convert('RGB')).stddev
            require(max(hi - lo for lo, hi in extrema) >= 32 and max(deviation) >= 5, 'Screenshot is blank')
            record = {**identity(path), 'width': image.width, 'height': image.height,
                'channel_stddev': deviation, 'window_geometry': geometry,
                'host_pid': self.process.pid, 'captured_monotonic': time.monotonic(),
                'nonblank': True, 'visual_page_review_required': True}
        self.result.setdefault('screenshots', []).append(record)
        return record

    def finish_normal(self):
        require(self.process is not None, 'Host was never launched')
        while self.process.poll() is None:
            self.remaining()
            self.check_panic()
            time.sleep(.1)
        self.check_panic()
        require(self.process.returncode == 0, 'Host exited with ' + str(self.process.returncode))
        self.result.update(host_exit=0, normal_host_exit=True, shutdown='Natural CLI guest-exit callback; no WM close')

    def _cleanup_process(self):
        if self.process and self.process.poll() is None:
            self.result['forced_cleanup'] = True
            for sig in (signal.SIGTERM, signal.SIGKILL):
                if self.process.poll() is not None:
                    break
                try:
                    os.killpg(self.process.pid, sig)
                except ProcessLookupError:
                    pass
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    continue
            require(self.process.poll() is not None, 'Owned host survived bounded cleanup; keep originals in backup')
        if self.process:
            self.result['host_exit'] = self.process.poll()
        if self.log_stream:
            self.log_stream.close()

    def verify_inputs(self):
        for name, before in self.inputs.items():
            require(identity(Path(before['path'])) == before, 'Input changed during run: ' + name)

    def execute(self, callback):
        require(Path(os.environ.get('XDG_DATA_HOME', '/invalid')).resolve() / 'EKA2L1/data' == self.data,
                'Source /workspace/eka-validation/env.sh')
        require(os.environ.get('PULSE_SERVER') == 'unix:/workspace/symbian-media-work/runtime/native',
                'Source /workspace/symbian-media-work/env.sh')
        require(bool(os.environ.get('DISPLAY')), 'DISPLAY must be configured')
        require(shutil.which('import') is not None, 'ImageMagick import is required')
        self.output.mkdir(parents=True, exist_ok=False, mode=0o700)
        previous_handlers = {}
        def interrupted(signum, frame):
            raise RuntimeError('Runner cancelled by signal ' + str(signum))
        for sig in (signal.SIGINT, signal.SIGTERM):
            previous_handlers[sig] = signal.signal(sig, interrupted)
        try:
            with contextlib.ExitStack() as stack:
                for name in ('.jsc-run.lock', '.browser-run.lock'):
                    lock = stack.enter_context((self.data / name).open('a'))
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    self.locks.append(lock)
                active = subprocess.run(['pgrep', '-x', 'eka2l1_qt'], capture_output=True, timeout=3)
                require(active.returncode == 1, 'An EKA host is already running; do not share runtime')
                self.verify_inputs()
                self.saved = SavedFiles(self.data, self.token, self.result['restoration'])
                try:
                    self.saved.reserve(self.guest(r'C:\Data\WebKit'), directory=True)
                    self.deploy(self.args.browser, r'C:\sys\bin\browser.exe')
                    for name, target in (
                        ('browser_reg.rsc', r'C:\private\10003a3f\import\apps\browser_reg.rsc'),
                        ('browser_loc.rsc', r'C:\resource\apps\browser_loc.rsc'),
                        ('browser_ui.rsc', r'C:\resource\apps\browser_ui.rsc')):
                        self.deploy(self.args.resource_dir / name, target)
                    self.watch(r'C:\Data\WebKit\log.txt', 'browser.log')
                    self.watch(r'C:\Data\WebKit\log.old.txt', 'browser-old.log')
                    self.watch(r'C:\Data\WebKit\curl.txt', 'curl.log')
                    callback(self)
                    require(self.result['normal_host_exit'] and not self.result['forced_cleanup'],
                            'Normal host termination was not established')
                    self.verify_inputs()
                    self.result['status'] = 'PASS'
                finally:
                    for sig in previous_handlers:
                        signal.signal(sig, signal.SIG_IGN)
                    # Do not restore files while an owned host may still be using them.
                    self._cleanup_process()
                    try:
                        for name, path in self.watches.items():
                            if path.is_file():
                                output = self.output / name
                                output.write_bytes(path.read_bytes())
                                self.result['logs'][name] = identity(output)
                    finally:
                        self.saved.restore()
        except (Exception, KeyboardInterrupt) as error:
            self.result.update(status='FAIL', error=str(error) or 'Interrupted')
        finally:
            for sig, handler in previous_handlers.items():
                signal.signal(sig, handler)
            self.result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            self.result['host_log'] = identity(self.host_log)
            self.result['runtime_locks_released'] = not self.process or self.process.poll() is not None
            if self.result['forced_cleanup']:
                self.result['status'] = 'FAIL'
            (self.output / 'result.json').write_text(json.dumps(self.result, indent=2) + '\n')
        return 0 if self.result['status'] == 'PASS' else 1

def run_main(args, suite, extra_inputs, callback):
    try:
        session = RuntimeSession(args, suite, extra_inputs)
        if not args.execute:
            print(json.dumps({'status': 'PREPARED_NOT_EXECUTED', 'suite': suite,
                'output_will_be': str(session.output), 'inputs': session.inputs,
                'requires_exclusive_runtime': True}, indent=2))
            return 0
        code = session.execute(callback)
        print(json.dumps({'status': session.result['status'], 'output': str(session.output),
                          'error': session.result.get('error')}, indent=2))
        return code
    except (Exception, KeyboardInterrupt) as error:
        print('FAIL: ' + (str(error) or 'Interrupted'), file=sys.stderr)
        return 1

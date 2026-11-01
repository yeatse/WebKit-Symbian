#!/usr/bin/env python3
"""Root-owned ordinary downloads. Every input needs a current screenshot review."""
from pathlib import Path
import argparse, datetime as dt, fcntl, hashlib, importlib.util
import json, os, re, shutil, signal, subprocess, sys, time, uuid

WORK = Path('/workspace/symbian-download-runtime-work')
REPO = Path('/workspace/WebKit-Symbian-graphics')
DATA = Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
SUPPORT = Path('/workspace/symbian-p1-system-runtime-work/runtime_support.py')
TERMINAL = re.compile(r'download (\d+): terminal=(\d+) error=(-?\d+) cleanup=(-?\d+)')
CASES = ('get', 'gzip', 'chunked', 'cancel-transfer', 'existing')

def require(ok, message):
    if not ok: raise RuntimeError(message)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def write(path, value):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    tmp.replace(path)

def read(path):
    return path.read_text(errors='replace') if path.is_file() else ''

def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def validate_inputs(args):
    require(args.output.is_absolute() and not args.output.exists(), 'output must be fresh and absolute')
    require(not args.output.resolve().is_relative_to(DATA), 'evidence must be outside guest DATA')
    require(60 <= args.timeout <= 600, 'timeout must be 60..600 seconds')
    inputs = {}
    for label in ('browser', 'host'):
        path = getattr(args, label).resolve()
        expected = getattr(args, label + '_sha256')
        require(re.fullmatch('[0-9a-f]{64}', expected) and path.is_file() and sha(path) == expected, label + ' identity mismatch')
        setattr(args, label, path)
        inputs[label] = {'path': str(path), 'sha256': sha(path)}
    for name in ('browser_ui.rsc', 'browser_reg.rsc', 'browser_loc.rsc'):
        path = args.browser.parent / name
        require(path.is_file(), 'missing adjacent candidate resource: ' + name)
        inputs[name] = {'path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}
    for path in (WORK/'fixture.py', WORK/'native.py', WORK/'snapshot.py', WORK/'validate.py',
                 REPO/'Tools/Symbian/eka2l1/run-browser.sh', SUPPORT,
                 Path(__file__).resolve(), Path(__file__).resolve().parent/'control.py'):
        require(path.is_file(), 'missing runtime dependency: ' + str(path))
        inputs[path.name] = {'path': str(path), 'sha256': sha(path)}
    return inputs

class Observer:
    def __init__(self, args, inputs):
        self.a, self.inputs, self.out = args, inputs, args.output
        abbreviation = {'get': 'g', 'gzip': 'z', 'chunked': 'h', 'cancel-transfer': 'x', 'existing': 'e'}
        self.token = 'dl-' + abbreviation[args.case] + '-' + uuid.uuid4().hex[:12]
        self.guest_directory = 'C:\\Data\\DownloadRuntime\\' + self.token + '\\'
        self.destination = DATA / 'drives/c/Data/DownloadRuntime' / self.token
        self.live_log = DATA / 'drives/c/Data/WebKit/log.txt'
        self.state = {'status': 'STARTING', 'run': self.token, 'case': args.case,
            'command_sequence': 0, 'native_reviewed': False, 'inputs': inputs,
            'destination': str(self.destination), 'guest_directory': self.guest_directory,
            'native_steps': [], 'forced_cleanup': None}
        self.browser = self.fixture = self.saved = self.config_backup = None
        self.streams, self.locks = [], []
        self.ready = self.current_capture = None
        self.number, self.capture_number = 1, 0
        self.ui_verified, self.selected_path = False, None

    def save(self):
        write(self.out/'state.json', self.state)

    def process(self, command, log, env=None, pass_fds=()):
        stream = log.open('w'); self.streams.append(stream)
        return subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT, env=env, pass_fds=pass_fds)

    def native(self, operation, output, extra=()):
        command = [sys.executable, '-B', str(WORK/'native.py'), operation,
            '--pid', str(self.state['pid']), '--run', self.token, '--case', self.a.case,
            '--output', str(output), *extra]
        with (self.out/(output.name + '-driver.log')).open('w') as stream:
            subprocess.run(command, check=True, stdout=stream, stderr=subprocess.STDOUT, timeout=40)

    def capture(self):
        self.capture_number += 1
        directory = self.out/f'capture-{self.capture_number:03d}'
        self.native('capture', directory)
        self.current_capture = directory/'calibration.json'
        capture = json.loads(self.current_capture.read_text())
        self.state.update(status='WAITING_FOR_REVIEWED_INPUT', capture=str(self.current_capture),
            capture_sha256=sha(self.current_capture),
            screenshots=[str(directory/item['path']) for item in capture['screenshots']])
        self.save()

    def action(self, value):
        require(self.current_capture is not None, 'no current native capture')
        require(value.get('capture_sha256') == sha(self.current_capture), 'stale capture approval')
        require(value.get('screenshots_reviewed') is True, 'root must review current screenshots')
        path = self.out/f'plan-{self.number:03d}.json'
        write(path, {'calibration_sha256': sha(self.current_capture),
                     'screenshots_reviewed': True, 'actions': value['actions']})
        module(WORK/'native.py', 'download_native_plan').load_plan(path, self.current_capture)
        directory = self.out/f'input-{self.number:03d}'
        self.native('act', directory, ['--calibration', str(self.current_capture), '--plan', str(path)])
        self.state['native_steps'].append(str(directory/'interaction.json'))
        self.capture()

    def release(self):
        require(self.a.case == 'cancel-transfer', 'release is only for this local slow fixture')
        text = read(self.live_log); terminals = TERMINAL.findall(text)
        require(len(terminals) == 1 and tuple(map(int, terminals[0][1:])) == (1, 0, 0),
                'release requires the actual cancelled terminal callback first')
        preceding = text[:TERMINAL.search(text).start()]
        require(re.search(r'download '+terminals[0][0]+r': [1-9][0-9]*/-?\d+ bytes', preceding),
                'cancel-transfer requires positive progress for the same ID')
        import urllib.request
        request = urllib.request.Request(self.ready['host_control_url']+'/release?run='+self.token,
                                         method='POST', data=b'')
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(request, timeout=5) as response: require(response.status == 200, 'gate release failed')
        self.state['slow_gate_released'] = True

    def control(self, value):
        require(value.get('run') == self.token and value.get('sequence') == self.number, 'control identity mismatch')
        operation = value.get('operation')
        if operation in ('act', 'capture'):
            require(self.browser.poll() is None, 'browser already ended')
            self.action(value) if operation == 'act' else self.capture()
        elif operation == 'release': self.release()
        elif operation == 'review-result':
            require(value.get('native_ui_reviewed') is True, 'root must view final native UI')
            terminals = TERMINAL.findall(read(self.live_log))
            expected = 0 if self.a.case in ('get', 'gzip', 'chunked') else 1
            require(len(terminals) == 1 and tuple(map(int, terminals[0][1:])) == (expected, 0, 0),
                    'final native review requires the matching terminal callback first')
            path = value.get('selected_guest_path')
            require(isinstance(path, str) and path.casefold() == (self.guest_directory+'payload.bin').casefold(),
                    'confirm the exact selected path from the actual native UI')
            self.ui_verified, self.selected_path = True, path
            self.state['native_reviewed'] = True
        else: raise RuntimeError('unknown control operation')
        self.state['command_sequence'] = self.number
        self.number += 1; self.save()

    def snapshot(self, name):
        subprocess.run([sys.executable, str(WORK/'snapshot.py'), '--directory', str(self.destination),
            '--guest-directory', self.guest_directory, '--run', self.token, '--case', self.a.case,
            '--output', str(self.out/(name+'.json')), '--copy-to', str(self.out/(name+'-files'))],
            check=True, stdout=subprocess.DEVNULL)

    def setup(self):
        self.out.mkdir(mode=0o700, parents=True); (self.out/'commands').mkdir(); self.save()
        require(subprocess.run(['pgrep','-x','eka2l1_qt'], stdout=subprocess.DEVNULL).returncode != 0,
                'another EKA is active; root owns runtime exclusively')
        for name in ('.jsc-run.lock', '.browser-run.lock'):
            lock=(DATA/name).open('a'); fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB); self.locks.append(lock)
        self.saved = module(SUPPORT, 'download_preservation_support').SavedFiles(
            DATA, self.token, self.state.setdefault('restoration', {}))
        self.saved.reserve(DATA/'drives/c/Data/WebKit', directory=True)
        for name in ('sys/bin/browser.exe', 'private/10003a3f/import/apps/browser_reg.rsc',
                     'resource/apps/browser_loc.rsc', 'resource/apps/browser_ui.rsc',
                     'resource/icu/icudt70l.dat', 'resource/fontconfig/fonts.conf'):
            self.saved.reserve(DATA/'drives/c'/name)
        self.saved.reserve(self.destination, directory=True)
        if self.a.case == 'existing':
            (self.destination/'payload.bin').write_bytes(b'ordinary-download-existing-sentinel\n'+self.token.encode()+b'\n')
        self.snapshot('before')
        config=DATA.parent/'config.yml'
        require(config.is_file() and not config.is_symlink(), 'expected existing EKA config')
        private=self.out/'config.private.yml'
        self.config_backup=(config, private, config.stat().st_ino, sha(config))
        config.rename(private)
        shutil.copy2(private, config)
        self.fixture=self.process([sys.executable, '-B', str(WORK/'fixture.py'), '--port', '0',
            '--run', self.token, '--case', self.a.case, '--gate-seconds', '300',
            '--output', str(self.out/'fixture')], self.out/'fixture.log')
        until=time.monotonic()+10
        while not (self.out/'fixture/ready.json').is_file():
            require(time.monotonic()<until and self.fixture.poll() is None, 'fixture did not become ready')
            time.sleep(.1)
        self.ready=json.loads((self.out/'fixture/ready.json').read_text())
        env=os.environ.copy(); env.update(EKA2L1=str(self.a.host), ICU_DIR='/workspace/symbian-idna-work/icu',
            SYMBIAN_WEBKIT_WORK=str(self.out/'browser'), WAIT=str(self.a.timeout), EKA2L1_COMPLETION_GRACE='12',
            EKA2L1_EXPECT_PASS='terminal='+('0' if self.a.case in ('get','gzip','chunked') else '1')+' error=0 cleanup=0',
            EKA2L1_BROWSER_LOCK_FD=str(self.locks[1].fileno()))
        self.browser=self.process(['bash', str(REPO/'Tools/Symbian/eka2l1/run-browser.sh'),
            str(self.a.browser), self.ready['url']], self.out/'runner.log', env,
            tuple(lock.fileno() for lock in self.locks))
        until=time.monotonic()+45; marker='READY: belle-download-runtime '
        while True:
            require(time.monotonic()<until and self.browser.poll() is None, 'missing fresh fixture READY')
            rows=[line for line in read(self.live_log).splitlines() if marker in line]
            if any(json.loads(line.split(marker,1)[1]).get('run')==self.token for line in rows): break
            time.sleep(.2)
        pids=subprocess.check_output(['pgrep','-x','eka2l1_qt'],text=True).split()
        require(len(pids)==1, 'expected one EKA process')
        pid=int(pids[0]); require(Path(f'/proc/{pid}/exe').resolve()==self.a.host, 'host executable mismatch')
        ancestor=pid
        while ancestor>1 and ancestor!=self.browser.pid:
            ancestor=int(Path(f'/proc/{ancestor}/stat').read_text().split(') ',1)[1].split()[1])
        require(ancestor==self.browser.pid, 'EKA does not belong to this runner')
        self.state.update(pid=pid, url=self.ready['url'])
        runner_pid = self.standard_runner_pid()
        host_parent = int(Path(f'/proc/{pid}/stat').read_text().split(') ',1)[1].split()[1])
        require(runner_pid == host_parent, 'host is not a direct child of the owned standard Python runner')
        self.capture()

    def standard_runner_pid(self):
        """Find only our bash wrapper's standard Python here-document runner."""
        if self.browser.poll() is not None:
            return None
        parent = self.browser.pid
        children = []
        for process in Path('/proc').iterdir():
            if not process.name.isdecimal():
                continue
            try:
                owner = int((process/'stat').read_text().split(') ',1)[1].split()[1])
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                continue
            if owner == parent:
                children.append(int(process.name))
        matches = []
        for pid in [parent, *map(int, children)]:
            try:
                words = Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
            except FileNotFoundError:
                continue
            if (len(words) >= 4 and Path(os.fsdecode(words[0])).name.startswith('python')
                    and words[1] == b'-' and Path(os.fsdecode(words[2])).resolve() == self.a.browser
                    and os.fsdecode(words[3]) == self.ready['url']):
                matches.append(pid)
        if not matches and self.browser.poll() is not None:
            return None
        require(len(matches) == 1, 'cannot identify the owned standard Python runner; do not signal the guest')
        previous = self.state.get('runner_signal_pid')
        require(previous is None or previous == matches[0], 'standard runner identity changed')
        self.state['runner_signal_pid'] = matches[0]
        return matches[0]

    def request_runner_close(self, reason):
        target = self.standard_runner_pid()
        if target is not None:
            self.state.update(runner_cancel_requested=True, runner_cancel_reason=reason)
            self.save()
            # Sending SIGTERM to the bash wrapper alone does not forward it to
            # this Python handler. The owned handler performs normal Qt close.
            try:
                os.kill(target, signal.SIGTERM)
            except ProcessLookupError:
                pass
        self.browser.wait(timeout=30)

    def check_terminal_failure(self):
        """Stop observation on real callback errors, before accepting more UI input."""
        terminals = TERMINAL.findall(read(self.live_log))
        expected = 0 if self.a.case in ('get', 'gzip', 'chunked') else 1
        if not terminals or (len(terminals) == 1 and tuple(map(int, terminals[0][1:])) == (expected, 0, 0)):
            return False
        self.state.update(status='FAIL', functional_failure={
            'reason': 'duplicate terminal callbacks' if len(terminals) > 1 else 'unexpected terminal callback',
            'expected': {'terminal': expected, 'error': 0, 'cleanup': 0},
            'observed': [{'id': int(identifier), 'terminal': int(failure),
                          'error': int(error), 'cleanup': int(cleanup)}
                         for identifier, failure, error, cleanup in terminals]})
        if self.browser.poll() is None:
            self.request_runner_close('functional download failure')
        self.save()
        return True

    def observe(self):
        deadline=time.monotonic()+self.a.timeout+50; ended=None
        while time.monotonic()<deadline:
            if self.check_terminal_failure():
                break
            command=self.out/'commands'/f'{self.number:03d}.json'
            if command.is_file():
                try: self.control(json.loads(command.read_text()))
                except (RuntimeError, ValueError, KeyError, subprocess.SubprocessError) as error:
                    self.state.update(control_error=str(error), status='INPUT_STOPPED_REQUIRES_REVIEW'); self.save()
                    self.browser.wait(timeout=self.a.timeout+30); break
            if self.browser.poll() is not None:
                if ended is None:
                    ended=time.monotonic(); self.state['status']='WAITING_FINAL_NATIVE_REVIEW'; self.save()
                if self.ui_verified or time.monotonic()-ended>45: break
            time.sleep(.1)
        require(self.browser.poll() is not None, 'runner exceeded its bounded lifetime')
        # Include callbacks written just before the runner finished.
        self.check_terminal_failure()
        self.state['launcher_exit']=self.browser.returncode
        self.snapshot('after')
        directories=list((self.out/'browser/runs').glob('browser-*'))
        require(len(directories)==1, 'expected exactly one new browser run')
        directory=directories[0]; report={key:self.ready[key] for key in ('run','case','url')}
        normal=self.browser.returncode==0 and 'Browser observation completed' in read(self.out/'runner.log')
        self.state['forced_cleanup'] = False if normal else None
        report.update(host_exit_code=0 if normal else None, normal_close=normal, forced_cleanup=False if normal else None,
            browser_sha256=self.inputs['browser']['sha256'], emulator_sha256=self.inputs['host']['sha256'],
            finished_utc=dt.datetime.now(dt.timezone.utc).isoformat(), native_ui_reviewed=self.ui_verified,
            selected_guest_path=self.selected_path,
            functional_failure=self.state.get('functional_failure'))
        write(self.out/'runner-report.json', report)
        cmd=[sys.executable, str(WORK/'validate.py'), '--fixture', str(self.out/'fixture'),
            '--before', str(self.out/'before.json'), '--after', str(self.out/'after.json'),
            '--destination-name', 'payload.bin', '--guest-log', str(directory/'log.txt'),
            '--emulator-log', str(directory/'emulator.log'), '--runner-report', str(self.out/'runner-report.json'),
            '--output', str(self.out/'validation.json')]
        for interaction in self.state['native_steps']: cmd+=['--interaction', interaction]
        with (self.out/'validation.log').open('w') as stream:
            code=subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT).returncode
        result=json.loads((self.out/'validation.json').read_text())
        for identity in self.inputs.values():
            require(sha(identity['path']) == identity['sha256'], 'immutable input changed during observation: '+identity['path'])
        if self.state.get('functional_failure') or result['status'] == 'FAIL':
            status = 'FAIL'
        else:
            status = result['status'] if self.ui_verified else 'INCOMPLETE'
        self.state.update(validator_exit=code, validation_status=result['status'], status=status); self.save()

    def restore(self):
        # A first cancellation can arrive after observation ended, too. Finish
        # unlink/rename restoration without any signal interrupting it midway.
        for number in (signal.SIGTERM, signal.SIGINT):
            signal.signal(number, signal.SIG_IGN)
        errors=[]
        if not any((self.browser, self.fixture, self.saved, self.config_backup,
                    self.streams, self.locks)):
            # Output creation can fail before any runtime state was touched.
            # Do not launch pgrep or attempt to save into a nonexistent parent.
            self.state.update(restoration_attempted=False, restoration_errors=[])
            if self.out.is_dir():
                self.save()
            return
        if self.browser is not None and self.browser.poll() is None:
            # Signal the existing runner, whose handler requests ordinary Qt
            # close and records cancellation as failure. Never signal the guest.
            try: self.request_runner_close('observer cleanup')
            except (RuntimeError, subprocess.TimeoutExpired) as error:
                errors.append('runner active; private originals left in backup: '+str(error))
        guest_active = subprocess.run(['pgrep', '-x', 'eka2l1_qt'], stdout=subprocess.DEVNULL).returncode == 0
        self.state['no_guest_running_at_restore'] = not guest_active
        if guest_active:
            errors.append('guest still active; private originals left in backup')
        if (self.browser is None or self.browser.poll() is not None) and not guest_active:
            if self.fixture is not None and self.fixture.poll() is None:
                self.fixture.send_signal(signal.SIGINT)
                try: self.fixture.wait(timeout=8)
                except subprocess.TimeoutExpired: errors.append('local fixture did not stop')
            if self.config_backup:
                config,private,inode,before=self.config_backup
                if private.exists():
                    config.unlink(missing_ok=True); private.rename(config)
                self.state['config_restored']=config.stat().st_ino==inode and sha(config)==before
                if not self.state['config_restored']: errors.append('config restore mismatch')
            if self.saved:
                try: self.saved.restore()
                except RuntimeError as error: errors.append(str(error))
        self.state['restoration_errors']=errors
        for stream in self.streams: stream.close()
        for lock in self.locks: lock.close()
        if self.out.is_dir():
            self.save()
        require(not errors, '; '.join(errors))

def execute_observer(observer):
    """Keep the original setup/observation error if cleanup also fails."""
    primary = cleanup = None
    try:
        observer.setup()
        observer.observe()
    except Exception as error:
        primary = error
    finally:
        try:
            observer.restore()
        except Exception as error:
            cleanup = error
    if primary is not None or cleanup is not None:
        error = primary if primary is not None else cleanup
        observer.state.update(status='FAIL', error=str(error), error_type=type(error).__name__)
        if primary is not None:
            observer.state['primary_error'] = {'type': type(primary).__name__, 'message': str(primary)}
        if cleanup is not None:
            observer.state['cleanup_error'] = {'type': type(cleanup).__name__, 'message': str(cleanup)}
        # Reporting failures must not replace the original error either.
        if observer.out.is_dir():
            try:
                observer.save()
            except Exception as report_error:
                observer.state['state_report_error'] = str(report_error)
        print('FAIL: '+type(error).__name__+': '+str(error), file=sys.stderr)
        if cleanup is not None and primary is not None:
            print('Cleanup also failed: '+type(cleanup).__name__+': '+str(cleanup), file=sys.stderr)
        if observer.state.get('state_report_error'):
            print('State report unavailable: '+observer.state['state_report_error'], file=sys.stderr)
        return 1
    return 0 if observer.state['status']=='PASS' else 2 if observer.state['status']=='INCOMPLETE' else 1

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=CASES, required=True)
    parser.add_argument('--browser', type=Path, required=True)
    parser.add_argument('--browser-sha256', required=True)
    parser.add_argument('--host', type=Path, default=Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt'))
    parser.add_argument('--host-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--execute', action='store_true')
    args=parser.parse_args(); observer=None
    def interrupted(signum, _frame):
        for number in (signal.SIGTERM, signal.SIGINT): signal.signal(number, signal.SIG_IGN)
        raise RuntimeError('root download observer cancelled by signal '+str(signum))
    if args.execute:
        for number in (signal.SIGTERM, signal.SIGINT): signal.signal(number, interrupted)
    try:
        inputs=validate_inputs(args)
        if not args.execute:
            print(json.dumps({'status':'PREPARED_NOT_EXECUTED', 'inputs':inputs,
                              'case':args.case, 'output':str(args.output)}, indent=2)); return 0
        observer=Observer(args, inputs)
        return execute_observer(observer)
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('FAIL: '+str(error), file=sys.stderr); return 1

if __name__=='__main__': raise SystemExit(main())

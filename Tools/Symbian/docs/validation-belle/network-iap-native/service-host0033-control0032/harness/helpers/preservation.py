#!/usr/bin/env python3
"""Prepare-only by default. Root may later run one isolated baseline native probe."""
import argparse
import contextlib
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
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parent
DATA = Path('/workspace/eka-validation/xdg/data/EKA2L1/data')
BASE = DATA.parent
QT = Path('/workspace/eka-validation/xdg/config/EKA2L1')
HOST = Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
PROBE_ROOT = Path('/workspace/symbian-iap-proxy-current-review/native-probe')
PROBE = PROBE_ROOT / 'build/e32/iap_proxy_readonly_probe.exe'
CONFIG_HELPER = Path('/workspace/symbian-cookie-store-runtime-c21-work/config_preservation.py')
RESERVE = 512 * 1024 * 1024
DEADLINE = 60
MAX_NODES = 1000000
DEVICE = 'RM-707'
PINS = {
    HOST: '692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd',
    PROBE: 'a66c6b58f3b8c497af76118175acab13fb470c54c8f00aee51baa5a0bcab3761',
    PROBE_ROOT / 'probe.cpp': '2c2d7c0c86ed193aa7e9b9e5580875bfb794bef67022adac6ca8a10c0720ff0c',
    PROBE_ROOT / 'validate.py': '94198a9221c3f77f0067226a7bf321ee99a2620cff1425cb59c7213b4837ea80',
    CONFIG_HELPER: 'e6679c12d66fadb4ad6c69f46f0deede57470f379d5f64591d1e6c312b8015d0',
}
FATAL = re.compile(r'access violation|\bpanicked\b|KERN-EXEC|USER-EXEC|segmentation fault|\bSIGSEGV\b|\bSIGABRT\b|Main loop exited with exception|Unable to launch process|Thread .* (?:forcefully killed|terminated peacefully) with category:.*exit code: -?[1-9][0-9]*', re.I)


class Refusal(Exception):
    """Only fixed public codes may cross the privacy boundary."""


def require(ok, code):
    if not ok:
        raise Refusal(code)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def canonical(path, exists=True):
    path = Path(path)
    require(path.is_absolute() and path == path.resolve(), 'UNSAFE_PATH')
    for item in [path, *path.parents]:
        require(not item.is_symlink(), 'SYMLINK_PATH')
    require(not exists or path.exists(), 'MISSING_PATH')
    return path


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check_pins():
    for path, expected in PINS.items():
        canonical(path)
        require(path.is_file() and digest(path) == expected, 'PIN_CHANGED')
    evidence = json.loads((ROOT / 'storage-source-pins.json').read_text())
    for row in evidence:
        path = canonical(Path(row['path']))
        require(digest(path) == row['sha256'], 'STORAGE_SOURCE_CHANGED')
    own = json.loads((ROOT / 'wrapper-pins.json').read_text())
    require(set(own) == {'run.py', 'test_guards.py', 'storage-source-pins.json'}, 'WRAPPER_PIN_SCHEMA_CHANGED')
    for name, expected in own.items():
        require(digest(canonical(ROOT / name)) == expected, 'WRAPPER_SOURCE_CHANGED')
    return True


def no_host():
    require(subprocess.run(['pgrep', '-x', 'eka2l1_qt'], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=3).returncode == 1,
            'HOST_PRESENT_OR_CHECK_FAILED')


def capacity_ok(free, needed=0):
    return type(free) is int and type(needed) is int and needed >= 0 and free >= needed + RESERVE


def root_identity(meta):
    return {key: meta[key] for key in ('device', 'inode', 'mode', 'mtime_ns')}


def metadata(path):
    value = Path(path).lstat()
    return {'device': value.st_dev, 'inode': value.st_ino,
            'mode': stat.S_IMODE(value.st_mode), 'mtime_ns': value.st_mtime_ns,
            'atime_ns': value.st_atime_ns, 'size': value.st_size,
            'blocks': value.st_blocks, 'nlink': value.st_nlink,
            'directory': stat.S_ISDIR(value.st_mode),
            'regular': stat.S_ISREG(value.st_mode),
            'symlink': stat.S_ISLNK(value.st_mode)}


def node_safe(row, device):
    return (row['device'] == device and not row['symlink']
            and (row['directory'] or (row['regular'] and row['nlink'] == 1)))


def scan_tree(root, device):
    """Private metadata only; never print or publish the inventory."""
    root = canonical(root)
    rows = []
    pending = [(root, '.')]
    # os.path.ismount cannot detect every same-device bind mount on Linux.
    # Parse mountpoints internally and reject any mount inside the source tree.
    mounts = set()
    for line in Path('/proc/self/mountinfo').read_text().splitlines():
        field = line.split()[4]
        field = re.sub(r'\\([0-7]{3})', lambda match: chr(int(match.group(1), 8)), field)
        point = Path(field)
        if point == root or point.is_relative_to(root):
            mounts.add(point)
    require(not mounts, 'MOUNT_INSIDE_DRIVE_REFUSED')
    allocated = apparent = 0
    while pending:
        path, relative = pending.pop()
        row = metadata(path)
        require(node_safe(row, device), 'UNSAFE_DRIVE_NODE')
        require(not path.is_mount() or path == root, 'NESTED_MOUNT_REFUSED')
        row['relative'] = relative
        rows.append(row)
        require(len(rows) <= MAX_NODES, 'DRIVE_NODE_LIMIT')
        allocated += row['blocks'] * 512
        if row['regular']:
            apparent += row['size']
        else:
            with os.scandir(path) as entries:
                for entry in entries:
                    pending.append((Path(entry.path), str(Path(relative) / entry.name)))
    require(rows[0]['directory'], 'DRIVE_NOT_DIRECTORY')
    return rows, max(allocated, apparent)


def strict_catalog(text):
    """Reject YAML features whose map/order semantics differ from yaml-cpp."""
    import yaml
    class StrictLoader(yaml.SafeLoader):
        def compose_node(self, parent, index):
            event = self.peek_event()
            require(not isinstance(event, yaml.AliasEvent) and getattr(event, 'anchor', None) is None,
                    'DEVICE_CATALOG_ALIAS_REFUSED')
            return super().compose_node(parent, index)

        def construct_mapping(self, node, deep=False):
            require(isinstance(node, yaml.MappingNode), 'DEVICE_CATALOG_UNSUPPORTED')
            result = {}
            for key_node, value_node in node.value:
                require(key_node.tag != 'tag:yaml.org,2002:merge', 'DEVICE_CATALOG_MERGE_REFUSED')
                key = self.construct_object(key_node, deep=deep)
                require(type(key) is str and key not in result, 'DEVICE_CATALOG_DUPLICATE_OR_NONSTRING_KEY')
                result[key] = self.construct_object(value_node, deep=deep)
            return result
    return yaml.load(text, Loader=StrictLoader)


def selected_scope():
    """Read the device catalog internally; never expose contents or identities."""
    catalog = canonical(DATA / 'devices.yml')
    require(catalog.is_file() and catalog.stat().st_size <= 4 * 1024 * 1024,
            'DEVICE_CATALOG_UNSAFE')
    value = strict_catalog(catalog.read_text())
    require(type(value) is dict and 0 < len(value) <= 256, 'DEVICE_CATALOG_UNSUPPORTED')
    selected = None
    for name, entry in value.items():
        require(type(name) is str and re.fullmatch(r'[A-Za-z0-9_-]+', name)
                and type(entry) is dict, 'DEVICE_CATALOG_UNSUPPORTED')
        require(entry.get('pending-deletion', False) is False, 'PENDING_DEVICE_DELETION')
        isolated = entry.get('isolated-drives', False)
        require(type(isolated) is bool, 'DEVICE_CATALOG_UNSUPPORTED')
        if isolated:
            require((DATA / 'drives' / name.lower()).is_dir(), 'OTHER_DEVICE_ROOT_WOULD_BE_CREATED')
        if name.lower() == DEVICE.lower():
            require(selected is None, 'AMBIGUOUS_DEVICE')
            selected = (name, entry, isolated)
    require(selected is not None, 'TARGET_DEVICE_ABSENT')
    name, entry, isolated = selected
    parent = DATA / 'drives'
    if isolated:
        parent /= DEVICE.lower()
    canonical(parent)
    # Fixed argv has no --mount or UI interaction. Pinned frontend mounts only these
    # three physical roots and Z as ROM+write-protected. Every physical root is isolated.
    roots = [parent / letter for letter in 'cde']
    require(all(path.parent == parent for path in roots), 'DRIVE_SCOPE_UNSUPPORTED')
    require((DATA / 'drives/z').is_dir(), 'ROM_ROOT_ABSENT')
    # The working catalog will contain ONLY this entry, at index zero. Counting
    # every original YAML entry is unsafe: host add_new_device may reject an
    # earlier device and shift its vector indices before startup mounts drives.
    return {name: entry}, roots


def private_json(path, value):
    path = Path(path)
    temp = path.with_name(path.name + '.tmp')
    fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(value, stream)
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def independent_copy(source, target):
    # copy2 is deliberately used instead of cp -a or any hard-link optimization.
    shutil.copy2(source, target, follow_symlinks=False)
    a, b = Path(source).stat(), Path(target).stat()
    require((a.st_dev, a.st_ino) != (b.st_dev, b.st_ino) and b.st_nlink == 1,
            'COPY_ALIASES_ORIGINAL')
    require(digest(source) == digest(target), 'COPY_BYTES_DIFFER')


class WholeDrives:
    def __init__(self, roots, private, journal):
        self.roots, self.private, self.journal = roots, private, journal
        self.entries = []

    def save(self):
        private_json(self.journal, {'schema': 'private-whole-drives-v1', 'entries': self.entries})

    def isolate(self, inventories):
        for index, (root, rows) in enumerate(zip(self.roots, inventories)):
            backup = self.private / ('drive-' + str(index))
            entry = {'target': str(root), 'backup': str(backup), 'original': rows,
                     'moved': False, 'owned_clone': None, 'restored': False}
            self.entries.append(entry)
            self.save()
            if rows:
                require(root_identity(metadata(root)) == root_identity(rows[0]), 'ORIGINAL_CHANGED')
                os.rename(root, backup)
                entry['moved'] = True
                self.save()
            root.mkdir(mode=0o700)
            entry['owned_clone'] = root_identity(metadata(root))
            self.save()
            if rows:
                # Inventories have already rejected every symlink, hardlink, mount,
                # special node and cross-device entry. copytree repeats its own checks.
                shutil.copytree(backup, root, dirs_exist_ok=True, symlinks=True,
                                copy_function=independent_copy)
                root.chmod(0o700)
                clone_rows, _ = scan_tree(root, rows[0]['device'])
                require(len(clone_rows) == len(rows), 'COPY_SHAPE_DIFFER')
                originals = {row['relative']: row for row in rows}
                for row in clone_rows:
                    original = originals.get(row['relative'])
                    require(original is not None and row['directory'] == original['directory'], 'COPY_SHAPE_DIFFER')
                    require((row['device'], row['inode']) != (original['device'], original['inode']), 'COPY_ALIASES_ORIGINAL')
                entry['owned_clone'] = root_identity(metadata(root))
                self.save()

    def restore(self, host_stopped):
        require(host_stopped, 'RESTORE_LIVE_HOST_REFUSED')
        # Check ALL roots before removing any clone or moving any original.
        for entry in self.entries:
            root, backup = Path(entry['target']), Path(entry['backup'])
            original = entry['original']
            if entry['moved']:
                require(backup.is_dir() and root_identity(metadata(backup)) == root_identity(original[0]), 'ORIGINAL_BACKUP_CHANGED')
            else:
                require(not backup.exists(), 'UNEXPECTED_ORIGINAL_BACKUP')
            if entry['owned_clone']:
                require(root.is_dir() and not root.is_symlink(), 'OWNED_CLONE_MISSING')
                require((metadata(root)['device'], metadata(root)['inode']) ==
                        (entry['owned_clone']['device'], entry['owned_clone']['inode']), 'CLONE_OWNERSHIP_LOST')
                scan_tree(root, entry['owned_clone']['device'])
            elif entry['moved']:
                require(not root.exists() and not root.is_symlink(), 'UNKNOWN_ROOT_REPLACEMENT')
            elif original:
                require(root_identity(metadata(root)) == root_identity(original[0]), 'UNMOVED_ORIGINAL_CHANGED')
        for entry in reversed(self.entries):
            root, backup = Path(entry['target']), Path(entry['backup'])
            if entry['owned_clone']:
                shutil.rmtree(root)
            if entry['moved']:
                os.rename(backup, root)
                require(root_identity(metadata(root)) == root_identity(entry['original'][0]), 'RESTORED_IDENTITY_DIFFER')
            else:
                require(bool(entry['original']) == root.exists(), 'RESTORED_ABSENCE_DIFFER')
            entry['restored'] = True
            self.save()
        return True


class HostHealth:
    """Classify stdout in memory; never persist arbitrary host output."""
    def __init__(self):
        self.fatal = False
        self.bound_exceeded = False
        self.total = 0

    def consume(self, stream):
        pending = b''
        try:
            while True:
                block = stream.read(4096)
                if not block:
                    break
                self.total += len(block)
                if self.total > 16 * 1024 * 1024:
                    self.bound_exceeded = True
                pending += block
                lines = pending.split(b'\n')
                pending = lines.pop()[-65536:]
                for line in lines:
                    self.classify(line)
            self.classify(pending)
        except OSError:
            self.bound_exceeded = True

    def classify(self, line):
        text = line.decode('utf-8', errors='replace')
        self.fatal |= FATAL.search(text) is not None


def public_result(result):
    # Never return the PrivateConfigurations report, journal, paths or inventories.
    keys = {'status', 'run', 'error_code', 'runtime_started', 'clone_started',
            'baseline_snapshot_only', 'commdb_storage_scope_confirmed', 'routing_verified',
            'natural_cli_exit', 'host_stopped', 'guest_drives_restored',
            'configuration_restored', 'inputs_unchanged', 'private_originals_retained',
            'private_journal_retained', 'normal_completion_evidence',
            'free_after_clone_at_least_512mib', 'snapshot'}
    return {key: value for key, value in result.items() if key in keys}


def fresh_profile(croot):
    # Entire C is a disposable clone. Remove only its cloned WebKit profile so the
    # only exported guest file is this run's fixed public snapshot, never old data.
    current = croot
    for component in ('Data', 'WebKit'):
        matches = [path for path in current.iterdir() if path.name.casefold() == component.casefold()]
        require(len(matches) <= 1, 'AMBIGUOUS_PROFILE_PATH')
        current = matches[0] if matches else current / component
        if not current.exists():
            current.mkdir(mode=0o700)
    require(current.is_dir() and not current.is_symlink(), 'UNSAFE_CLONED_PROFILE')
    shutil.rmtree(current)
    current.mkdir(mode=0o700)
    binary = croot
    for component in ('sys', 'bin', 'iap_proxy_readonly_probe.exe'):
        require(binary.is_dir(), 'UNSAFE_PROBE_DESTINATION')
        matches = [path for path in binary.iterdir() if path.name.casefold() == component.casefold()]
        require(len(matches) <= 1, 'AMBIGUOUS_PROBE_DESTINATION')
        binary = matches[0] if matches else binary / component
        if component != 'iap_proxy_readonly_probe.exe' and not binary.exists():
            binary.mkdir(mode=0o700)
    require(not binary.is_symlink(), 'UNSAFE_PROBE_DESTINATION')
    shutil.copy2(PROBE, binary)
    require(digest(binary) == PINS[PROBE], 'PROBE_DEPLOY_CHANGED')
    return current / 'iap-proxy-readonly.log'


def execute(args, token):
    result = {'status': 'FAIL', 'run': token, 'runtime_started': False, 'clone_started': False,
              'baseline_snapshot_only': True, 'commdb_storage_scope_confirmed': False,
              'routing_verified': False, 'natural_cli_exit': False, 'host_stopped': False,
              'guest_drives_restored': False, 'configuration_restored': False,
              'inputs_unchanged': False, 'private_originals_retained': False,
              'free_after_clone_at_least_512mib': False}
    require(Path(os.environ.get('XDG_DATA_HOME', '/invalid')).resolve() == BASE.parent,
            'SOURCE_ROOT_RUNTIME_ENVIRONMENTS')
    require(Path(os.environ.get('XDG_CONFIG_HOME', '/invalid')).resolve() == QT.parent,
            'SOURCE_ROOT_RUNTIME_ENVIRONMENTS')
    require(os.environ.get('DISPLAY') and os.environ.get('PULSE_SERVER') ==
            'unix:/workspace/symbian-media-work/runtime/native', 'SOURCE_ROOT_RUNTIME_ENVIRONMENTS')
    private = canonical(args.private_root, False)
    require(not private.exists() and not private.is_relative_to(BASE.parent.parent)
            and not private.is_relative_to(args.output) and not args.output.is_relative_to(private), 'UNSAFE_PRIVATE_ROOT')
    require(private.parent.stat().st_dev == DATA.stat().st_dev, 'PRIVATE_ROOT_CROSS_FILESYSTEM')
    configs = drives = host = monitor = None
    config_report = {}
    locks = []
    cleanup_ok = True
    signal_handlers = {}
    with contextlib.ExitStack() as stack:
        for name in ('.jsc-run.lock', '.browser-run.lock'):
            path = DATA / name
            require(not path.is_symlink(), 'UNSAFE_LOCK')
            stream = stack.enter_context(path.open('a'))
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            locks.append(stream)
        no_host()
        check_pins()
        working_catalog, roots = selected_scope()
        device = DATA.stat().st_dev
        inventories, needed = [], 0
        for root in roots:
            if root.exists() or root.is_symlink():
                rows, size = scan_tree(root, device)
            else:
                canonical(root, False)
                rows, size = [], 0
            inventories.append(rows)
            needed += size
        require(inventories[0], 'ORIGINAL_C_REQUIRED')
        # Copies plus configuration clone, logs and filesystem metadata need slack.
        require(capacity_ok(shutil.disk_usage(DATA).free, needed + 64 * 1024 * 1024), 'INSUFFICIENT_SPACE_BEFORE_CLONE')
        private.mkdir(mode=0o700)
        work = private / 'host-cwd'
        work.mkdir(mode=0o700)
        # EKA always creates its own log in cwd. Keep it private and never archive it.
        # Native replacement DLL lookup uses a known public artifact directory.
        (work / 'patch').symlink_to(HOST.parent / 'patch', target_is_directory=True)
        helper = load_module(CONFIG_HELPER, 'iap_private_configurations')
        configs = helper.PrivateConfigurations(private / 'config-originals',
            [(BASE / 'config.yml', False), (BASE / 'bindings', True), (QT, True),
             (DATA / 'devices.yml', False)], config_report)
        drives = WholeDrives(roots, private, private / 'restore-journal.json')
        def interrupted(number, _):
            raise Refusal('WRAPPER_INTERRUPTED')
        for sig in (signal.SIGINT, signal.SIGTERM):
            signal_handlers[sig] = signal.signal(sig, interrupted)
        try:
            configs.reserve_all()
            for path in (BASE / 'bindings', QT):
                if path.is_dir():
                    path.chmod(0o700)
            # Fresh known working config: no private proxy, hosts or mount settings
            # are read into wrapper output or inherited from the cloned config.
            controlled = {'data-storage': str(DATA), 'device': 0,
                          'log-filter': '*:Error Kernel:Trace', 'enable-gdb-stub': False}
            (BASE / 'config.yml').write_text(json.dumps(controlled) + '\n')
            (DATA / 'devices.yml').write_text(json.dumps(working_catalog) + '\n')
            result['clone_started'] = True
            drives.isolate(inventories)
            guest_log = fresh_profile(roots[0])
            require(capacity_ok(shutil.disk_usage(DATA).free), 'INSUFFICIENT_SPACE_AFTER_CLONE')
            result['free_after_clone_at_least_512mib'] = True
            check_pins()
            no_host()
            health = HostHealth()
            env = os.environ.copy()
            env.pop('DEBUG_CURL', None)
            env.pop('CURL_VERBOSE', None)
            old_limit = resource.getrlimit(resource.RLIMIT_CORE)
            try:
                resource.setrlimit(resource.RLIMIT_CORE, (0, old_limit[1]))
                host = subprocess.Popen([str(HOST), '--device', DEVICE, '--run',
                    r'C:\sys\bin\iap_proxy_readonly_probe.exe', ' --run ' + token],
                    cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    start_new_session=True, pass_fds=tuple(item.fileno() for item in locks))
            finally:
                resource.setrlimit(resource.RLIMIT_CORE, old_limit)
            result['runtime_started'] = True
            monitor = threading.Thread(target=health.consume, args=(host.stdout,), daemon=True)
            monitor.start()
            try:
                exit_code = host.wait(timeout=DEADLINE)
            except subprocess.TimeoutExpired:
                raise Refusal('NATIVE_DEADLINE_EXPIRED')
            monitor.join(timeout=3)
            require(not monitor.is_alive() and not health.bound_exceeded, 'HOST_OUTPUT_BOUND_FAILED')
            # The preserved Qt console option can disable stdout. EKA also writes
            # this known, new file in our private cwd; classify it without export.
            own_log = work / 'EKA2L1.log'
            require(own_log.is_file() and not own_log.is_symlink() and own_log.stat().st_size <= 16 * 1024 * 1024, 'OWNED_HOST_LOG_MISSING_OR_BOUND')
            with own_log.open('rb') as stream:
                for line in stream:
                    require(len(line) <= 65536, 'OWNED_HOST_LOG_LINE_BOUND')
                    health.classify(line)
            # Initial thread names are only "Main". A generic zero-exit thread
            # line is not target identity evidence. The fixed CLI callback exits
            # this host only when its target exits; the fixed fresh native record
            # below plus fixed CRT User::Exit(main result) and health gates supply
            # the ordinary baseline evidence, without claiming a named exit line.
            require(exit_code == 0 and not health.fatal, 'NATURAL_NATIVE_EXIT_NOT_VERIFIED')
            require(guest_log.is_file() and not guest_log.is_symlink() and guest_log.stat().st_size <= 16384, 'PUBLIC_SNAPSHOT_MISSING')
            text = guest_log.read_text()
            validator = load_module(PROBE_ROOT / 'validate.py', 'iap_public_snapshot_validator')
            snapshot = validator.validate(text, token)
            raw = json.loads(text.splitlines()[1][7:])
            require(raw['commdb_requested'] is False and raw['commdb_opened'] is False,
                    'COMMDB_MODE_NOT_AUTHORIZED_BY_STORAGE_SCOPE')
            result['snapshot'] = snapshot
            result['natural_cli_exit'] = True
            result['normal_completion_evidence'] = 'source_control_flow_plus_fresh_public_result_and_frontend_target_exit'
        except Refusal as error:
            result['error_code'] = str(error)
        except BaseException:
            result['error_code'] = 'UNEXPECTED_PRIVATE_FAILURE'
        finally:
            for sig in signal_handlers:
                signal.signal(sig, signal.SIG_IGN)
            stopped = host is None or host.poll() is not None
            if stopped:
                try:
                    no_host()
                except BaseException:
                    stopped = False
            result['host_stopped'] = stopped
            # No forced kill/WM close can be mistaken for natural CLI completion.
            # A live host keeps locks inherited and all originals/journals private.
            if stopped:
                try:
                    result['guest_drives_restored'] = drives.restore(True)
                except BaseException:
                    cleanup_ok = False
                try:
                    configs.restore()
                    result['configuration_restored'] = config_report.get('restored') is True
                except BaseException:
                    cleanup_ok = False
            else:
                cleanup_ok = False
            try:
                result['inputs_unchanged'] = check_pins()
            except BaseException:
                cleanup_ok = False
            result['private_originals_retained'] = not (cleanup_ok and result['guest_drives_restored'] and result['configuration_restored'])
            for sig, handler in signal_handlers.items():
                signal.signal(sig, handler)
        if not result.get('error_code') and cleanup_ok and result['guest_drives_restored'] and result['configuration_restored'] and result['inputs_unchanged']:
            result['status'] = 'BASELINE_SNAPSHOT_COMPLETE'
        else:
            result.setdefault('error_code', 'RESTORATION_OR_PIN_GATE_FAILED')
        # Private evidence is never copied to the public output. Remove this run's
        # directory only when no originals remain and all restore gates succeeded.
        result['private_journal_retained'] = result['status'] != 'BASELINE_SNAPSHOT_COMPLETE'
        if result['status'] == 'BASELINE_SNAPSHOT_COMPLETE':
            try:
                shutil.rmtree(private)
            except BaseException:
                result.update(status='FAIL', error_code='PRIVATE_CLEANUP_FAILED', private_journal_retained=True)
    return public_result(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--private-root', type=Path)
    args = parser.parse_args()
    # Existing helper journals use normal Python file creation. Keep every new
    # transaction/evidence file private even when Root's shell umask is wider.
    old_umask = os.umask(0o077)
    token = 'iap-' + uuid.uuid4().hex
    result = {'status': 'FAIL', 'run': token, 'runtime_started': False, 'clone_started': False,
              'routing_verified': False, 'commdb_storage_scope_confirmed': False}
    try:
        canonical(args.output, False)
        require(not args.output.exists() and not args.output.is_relative_to(BASE.parent.parent), 'FRESH_EXTERNAL_OUTPUT_REQUIRED')
        check_pins()
        if not args.execute:
            result.update(status='PREPARED_NOT_EXECUTED', baseline_snapshot_only=True,
                          natural_cli_exit=False, host_stopped=False, guest_drives_restored=False,
                          configuration_restored=False, inputs_unchanged=True,
                          private_originals_retained=False)
        else:
            require(args.private_root is not None, 'EXPLICIT_PRIVATE_ROOT_REQUIRED')
            result = execute(args, token)
        args.output.mkdir(mode=0o700)
        (args.output / 'result.json').write_text(json.dumps(public_result(result), indent=2) + '\n')
    except Refusal as error:
        result.update(status='FAIL', error_code=str(error))
    except BaseException:
        result.update(status='FAIL', error_code='UNEXPECTED_PRIVATE_FAILURE')
    finally:
        os.umask(old_umask)
    print(json.dumps(public_result(result), indent=2))
    return 0 if result['status'] in ('PREPARED_NOT_EXECUTED', 'BASELINE_SNAPSHOT_COMPLETE') else 1


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Root-owned browser harness helpers. Importing this module performs no I/O."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import time
from fractions import Fraction

ROOT = Path(__file__).resolve().parents[1]
ROI_NAMES = ('opaque_red', 'opaque_green', 'opaque_blue', 'opaque_yellow',
    'red_half_over_white', 'clip_white_left', 'clip_magenta_right', 'clip_outside_white',
    'moving_final_teal', 'moving_vacated_white', 'dirty_final_cyan',
    'resize_final_purple', 'resize_vacated_white', 'canvas2d_green')


class Refusal(RuntimeError):
    pass


def need(ok, code):
    if not ok:
        raise Refusal(code)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def regular(path, maximum):
    path = Path(path)
    need(not path.is_symlink(), 'SYMLINK_FILE')
    info = path.stat()
    need(stat.S_ISREG(info.st_mode) and info.st_size <= maximum, 'FILE_TYPE_OR_BOUND')
    return info


def verify_file(path, pin):
    info = regular(path, pin['bytes'])
    need(info.st_size == pin['bytes'] and sha(path) == pin['sha256'], 'PUBLIC_INPUT_PIN_CHANGED')


def fit_public_geometry(area, native):
    """Map an unrotated 360x640 display in physical X11 pixels, no background/stretch.

    Flat ROI interiors have at least four CSS pixels of edge clearance. Integer
    rounding can differ by one output pixel from the native float32 draw math;
    it never authorizes searching or moving the mapping to find a color.
    """
    need(set(area) == {'x', 'y', 'width', 'height', 'depth'}, 'DISPLAY_AREA_FIELDS')
    need(set(native) == {'width', 'height', 'screen_x', 'screen_y'}, 'NATIVE_CONTENT_FIELDS')
    need(all(type(v) is int for v in area.values()) and all(type(v) is int for v in native.values()), 'GEOMETRY_TYPES')
    need(area['x'] >= 0 and area['y'] >= 0 and 160 <= area['width'] <= 4096 and 240 <= area['height'] <= 4096 and area['depth'] == 24, 'DISPLAY_AREA_BOUND')
    need(320 <= native['width'] <= 360 and 304 <= native['height'] <= 640 and
         0 <= native['screen_x'] <= 360-native['width'] and 0 <= native['screen_y'] <= 640-native['height'], 'NATIVE_CONTENT_BOUND')
    scale = min(Fraction(area['width'], 360), Fraction(area['height'], 640))
    need(Fraction(1, 4) <= scale <= 4, 'DISPLAY_SCALE_BOUND')
    offset_x = (Fraction(area['width']) - 360 * scale) / 2
    offset_y = (Fraction(area['height']) - 640 * scale) / 2
    x = area['x'] + int(offset_x + native['screen_x'] * scale)
    y = area['y'] + int(offset_y + native['screen_y'] * scale)
    proof = {'algorithm': 'public-mainwindow-unrotated-centered-fit-v1', 'native_screen': [360, 640],
        'display_area': dict(area), 'native_content': dict(native),
        'task_conditions': {'screen-scale-ratio': 100, 'stretchDisplay': False, 'screen-rotation': 0,
            'screen-upscale': -1, 'screen-upscale-method': 0, 'background-image': '', 'QT_SCALE_FACTOR': '1'},
        'rounding': 'floor; flat patch interiors only; no search',
        'mapping': {'origin_x': x, 'origin_y': y, 'scale_numerator': scale.numerator, 'scale_denominator': scale.denominator}}
    result = dict(proof['mapping'])
    result['proof_sha256'] = hashlib.sha256((json.dumps(proof, sort_keys=True, separators=(',', ':')) + '\n').encode()).hexdigest()
    return result, proof


def component_path(parent, components):
    """Resolve only fixed public destination components in an already owned clone."""
    parent = Path(parent)
    for index, part in enumerate(components):
        need(part not in ('', '.', '..') and '/' not in part and '\\' not in part, 'INVALID_COMPONENT')
        need(parent.is_dir() and not parent.is_symlink(), 'CLONE_PARENT_UNSAFE')
        matches = [p for p in parent.iterdir() if p.name.casefold() == part.casefold()]
        need(len(matches) <= 1, 'AMBIGUOUS_CLONE_COMPONENT')
        target = matches[0] if matches else parent / part
        need(not target.is_symlink(), 'CLONE_SYMLINK')
        parent = target
        if index != len(components) - 1 and not parent.exists():
            parent.mkdir(mode=0o700)
    return parent


PUBLIC_ASSET_DESTINATIONS = {
 'browser.exe': ('sys','bin','browser.exe'),
 'browser_loc.rsc': ('resource','apps','browser_loc.rsc'),
 'browser_ui.rsc': ('resource','apps','browser_ui.rsc'),
 'browser_reg.rsc': ('private','10003a3f','import','apps','browser_reg.rsc'),
 'icudt70l.dat': ('resource','icu','icudt70l.dat'),
 'fonts.conf': ('resource','fontconfig','fonts.conf'),
}
CASE_ALIAS_TARGET_CAP=32

def plan_public_asset_targets(clone_c,components):
 """Expand only fixed public ASCII case aliases without filesystem mutation."""
 clone_c=Path(clone_c)
 need(clone_c.is_absolute()and clone_c==clone_c.resolve()and clone_c.is_dir()and not clone_c.is_symlink(),'CLONE_ROOT_UNSAFE')
 need(tuple(components)in PUBLIC_ASSET_DESTINATIONS.values(),'PUBLIC_DESTINATION_REJECTED')
 parents,directories=[clone_c],set()
 for index,component in enumerate(components):
  final=index==len(components)-1;following=[]
  for parent in parents:
   matches=[]
   if parent.exists()or parent.is_symlink():
    need(not parent.is_symlink()and parent.is_dir(),'CLONE_PARENT_UNSAFE')
    for entry in parent.iterdir():
     if entry.name.casefold()==component.casefold():
      need(entry.name.isascii(),'CASE_ALIAS_NON_ASCII');matches.append(entry)
   if not matches:matches=[parent/component]
   for target in matches:
    if target.exists()or target.is_symlink():
     info=target.lstat();need(not stat.S_ISLNK(info.st_mode),'CLONE_SYMLINK')
     if final:
      need(stat.S_ISREG(info.st_mode),'PUBLIC_TARGET_NOT_REGULAR')
      need(info.st_nlink==1,'PUBLIC_TARGET_LINK_COUNT')
      need(bool(info.st_mode&0o222),'PUBLIC_TARGET_READONLY')
     else:need(stat.S_ISDIR(info.st_mode),'CLONE_PARENT_UNSAFE')
    elif not final:directories.add(target)
    following.append(target);need(len(following)<=CASE_ALIAS_TARGET_CAP,'PUBLIC_TARGET_BRANCH_CAP')
  parents=following
 return parents,directories

def validate_fresh_profile(clone_c,profile):
 profile=Path(profile)
 need(profile.is_absolute()and profile.is_relative_to(clone_c),'FRESH_PROFILE_SCOPE_REJECTED')
 parts=profile.relative_to(clone_c).parts
 need(len(parts)==2 and all(p.isascii()for p in parts)and tuple(p.casefold()for p in parts)==('data','webkit'),'FRESH_PROFILE_SCOPE_REJECTED')
 current=clone_c
 for part in parts:
  current=current/part;need(not current.is_symlink()and current.is_dir(),'NEW_PROFILE_REQUIRED')
 need(not any(profile.iterdir()),'EMPTY_NEW_PROFILE_REQUIRED')
 return profile

def copy_public_payload(source,target,pin):
 descriptor=os.open(target,os.O_WRONLY|os.O_CREAT|os.O_NOFOLLOW,0o600)
 with os.fdopen(descriptor,'wb')as output:
  info=os.fstat(output.fileno());need(stat.S_ISREG(info.st_mode)and info.st_nlink==1,'PUBLIC_TARGET_NOT_REGULAR')
  need(bool(info.st_mode&0o222),'PUBLIC_TARGET_READONLY');output.truncate(0)
  with Path(source).open('rb')as input_stream:shutil.copyfileobj(input_stream,output)
 verify_file(target,pin)

def stage_candidate_in_isolated_clone(clone_c,candidate,*,isolation_verified=False,fresh_profile=None,diagnostic=None):
 """Aliases apply only to six pinned public payloads, never private profile."""
 def phase(name,asset=None):
  if diagnostic is not None:diagnostic(name,asset)
 need(isolation_verified is True,'PRESERVED_CLONE_REQUIRED');clone_c,candidate=Path(clone_c),Path(candidate)
 pins=json.loads((ROOT/'candidate-files.json').read_text());need(set(pins)==set(PUBLIC_ASSET_DESTINATIONS),'PUBLIC_PAYLOAD_SET_CHANGED')
 plans,directories={},set()
 for name,pin in pins.items():
  phase('STAGE_PUBLIC_INPUTS',name)
  need(tuple(pin['clone_c_destination_components'])==PUBLIC_ASSET_DESTINATIONS[name],'PUBLIC_DESTINATION_REJECTED');verify_file(candidate/name,pin)
  phase('STAGE_PUBLIC_ALIAS_PLAN',name)
  plans[name],required=plan_public_asset_targets(clone_c,PUBLIC_ASSET_DESTINATIONS[name]);directories.update(required)
 phase('STAGE_PUBLIC_PROFILE')
 if fresh_profile is None:fresh_profile=component_path(clone_c,['Data','WebKit'])
 profile=validate_fresh_profile(clone_c,fresh_profile);fixture=ROOT/'fixture/screenplay.html'
 need(sha(fixture)=='78a6f3f4df3cdc37b6b634d104bab3d783b4c56749b0c340b96142b6b91b882f','FIXTURE_PIN_CHANGED')
 # Finish every target/type/symlink/readonly/cap check before payload writes.
 phase('STAGE_PUBLIC_DIRECTORIES')
 for directory in sorted(directories,key=lambda p:(len(p.parts),str(p))):directory.mkdir(mode=0o700,exist_ok=False)
 outputs=[]
 for name,pin in pins.items():
  phase('STAGE_PUBLIC_COPY',name)
  for target in plans[name]:copy_public_payload(candidate/name,target,pin)
  outputs.append({'name':name,'bytes':pin['bytes'],'sha256':pin['sha256'],'target_copy_count':len(plans[name])})
 phase('STAGE_PUBLIC_FIXTURE');shutil.copyfile(fixture,profile/'screenplay.html')
 return outputs


def create_curated_patch_view(destination, public_build_patch):
    """Copy six pinned MMF assets only; never expose a whole original patch tree."""
    destination, source = Path(destination), Path(public_build_patch)
    pins = json.loads((ROOT / 'patch-assets.json').read_text())
    need(not destination.exists() and not destination.is_symlink(), 'FRESH_PATCH_VIEW_REQUIRED')
    need(len(pins) == 6 and all(name.startswith('mediaclient') for name in pins), 'FIXED_PATCH_ALLOWLIST_REQUIRED')
    for name, pin in pins.items():
        need(Path(name).name == name and name.endswith(('.dll', '.map')), 'PATCH_NAME_REJECTED')
        verify_file(source / name, pin)
    destination.mkdir(mode=0o700)
    for name, pin in pins.items():
        target = destination / name
        shutil.copyfile(source / name, target)
        verify_file(target, pin)
        target.chmod(0o444)
    destination.chmod(0o555)
    return {'files': len(pins), 'readonly_bind_still_required': True,
            'goommonitor_excluded': True, 'avkonfep_excluded': True,
            'egl_gles_patch_coverage_verified': False}


PRIVATE_MMF_COPY_NAMES = frozenset((
    'mediaclientaudio_general.dll', 'mediaclientvideo_v100.dll',
    'mediaclientaudio.dll.map', 'mediaclientvideo.dll.map',
    'mediaclientaudiostream_general.dll', 'mediaclientaudiostream.dll.map',
))


def prepare_private_patch_cleanup(private_root, patch_view):
    """Make only the fully verified task-private public patch copy removable."""
    private_root, patch_view = Path(private_root), Path(patch_view)
    need(private_root.is_absolute() and private_root == private_root.resolve() and
         private_root.is_dir() and not private_root.is_symlink(), 'PRIVATE_PATCH_ROOT_REJECTED')
    need(patch_view == private_root / 'mmf-patch-view', 'PRIVATE_PATCH_SCOPE_REJECTED')
    before = patch_view.lstat()
    need(stat.S_ISDIR(before.st_mode) and not stat.S_ISLNK(before.st_mode), 'PRIVATE_PATCH_DIRECTORY_REJECTED')
    need(before.st_dev == private_root.stat().st_dev, 'PRIVATE_PATCH_FILESYSTEM_REJECTED')
    pins = json.loads((ROOT / 'patch-assets.json').read_text())
    need(set(pins) == PRIVATE_MMF_COPY_NAMES, 'PRIVATE_PATCH_PIN_SET_REJECTED')
    descriptor = os.open(patch_view, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        opened = os.fstat(descriptor)
        need((opened.st_dev, opened.st_ino) == (before.st_dev, before.st_ino), 'PRIVATE_PATCH_DIRECTORY_CHANGED')
        need(set(os.listdir(descriptor)) == PRIVATE_MMF_COPY_NAMES, 'PRIVATE_PATCH_CONTENTS_REJECTED')
        for name in sorted(PRIVATE_MMF_COPY_NAMES):
            entry = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
            pin = pins[name]
            need(stat.S_ISREG(entry.st_mode) and entry.st_nlink == 1 and
                 entry.st_dev == opened.st_dev and entry.st_size == pin['bytes'], 'PRIVATE_PATCH_FILE_REJECTED')
            payload = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=descriptor)
            with os.fdopen(payload, 'rb') as stream:
                actual = os.fstat(stream.fileno())
                need((actual.st_dev, actual.st_ino, actual.st_size) ==
                     (entry.st_dev, entry.st_ino, pin['bytes']) and
                     stat.S_ISREG(actual.st_mode) and actual.st_nlink == 1, 'PRIVATE_PATCH_FILE_CHANGED')
                digest = hashlib.sha256()
                for chunk in iter(lambda: stream.read(65536), b''):
                    digest.update(chunk)
                need(digest.hexdigest() == pin['sha256'], 'PRIVATE_PATCH_PAYLOAD_CHANGED')
        # Files remain read-only. Unlinking needs write permission on this
        # private parent directory; no original build/XDG patch is changed.
        os.fchmod(descriptor, 0o700)
    finally:
        os.close(descriptor)


class FreshPageLog:
    """Incrementally retain only strict fresh public page records, never a raw log."""
    def __init__(self, log, run):
        self.path = Path(log)
        need(not self.path.exists() and not self.path.is_symlink(), 'FRESH_BROWSER_LOG_REQUIRED')
        self.check = load_module(ROOT / 'fixture/check.py', 'screenplay_owned_page_check')
        self.check.token(run)
        self.run = run
        self.offset = 0
        self.partial = b''
        self.rows = []
        self.report = None
        self.error = None
        self.inode = None
        self.screenplay_submitted = 0
        self.screenplay_fallbacks = 0
        self.native_content_geometry = None

    def poll(self):
        if self.error or not self.path.exists():
            return
        try:
            info = regular(self.path, 16 * 1024 * 1024)
            identity = (info.st_dev, info.st_ino)
            if self.inode is None:
                self.inode = identity
            need(identity == self.inode and info.st_size >= self.offset, 'BROWSER_LOG_REPLACED')
            fd = os.open(self.path, os.O_RDONLY | os.O_NOFOLLOW)
            with os.fdopen(fd, 'rb') as stream:
                opened = os.fstat(stream.fileno())
                need((opened.st_dev, opened.st_ino) == self.inode, 'BROWSER_LOG_OPEN_CHANGED')
                stream.seek(self.offset)
                data = stream.read(16 * 1024 * 1024 + 1)
            need(self.offset + len(data) <= 16 * 1024 * 1024, 'BROWSER_LOG_BOUND')
            self.offset += len(data)
            lines = (self.partial + data).split(b'\n')
            self.partial = lines.pop()
            need(len(self.partial) <= 8192, 'BROWSER_LOG_LINE_BOUND')
            for line in lines:
                need(len(line) <= 8192, 'BROWSER_LOG_LINE_BOUND')
                if b'ScreenPlay: ' in line:
                    need(re.fullmatch(rb'[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3} ScreenPlay: (?:surface submitted [1-9][0-9]{0,3}x[1-9][0-9]{0,3}(?: at [0-9]{1,4},[0-9]{1,4})?|initialization unavailable \(-?[0-9]+\), using bitmap|frame failed \(-?[0-9]+\), using bitmap)', line) is not None, 'SCREENPLAY_FIXED_TAG_REJECTED')
                    if b'ScreenPlay: surface submitted ' in line:
                        self.screenplay_submitted += 1
                        geometry = re.search(rb'surface submitted ([0-9]+)x([0-9]+) at ([0-9]+),([0-9]+)$', line)
                        if geometry:
                            self.native_content_geometry = dict(zip(('width', 'height', 'screen_x', 'screen_y'), map(int, geometry.groups())))
                    else:
                        self.screenplay_fallbacks += 1
                if self.check.LABEL.encode() not in line:
                    continue
                text = line.decode('ascii')
                prefix = 'console: ' + self.check.url_for(self.run) + ':'
                need(text.startswith(prefix), 'UNWRAPPED_OR_STALE_PAGE_RECORD')
                matched = re.fullmatch(re.escape(prefix) + r'[1-9][0-9]*: ' + re.escape(self.check.LABEL) + r'(.+)', text)
                need(matched is not None, 'PAGE_WRAPPER_INVALID')
                record = self.check.decode(matched[1])
                need(record['run'] == self.run, 'PAGE_TOKEN_CHANGED')
                need(self.report is None and len(self.rows) < 16, 'PAGE_RECORD_AFTER_DONE_OR_BOUND')
                self.rows.append(text)
                if record['event'] == 'done':
                    self.report = self.check.validate('\n'.join(self.rows) + '\n', self.run)
        except Exception:
            self.error = 'FRESH_PUBLIC_PAGE_RECORD_REJECTED'

    def public_text(self):
        need(self.report is not None and self.error is None, 'VALIDATED_PAGE_RECORDS_REQUIRED')
        return '\n'.join(self.rows) + '\n'


HELPER_PHASES = frozenset(('ARGS','PROC_ROOT_IDENTITY','PROC_LEAF_IDENTITY','PROC_NAMESPACE','PROC_EXECUTABLE','PROC_ANCESTRY','X_DISPLAY_OPEN','XRES_EXTENSION','WINDOW_TREE','QUERY_CLIENT_IDS','WM_PID_PROPERTY','WM_DELETE_CANDIDATES','RENDER_MARKER','NATIVE_LEAF_CANDIDATES','WINDOW_GEOMETRY','PUBLIC_ROI_BOUNDS','PUBLIC_ROI_READ','PUBLIC_ROI_FORMAT','STABLE_RECHECK','WM_DELETE_SEND','COMPLETE'))
HELPER_COUNT_FIELDS = frozenset(('proc_identity_checks','proc_identity_passes','query_client_ids_calls','query_client_ids_successes','query_client_ids_values','query_client_ids_owner_matches','windows_examined','declared_pid_matches','wm_delete_candidate_count','render_marker_count','marker_all_nodes','marker_nonleaf_nodes'))
HELPER_BOOL_FIELDS = frozenset(('xres_extension_present','xres_version_supported'))
def validate_helper_diagnostics(value):
    need(type(value) is dict and set(value) == HELPER_COUNT_FIELDS | HELPER_BOOL_FIELDS | {'phase'}, 'HELPER_DIAGNOSTIC_FIELDS')
    need(type(value['phase']) is str and value['phase'] in HELPER_PHASES, 'HELPER_DIAGNOSTIC_PHASE')
    need(all(type(value[key]) is int and 0 <= value[key] <= 65536 for key in HELPER_COUNT_FIELDS), 'HELPER_DIAGNOSTIC_COUNTS')
    need(all(type(value[key]) is bool for key in HELPER_BOOL_FIELDS), 'HELPER_DIAGNOSTIC_BOOLS')
    return dict(value)


class BrowserController:
    """A bounded hook for a fresh derived sandbox; no automatic invocation."""
    def __init__(self, page_log, display, host_executable, wm_close, wm_sha,
                 roi_helper, roi_sha, mapping=None, *, page_wait_seconds=30,
                 geometry_helper=None, geometry_sha=None):
        need(re.fullmatch(r':[0-9]+(?:\.[0-9]+)?', display) is not None, 'LOCAL_DISPLAY_REQUIRED')
        need(type(page_wait_seconds) is int and 1 <= page_wait_seconds <= 45, 'CONTROLLER_DEADLINE_BOUND')
        if mapping is None:
            need(geometry_helper is not None and geometry_sha is not None, 'OWNED_GEOMETRY_HELPER_REQUIRED')
            need(sha(geometry_helper) == geometry_sha, 'GEOMETRY_HELPER_PIN_CHANGED')
        else:
            self.validate_mapping(mapping)
        for path, digest in ((wm_close, wm_sha), (roi_helper, roi_sha)):
            need(sha(path) == digest, 'OWNED_HELPER_PIN_CHANGED')
        self.page = page_log
        self.display, self.host = display, str(Path(host_executable).resolve())
        self.wm, self.roi = str(wm_close), str(roi_helper)
        self.mapping = dict(mapping) if mapping is not None else None
        self.mapping_proof = None
        self.geometry_helper = str(geometry_helper) if geometry_helper else None
        self.geometry_report = None
        self.helper_reports = {}
        self.wait_seconds = page_wait_seconds
        self.started = None
        self.ready = None
        self.close_attempted = False
        self.close_sent = False
        self.roi_report = None
        self.error = None
        self.mapping_identity = None
        self.controller_ticks = 0
        self.ownership_mapping_attempts = 0
        self.ownership_last_error_code = None
        self.ownership_match_counts = {}
        self.ownership_error_counts = {}
        self.ownership_context = {}
        self.proc = load_module(ROOT / 'src/proc_owned.py', 'screenplay_owned_process_map')

    @staticmethod
    def validate_mapping(mapping):
        need(set(mapping) == {'origin_x', 'origin_y', 'scale_numerator', 'scale_denominator', 'proof_sha256'}, 'MAPPING_FIELDS')
        need(all(type(mapping[k]) is int for k in ('origin_x', 'origin_y', 'scale_numerator', 'scale_denominator')), 'MAPPING_TYPES')
        need(0 <= mapping['origin_x'] <= 4096 and 0 <= mapping['origin_y'] <= 4096, 'MAPPING_ORIGIN_BOUND')
        n, d = mapping['scale_numerator'], mapping['scale_denominator']
        need(1 <= n <= 4096 and 1 <= d <= 4096 and d <= 4*n and n <= 4*d, 'MAPPING_SCALE_BOUND')
        need(re.fullmatch(r'[0-9a-f]{64}', mapping['proof_sha256']) is not None, 'INDEPENDENT_MAPPING_EVIDENCE_REQUIRED')

    def helper(self, executable, arguments):
        role = 'wm_close' if executable == self.wm else 'roi' if executable == self.roi else 'geometry' if executable == self.geometry_helper else None
        need(role is not None, 'FIXED_HELPER_ROLE_REQUIRED')
        self.helper_reports[role] = {'exit_code': None, 'status': 'HELPER_NOT_COMPLETED', 'error_code': 'HELPER_INVOCATION_UNPROVEN', 'diagnostics': None}
        result = subprocess.run([executable, *map(str, arguments)], stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5,
            env={'PATH': '/usr/bin:/bin', 'LANG': 'C', 'DISPLAY': self.display})
        try:
            need(not result.stderr and len(result.stdout) <= 65536 and type(result.returncode) is int and 0 <= result.returncode <= 255, 'HELPER_OUTPUT_REJECTED')
            value = json.loads(result.stdout)
            need(type(value) is dict and 'diagnostics' in value, 'HELPER_RECORD_REJECTED')
            diagnostics = validate_helper_diagnostics(value.pop('diagnostics'))
            summary = {'exit_code': result.returncode, 'status': None, 'error_code': None, 'diagnostics': diagnostics}
            if result.returncode == 2:
                expected = {'wm_close_sent','error_code'} if role == 'wm_close' else {'status','error_code'}
                need(set(value) == expected, 'HELPER_FAILURE_FIELDS')
                code = value['error_code']
                allowed = {'ARGUMENTS','OWNER_OR_WINDOW_UNPROVEN','OWNER_OR_UNIQUE_LEAF_UNPROVEN','OWNER_MAPPING_OR_READ_UNPROVEN'}
                need(type(code) is str and code in allowed, 'HELPER_FAILURE_CODE')
                if role == 'wm_close':
                    need(value['wm_close_sent'] is False, 'HELPER_FAILURE_STATUS');summary['status'] = 'WM_CLOSE_REJECTED'
                else:
                    status = value['status'];need(status == ('OWNED_GEOMETRY_REJECTED' if role == 'geometry' else 'PUBLIC_ROIS_REJECTED'), 'HELPER_FAILURE_STATUS');summary['status'] = status
                summary['error_code'] = code
            else:
                if role == 'wm_close':
                    need(result.returncode == 0 and value == {'wm_close_sent': True, 'window_owner_verified': True}, 'HELPER_SUCCESS_STATUS')
                    status = 'WM_CLOSE_SENT'
                else:
                    status = value.get('status')
                    allowed = {'OWNED_NATIVE_RENDER_AREA'} if role == 'geometry' else {'PUBLIC_COLOR_ROIS_MATCH','PUBLIC_COLOR_ROIS_DIFFER'}
                    need(value.get('window_owner_verified') is True and status in allowed, 'HELPER_SUCCESS_STATUS')
                summary['status'] = status
            self.helper_reports[role] = summary
            return result.returncode, value
        except Exception:
            self.helper_reports[role] = {'exit_code': result.returncode if type(result.returncode) is int and 0 <= result.returncode <= 255 else None,
                'status': 'HELPER_REPORT_REJECTED', 'error_code': 'FIXED_HELPER_SCHEMA_REJECTED', 'diagnostics': None}
            raise Refusal('HELPER_RECORD_REJECTED') from None

    def tick(self, context):
        now = time.monotonic()
        self.controller_ticks += 1
        self.ownership_context = {
            'guard_ok': context.get('guard_ok') is True,
            'child_pid_associated': context.get('child_pid_associated') is True,
            'root_identity_fields_valid': all(type(context.get(key)) is int and context[key] > 0 for key in ('root_pid','root_starttime','owned_pgid')),
            'namespace_identity_fields_valid': all(type(context.get(key)) is int and context[key] > 0 for key in ('namespace_pid','pidns_inode')),
            'exec_candidate_fields_valid': type(context.get('exec_candidate_pids')) in (tuple,list) and 0 < len(context['exec_candidate_pids']) <= 16 and all(type(pid) is int and pid > 0 for pid in context['exec_candidate_pids']),
        }
        if self.started is None:
            self.started = now
        self.page.poll()
        if self.close_attempted:
            return
        if self.page.report is not None and self.ready is None:
            self.ready = now + 0.5
        due = self.page.error is not None or (self.ready is not None and now >= self.ready) or now - self.started >= self.wait_seconds
        if not due:
            return
        if not context.get('guard_ok') or not context.get('child_pid_associated'):
            return
        required = ('root_pid', 'root_starttime', 'owned_pgid', 'namespace_pid', 'pidns_inode')
        if not all(type(context.get(key)) is int and context[key] > 0 for key in required):
            return
        self.ownership_mapping_attempts += 1
        counts = {}
        try:
            identity = self.proc.map_owned_namespace_pid(context['root_pid'], context['root_starttime'],
                context['owned_pgid'], context['pidns_inode'], context['namespace_pid'], self.host,
                candidate_pids=context.get('exec_candidate_pids', ()), diagnostics=counts)
            self.ownership_match_counts = counts
            self.ownership_last_error_code = None
        except self.proc.OwnershipError as error:
            self.ownership_match_counts = counts
            value = error.args[0] if len(error.args) == 1 else None
            code = value if type(value) is str and value in self.proc.ERROR_CODES else 'OWNED_ERROR_UNCLASSIFIED'
            self.ownership_last_error_code = code
            self.ownership_error_counts[code] = self.ownership_error_counts.get(code, 0) + 1
            return
        except (FileNotFoundError, ProcessLookupError):
            self.ownership_match_counts = counts
            self.ownership_last_error_code = 'OWNED_PROC_UNAVAILABLE'
            self.ownership_error_counts['OWNED_PROC_UNAVAILABLE'] = self.ownership_error_counts.get('OWNED_PROC_UNAVAILABLE', 0) + 1
            return
        self.mapping_identity = identity
        arguments = [identity['pid'], context['namespace_pid'], context['root_pid'], context['root_starttime'],
                     context['owned_pgid'], identity['starttime'], context['pidns_inode'], self.host]
        self.close_attempted = True  # No second attempt, even if a helper fails.
        try:
            if self.page.report and self.page.report['page_status'] == 'PAGE_REQUESTS_PASS' and not self.page.error:
                if self.mapping is None:
                    code, geometry = self.helper(self.geometry_helper, arguments + [self.page.run])
                    need(code == 0 and set(geometry) == {'version', 'run', 'status', 'window_owner_verified', 'candidate_count', 'main_window', 'native_leaf', 'render_widget_identity_verified'}, 'OWNED_GEOMETRY_FIELDS')
                    need(geometry['version'] == 1 and geometry['run'] == self.page.run and geometry['window_owner_verified'] is True and geometry['candidate_count'] == 1 and geometry['status'] == 'OWNED_NATIVE_RENDER_AREA' and geometry['render_widget_identity_verified'] is True, 'OWNED_GEOMETRY_IDENTITY')
                    need(self.page.native_content_geometry is not None, 'NATIVE_PUBLIC_CONTENT_GEOMETRY_REQUIRED')
                    self.mapping, self.mapping_proof = fit_public_geometry(geometry['native_leaf'], self.page.native_content_geometry)
                    self.validate_mapping(self.mapping)
                    self.geometry_report = geometry
                m = self.mapping
                code, report = self.helper(self.roi, arguments + [self.page.run, m['origin_x'], m['origin_y'],
                    m['scale_numerator'], m['scale_denominator']])
                need(set(report) == {'version', 'run', 'status', 'window_owner_verified', 'patches', 'pixel_count', 'mismatch_count'}, 'ROI_FIELDS')
                need(report['version'] == 1 and report['run'] == self.page.run and report['window_owner_verified'] is True, 'ROI_IDENTITY')
                need(type(report['pixel_count']) is int and report['pixel_count'] > 0 and type(report['mismatch_count']) is int, 'ROI_COUNTS')
                need(report['status'] in ('PUBLIC_COLOR_ROIS_MATCH', 'PUBLIC_COLOR_ROIS_DIFFER'), 'ROI_STATUS')
                need(type(report['patches']) is list and len(report['patches']) == len(ROI_NAMES), 'ROI_PATCH_COUNT')
                for patch, name in zip(report['patches'], ROI_NAMES):
                    need(type(patch) is dict and set(patch) == {'name', 'pixel_count', 'mismatch_count'}, 'ROI_PATCH_FIELDS')
                    need(patch['name'] == name and type(patch['pixel_count']) is int and type(patch['mismatch_count']) is int, 'ROI_PATCH_TYPES')
                    need(0 < patch['pixel_count'] <= 16384 and 0 <= patch['mismatch_count'] <= patch['pixel_count'], 'ROI_PATCH_COUNTS')
                need(report['pixel_count'] == sum(p['pixel_count'] for p in report['patches']) and report['mismatch_count'] == sum(p['mismatch_count'] for p in report['patches']), 'ROI_AGGREGATE_COUNTS')
                need(code in (0, 1), 'ROI_EXIT_CODE')
                need((code == 0) == (report['status'] == 'PUBLIC_COLOR_ROIS_MATCH' and report['mismatch_count'] == 0), 'ROI_EXIT_STATUS')
                self.roi_report = report
        except Exception:
            self.error = 'PUBLIC_ROI_OBSERVATION_REJECTED'
        try:
            # The helper independently repeats process and XRes checks here.
            code, report = self.helper(self.wm, arguments)
            need(code == 0 and report == {'wm_close_sent': True, 'window_owner_verified': True}, 'NORMAL_CLOSE_REJECTED')
            self.close_sent = True
        except Exception:
            self.error = 'OWNED_WINDOW_NORMAL_CLOSE_UNPROVEN'

    def finish(self, sandbox):
        self.page.poll()
        stopped = (sandbox.get('stopped') is True and sandbox.get('exit_code') == 0
            and sandbox.get('forced_cleanup') is False and sandbox.get('collectors_live') == 0)
        old_host_gone = False
        if self.mapping_identity:
            try:
                current = self.proc.stat(self.mapping_identity['pid'])
                old_host_gone = current['starttime'] != self.mapping_identity['starttime']
            except FileNotFoundError:
                old_host_gone = True
        return {'page_status': self.page.report['page_status'] if self.page.report and not self.page.error else 'PAGE_UNPROVEN',
            'page_error_code': self.page.error, 'public_roi_report': self.roi_report,
            'mapping_proof_sha256': self.mapping['proof_sha256'] if self.mapping else None,
            'mapping_proof': self.mapping_proof, 'owned_geometry_report': self.geometry_report,
            'native_content_geometry': self.page.native_content_geometry,
            'helper_reports': dict(self.helper_reports),
            'public_page_report': self.page.report if self.page.error is None else None,
            'controller_ticks': self.controller_ticks, 'ownership_mapping_attempts': self.ownership_mapping_attempts,
            'ownership_context': dict(self.ownership_context),
            'ownership_match_counts': dict(self.ownership_match_counts),
            'ownership_last_error_code': self.ownership_last_error_code,
            'ownership_error_counts': dict(self.ownership_error_counts),
            'normal_close_attempted': self.close_attempted,
            'normal_close_sent': self.close_sent, 'owned_host_exit_independently_observed': stopped and old_host_gone,
            'controller_error_code': self.error, 'screenplay_submit_tags': self.page.screenplay_submitted,
            'screenplay_fallback_tags': self.page.screenplay_fallbacks,
            'screenplay_native_path_verified': False,
            'trusted_input_verified': False, 'phone_verified': False}

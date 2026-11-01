#!/usr/bin/env python3
"""Capture/calibrate and send real XTest input to an already-owned EKA PID."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

DEFAULT_INPUT = Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py')
ROLES = {'start', 'save-accept', 'save-cancel', 'transfer-cancel', 'existing-attempt',
         'existing-dismiss', 'native-step', 'edit-name', 'dismiss-note'}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load_input(path):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('download_native_input', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def load_plan(path, calibration):
    value = json.loads(path.read_text())
    if set(value) != {'calibration_sha256', 'screenshots_reviewed', 'actions'}:
        raise ValueError('plan needs calibration_sha256, screenshots_reviewed and actions')
    if value['calibration_sha256'] != sha(calibration) or value['screenshots_reviewed'] is not True:
        raise ValueError('inspect the actual calibration screenshots before making the plan')
    actions = value['actions']
    if not isinstance(actions, list) or not 1 <= len(actions) <= 40:
        raise ValueError('plan needs 1–40 actual calibrated input actions')
    for action in actions:
        if not isinstance(action, dict) or set(action) - {'target', 'role', 'point', 'key', 'modifier', 'text', 'wait', 'note'}:
            raise ValueError('unsupported action field')
        if action.get('target') not in ('main', 'fep') or action.get('role') not in ROLES:
            raise ValueError('action needs a main/fep target and a known evidence role')
        if sum(name in action for name in ('point', 'key', 'text')) != 1:
            raise ValueError('action needs exactly one point, key or text operation')
        if 'point' in action and (not isinstance(action['point'], list) or len(action['point']) != 2
                                 or any(type(x) is not int or x < 0 for x in action['point'])):
            raise ValueError('point must contain two nonnegative window-relative integers')
        if 'key' in action and not re.fullmatch(r'[A-Za-z0-9_]+', action['key']):
            raise ValueError('invalid X11 keysym name')
        if 'text' in action and not re.fullmatch(r'[A-Za-z0-9 ._-]{1,128}', action['text']):
            raise ValueError('this helper types only ordinary ASCII test basenames')
        if 'modifier' in action and ('key' not in action or action['modifier'] not in ('Control_L', 'Shift_L')):
            raise ValueError('modifier is only supported for a key operation')
        if not isinstance(action.get('wait', .5), (int, float)) or not 0 <= action.get('wait', .5) <= 10:
            raise ValueError('settling delay must be 0–10 seconds')
    return actions

class Recording:
    def __init__(self, args, x):
        self.args, self.x = args, x
        self.screenshots = []
        self.records = []

    def capture(self, name):
        self.x.check()
        windows = [('main', self.x.window)]
        fep = self.x.fep_window()
        if fep:
            windows.append(('fep', fep))
        for kind, window in windows:
            path = self.args.output / (name + '-' + kind + '.png')
            subprocess.run(['import', '-window', hex(window), str(path)], check=True, timeout=15)
            self.screenshots.append({'path': path.name, 'sha256': sha(path), 'target': kind,
                                     'geometry': self.x.geometry(window), 'monotonic_ns': time.monotonic_ns()})

    def key(self, window, name, modifier=None):
        self.x.check()
        self.x.check_focus(window)
        def code(key):
            result = self.x.x.XKeysymToKeycode(self.x.display, self.x.x.XStringToKeysym(key.encode('ascii')))
            if not result:
                raise ValueError('X11 key has no current mapping: ' + key)
            return result
        keycode = code(name)
        modifier_code = code(modifier) if modifier else None
        try:
            if modifier_code and not self.x.t.XTestFakeKeyEvent(self.x.display, modifier_code, 1, 0):
                raise RuntimeError('XTest modifier press failed')
            if not self.x.t.XTestFakeKeyEvent(self.x.display, keycode, 1, 0):
                raise RuntimeError('XTest key press failed')
            self.x.x.XSync(self.x.display, 0)
            time.sleep(.04)
        finally:
            self.x.t.XTestFakeKeyEvent(self.x.display, keycode, 0, 0)
            if modifier_code:
                self.x.t.XTestFakeKeyEvent(self.x.display, modifier_code, 0, 0)
            self.x.x.XSync(self.x.display, 0)

    def act(self, action, calibration):
        self.x.check()
        window = self.x.window if action['target'] == 'main' else self.x.fep_window()
        if not window:
            raise RuntimeError('the calibrated FEP modal is absent; capture the new state first')
        expected = calibration['main_size'] if action['target'] == 'main' else calibration.get('fep_size')
        geometry = self.x.geometry(window)
        if geometry[2:] != expected:
            raise RuntimeError('target size differs from the reviewed screenshot; recalibrate')
        self.records.append({'action': action, 'window': hex(window), 'monotonic_ns': time.monotonic_ns()})
        if 'point' in action:
            self.x.click_window(window, geometry, *action['point'])
        elif 'key' in action:
            self.key(window, action['key'], action.get('modifier'))
        else:
            for character in action['text']:
                name = {' ': 'space', '.': 'period', '-': 'minus', '_': 'minus'}.get(character, character.lower())
                self.key(window, name, 'Shift_L' if character.isupper() or character == '_' else None)
        time.sleep(action.get('wait', .5))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['capture', 'act'])
    parser.add_argument('--input-module', type=Path, default=DEFAULT_INPUT)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--run', required=True)
    parser.add_argument('--case', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--calibration', type=Path)
    parser.add_argument('--plan', type=Path)
    args = parser.parse_args()
    if args.operation == 'act' and (not args.calibration or not args.plan):
        parser.error('act requires a reviewed calibration and a concrete input plan')
    args.output.mkdir(parents=True, exist_ok=False)
    module = load_input(args.input_module)
    x = module.XInput(args.pid)
    recording = Recording(args, x)
    result = {'run': args.run, 'case': args.case, 'pid': args.pid, 'window': hex(x.window),
              'status': 'FAIL', 'records': recording.records, 'screenshots': recording.screenshots,
              'input_source': str(args.input_module.resolve()), 'input_source_sha256': sha(args.input_module),
              'scope': 'actual XTest input; no guest launch, injected JavaScript, dialog hooks or file mutation'}
    try:
        if args.operation == 'capture':
            recording.capture('calibration')
            fep = x.fep_window()
            result.update(status='CALIBRATION_CAPTURED', main_size=x.geometry(x.window)[2:],
                          fep_size=x.geometry(fep)[2:] if fep else None)
        else:
            calibration = json.loads(args.calibration.read_text())
            if (calibration.get('pid'), calibration.get('run'), calibration.get('case'), calibration.get('window')) != (args.pid, args.run, args.case, hex(x.window)):
                raise ValueError('calibration belongs to another PID, run, case or main window')
            if calibration.get('status') != 'CALIBRATION_CAPTURED' or calibration.get('main_size') != x.initial_size:
                raise ValueError('invalid or stale calibration')
            actions = load_plan(args.plan, args.calibration)
            copied = args.output / 'calibration'
            copied.mkdir()
            shutil.copyfile(args.calibration, copied / 'calibration.json')
            shutil.copyfile(args.plan, copied / 'plan.json')
            for item in calibration['screenshots']:
                source = args.calibration.parent / item['path']
                if sha(source) != item['sha256']:
                    raise ValueError('calibration screenshot changed')
                dest = copied / Path(item['path']).name
                shutil.copyfile(source, dest)
                recording.screenshots.append({**item, 'path': str(dest.relative_to(args.output))})
            for number, action in enumerate(actions, 1):
                recording.capture(f'{number:02d}-before')
                recording.act(action, calibration)
                recording.capture(f'{number:02d}-after')
            result['status'] = 'INPUT_RECORDED'
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        result['error'] = str(error)
    finally:
        name = 'calibration.json' if args.operation == 'capture' else 'interaction.json'
        (args.output / name).write_text(json.dumps(result, indent=2) + '\n')
        x.close()
    print(json.dumps(result, indent=2))
    return 1 if result['status'] == 'FAIL' else 0

if __name__ == '__main__':
    raise SystemExit(main())

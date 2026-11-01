#!/usr/bin/env python3
"""Send a calibrated real one-finger sequence to an existing EKA window."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--pid', type=int, required=True)
p.add_argument('--guest-log', type=Path, required=True)
p.add_argument('--case', choices=('tap', 'prevent-start', 'prevent-move', 'pan', 'pointer-pan', 'none', 'capture', 'navigation'), required=True)
p.add_argument('--run', required=True)
p.add_argument('--start', nargs=2, type=int, required=True)
p.add_argument('--end', nargs=2, type=int)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert re.fullmatch('[A-Za-z0-9_-]{1,64}', a.run)
a.output.mkdir(parents=True, exist_ok=False)
module_path = Path('/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py')
spec = importlib.util.spec_from_file_location('native_interact', module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
device = module.XInput(a.pid)
events = []
result = {'case': a.case, 'run': a.run, 'pid': a.pid, 'status': 'FAIL', 'native_input': events}

def content():
    return a.guest_log.read_text(errors='replace') if a.guest_log.exists() else ''

def wait_for(test, description):
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        device.check()
        value = test(content())
        if value:
            return value
        time.sleep(.2)
    raise RuntimeError('Timed out: ' + description)

def motion(point):
    geometry = device.check()
    x, y = point
    assert 0 <= x < geometry[2] and 0 <= y < geometry[3], 'Uncalibrated point outside window'
    device.check_focus(device.window)
    assert device.t.XTestFakeMotionEvent(device.display, device.screen, geometry[0] + x, geometry[1] + y, 0)
    device.x.XSync(device.display, 0)
    events.append({'motion': [x, y], 'monotonic': time.monotonic()})

def button(pressed):
    device.check()
    assert device.t.XTestFakeButtonEvent(device.display, 1, pressed, 0)
    device.x.XSync(device.display, 0)
    events.append({'button1': bool(pressed), 'monotonic': time.monotonic()})

pressed = False
try:
    marker = 'SYMBIAN TOUCH ' + a.case + ' READY '
    ready = wait_for(lambda text: next((line for line in text.splitlines() if marker in line and '"run":"' + a.run + '"' in line), None), 'fresh page READY')
    result['ready'] = ready
    result['window_geometry'] = device.check()
    subprocess.run(['import', '-window', 'root', str(a.output / 'before.png')], check=True, timeout=15)
    device.x.XRaiseWindow(device.display, device.window)
    device.x.XSetInputFocus(device.display, device.window, 2, 0)
    motion(a.start)
    button(True)
    pressed = True
    if a.case == 'navigation':
        time.sleep(1.2)
    elif a.case in ('tap', 'prevent-start'):
        time.sleep(.12)
    else:
        assert a.end and max(abs(a.end[i] - a.start[i]) for i in (0, 1)) >= 30, 'A calibrated drag of at least 30 pixels is required'
        time.sleep(.15)
        for step in range(1, 13):
            motion([round(a.start[i] + (a.end[i] - a.start[i]) * step / 12) for i in (0, 1)])
            time.sleep(.07)
    button(False)
    pressed = False
    end_case = 'landing' if a.case == 'navigation' else a.case
    completion = wait_for(lambda text: next((line for line in text.splitlines() if 'SYMBIAN TOUCH ' + end_case + ' DONE ' in line), None), 'native event sequence completion')
    result['completion'] = completion
    assert ' DONE PASS ' in completion, 'The page rejected the native sequence'
    subprocess.run(['import', '-window', 'root', str(a.output / 'after.png')], check=True, timeout=15)
    result['status'] = 'INPUT_AND_PAGE_PASS'
except BaseException as error:
    result['error'] = str(error)
    raise
finally:
    if pressed:
        device.t.XTestFakeButtonEvent(device.display, 1, 0, 0)
        device.x.XSync(device.display, 0)
    device.close()
    (a.output / 'interaction.json').write_text(json.dumps(result, indent=2) + '\n')

print('Native input and page checks passed; outer browser and host exit validation remains required.')

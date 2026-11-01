"""Root-owned normal XTest input; requires fresh public viewport calibration."""
import argparse, hashlib, importlib.util, json, math, time
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--run-dir', type=Path, required=True)
p.add_argument('--capture-dir', type=Path, required=True)
p.add_argument('--calibration', type=Path, required=True)
p.add_argument('--step', choices=['start', 'suspend', 'resume', 'close'], required=True)
a = p.parse_args()
result = json.loads((a.run_dir / 'result.json').read_text())
state = json.loads((a.run_dir / 'server/state.json').read_text())
pid = int((a.run_dir / 'host-pid.public').read_text())
assert pid == result['host_pid'] and state['run'] == result['run']
expected = dict(start='native-ready', suspend='native-running', resume='native-suspended', close='native-resumed')
event = state['events'][-1]
assert event['kind'] == expected[a.step]
layout = event['layout']; button = layout['button']
assert button['disabled'] is False
cal = json.loads(a.calibration.read_text())
assert cal['run'] == result['run'] and cal['visually_reviewed'] is True
assert [layout['innerWidth'], layout['innerHeight']] == cal['css_viewport']
assert layout['scrollX'] == layout['scrollY'] == 0
assert layout['devicePixelRatio'] == cal['device_pixel_ratio']
assert all(math.isfinite(button[k]) for k in ['x', 'y', 'width', 'height'])
assert 0 <= button['x'] < button['x'] + button['width'] <= layout['innerWidth']
assert 0 <= button['y'] < button['y'] + button['height'] <= layout['innerHeight']
spec = importlib.util.spec_from_file_location('owned_input', '/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
host = Path('/workspace/EKA2L1-validation/build/bin/eka2l1_qt')
assert Path('/proc', str(pid), 'exe').resolve() == host
assert hashlib.sha256(host.read_bytes()).hexdigest() == '692abe6937ed1b651914ff6ab35a78bbd3693a705ba47853fc6f9a438ae178bd'
capture = json.loads((a.capture_dir / 'capture.json').read_text())
assert capture['status'] == 'CAPTURING' and capture['source'] == 'symbian_media.monitor'
marks_path = a.capture_dir / 'marks.json'
marks = json.loads(marks_path.read_text()) if marks_path.exists() else {'suite': 'classic-webaudio-pcm', 'version': 1, 'run': result['run'], 'provenance': 'root-host-observations-not-independent-input-proof', 'marks': []}
labels = ['click-start','native-running','click-suspend','native-suspended','click-resume','native-resumed','click-close','quiet']
label = 'click-' + a.step
assert marks['run'] == result['run'] and len(marks['marks']) < len(labels) and labels[len(marks['marks'])] == label
x = m.XInput(pid)
try:
    geometry = x.check(); assert geometry == cal['window_geometry']
    left, top, width, height = cal['content_rectangle']
    assert 0 <= left < left + width <= geometry[2] and 0 <= top < top + height <= geometry[3]
    point = [round(left + (button['x'] + button['width'] / 2) * width / layout['innerWidth']), round(top + (button['y'] + button['height'] / 2) * height / layout['innerHeight'])]
    before = time.monotonic(); x.click(*point); after = time.monotonic()
    path = a.run_dir / 'trusted-input.jsonl'
    with path.open('a') as stream:
        stream.write(json.dumps({'step': a.step, 'run': result['run'], 'pid': pid, 'window': hex(x.window), 'geometry': geometry, 'point': point, 'layout': layout, 'before_monotonic': before, 'after_monotonic': after, 'method': 'real XTest pointer press/release'}) + '\n')
    marks['marks'].append({'label': label, 'observed_monotonic': after})
    temp = marks_path.with_suffix('.tmp'); temp.write_text(json.dumps(marks, indent=2) + '\n'); temp.replace(marks_path)
    print(json.dumps({'step': a.step, 'pid': pid, 'point': point, 'observed_monotonic': after}))
finally:
    x.close()

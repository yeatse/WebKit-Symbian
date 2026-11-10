#!/usr/bin/env python3
"""Summarize the browser log's frame-stats lines, per benchmark page.

  summarize-frame-stats.py log.txt
  summarize-frame-stats.py --compare before.txt after.txt
  summarize-frame-stats.py --json log.txt

Lines between "bench: start NAME" and "bench: end NAME" (from bench.js) are
grouped under NAME; without markers the whole log is one group. In each
group, the first frame-stats line of each presentation is dropped: it holds
the previous page's last frames and the idle time before the benchmark.
"""
import argparse
import json
import re
import sys

MARKER = re.compile(r'bench: (start|end|skip) (\S+)(.*)')
STATS = re.compile(r'frame-stats: (\S+) frames=(\d+) window=(\d+)ms((?: [\w-]+=[\d.]+/[\d.]+)*)')
STAGE = re.compile(r'([\w-]+)=([\d.]+)/([\d.]+)')
FIELD = re.compile(r'(\w+)=([\d.]+)')
TILES = re.compile(r'egl-tiles: (\d+) KB')
DRAW = re.compile(r'egl-(draw|surfaces): per frame (.*)')
DRAW_FIELD = re.compile(r'([a-z-]+)=([\d.]+)')
DECODE = re.compile(r'image-decode: (\S+) window=\d+ms n=(\d+) partial=(\d+) kpx=(\d+) ms=([\d.]+) '
                    r'max=([\d.]+)@(\d+x\d+) large=(\d+)/([\d.]+)ms')
MAIN = re.compile(r'main-thread: window=(\d+)ms busy=(\d+)ms tasks=(\d+) long=(\d+) longest=(\d+)ms (.*)')
LONG_TASK = re.compile(r'long-task: (\d+)ms ((?:[a-z]+=\d+ ?)*)(?:top-script=(\d+)ms (\S+) ?(.*))?')
RENDER = re.compile(r'egl-render: per frame (.*)')
RENDER_WORST = re.compile(r'egl-render-worst: (\d+)ms (.*)')
JIT = re.compile(r'jit: baseline n=(\d+) main=(\d+) wait=(\d+)/(\d+)ms compile=(\d+)ms '
                 r'dfg n=(\d+) main=(\d+) wait=(\d+)/(\d+)ms compile=(\d+)ms')
SLOW_MS = 1000 / 30
# Long tasks kept per group, slowest first.
LONG_TASKS_KEPT = 5


def new_presentation():
    return {'lines': 0, 'dropped': 0, 'frames': 0, 'window': 0.0, 'stages': {}, 'slow_windows': 0}


def parse(lines):
    """Returns {group: {'presentations': {...}, 'page': {...}, 'skipped': reason}} in log order."""
    groups = {}
    current = None

    def group(name):
        return groups.setdefault(name, {'presentations': {}, 'page': None, 'skipped': None, 'tiles_kb': None, 'draw': None, 'decode': {},
                                        'main': None, 'long_tasks': [], 'render': None, 'jit': None, 'seen': set()})

    for line in lines:
        marker = MARKER.search(line)
        if marker:
            kind, name, rest = marker.groups()
            if kind == 'start':
                current = name
                entry = group(name)
                entry['seen'] = set()
            elif kind == 'end':
                group(name)['page'] = {k: float(v) for k, v in FIELD.findall(rest)}
                current = None
            else:
                group(name)['skipped'] = rest.strip() or 'skipped'
            continue
        tiles = TILES.search(line)
        if tiles:
            entry = group(current or '(no benchmark)')
            entry['tiles_kb'] = max(entry['tiles_kb'] or 0, int(tiles.group(1)))
            continue
        draw = DRAW.search(line)
        if draw:
            entry = group(current or '(no benchmark)')
            most = entry['draw'] or {}
            # The surfaces line counts layers drawn through a surface, by reason.
            prefix = 'surface-' if draw.group(1) == 'surfaces' else ''
            for key, value in DRAW_FIELD.findall(draw.group(2)):
                key = prefix + key
                most[key] = max(most.get(key, 0.0), float(value))
            entry['draw'] = most
            continue
        main_thread = MAIN.search(line)
        if main_thread:
            entry = group(current or '(no benchmark)')
            window, busy, tasks, long_tasks, longest, rest = main_thread.groups()
            sums = entry['main'] or {'window': 0.0, 'busy': 0.0, 'tasks': 0, 'long': 0, 'longest': 0.0, 'kinds': {}}
            sums['window'] += float(window)
            sums['busy'] += float(busy)
            sums['tasks'] += int(tasks)
            sums['long'] += int(long_tasks)
            sums['longest'] = max(sums['longest'], float(longest))
            for key, value in DRAW_FIELD.findall(rest):
                sums['kinds'][key] = sums['kinds'].get(key, 0.0) + float(value)
            entry['main'] = sums
            continue
        jit = JIT.search(line)
        if jit:
            entry = group(current or '(no benchmark)')
            values = [float(value) for value in jit.groups()]
            sums = entry['jit'] or {tier: {'n': 0, 'main': 0, 'wait': 0.0, 'max_wait': 0.0, 'compile': 0.0}
                                    for tier in ('baseline', 'dfg')}
            for tier, (n, main, wait, max_wait, compile_ms) in zip(('baseline', 'dfg'), (values[:5], values[5:])):
                tier_sums = sums[tier]
                tier_sums['n'] += int(n)
                tier_sums['main'] += int(main)
                tier_sums['wait'] += wait
                tier_sums['max_wait'] = max(tier_sums['max_wait'], max_wait)
                tier_sums['compile'] += compile_ms
            entry['jit'] = sums
            continue
        long_task = LONG_TASK.search(line)
        if long_task:
            entry = group(current or '(no benchmark)')
            duration, kinds, script_ms, label, detail = long_task.groups()
            task = {'ms': float(duration), 'kinds': {k: float(v) for k, v in DRAW_FIELD.findall(kinds)}}
            if script_ms:
                task['script'] = {'ms': float(script_ms), 'label': label, 'detail': (detail or '').strip()}
            entry['long_tasks'] = sorted(entry['long_tasks'] + [task], key=lambda t: -t['ms'])[:LONG_TASKS_KEPT]
            continue
        render = RENDER.search(line)
        if render:
            entry = group(current or '(no benchmark)')
            sums = entry['render'] or {'lines': 0, 'sum': {}, 'worst_ms': 0.0, 'worst': {}}
            sums['lines'] += 1
            for key, value in DRAW_FIELD.findall(render.group(1)):
                sums['sum'][key] = sums['sum'].get(key, 0.0) + float(value)
            entry['render'] = sums
            continue
        worst = RENDER_WORST.search(line)
        if worst:
            entry = group(current or '(no benchmark)')
            sums = entry['render'] or {'lines': 0, 'sum': {}, 'worst_ms': 0.0, 'worst': {}}
            if float(worst.group(1)) > sums['worst_ms']:
                sums['worst_ms'] = float(worst.group(1))
                sums['worst'] = {k: float(v) for k, v in DRAW_FIELD.findall(worst.group(2))}
            entry['render'] = sums
            continue
        decode = DECODE.search(line)
        if decode:
            entry = group(current or '(no benchmark)')
            name, n, partial, kpx, ms, most, size, large, large_ms = decode.groups()
            sums = entry['decode'].setdefault(name, {'n': 0, 'partial': 0, 'kpx': 0, 'ms': 0.0, 'max': 0.0,
                                                      'max_size': '', 'large': 0, 'large_ms': 0.0})
            sums['n'] += int(n)
            sums['partial'] += int(partial)
            sums['kpx'] += int(kpx)
            sums['ms'] += float(ms)
            sums['large'] += int(large)
            sums['large_ms'] += float(large_ms)
            if float(most) > sums['max']:
                sums['max'], sums['max_size'] = float(most), size
            continue
        stats = STATS.search(line)
        if not stats:
            continue
        presentation, frames, window, stages = stats.groups()
        entry = group(current or '(no benchmark)')
        summary = entry['presentations'].setdefault(presentation, new_presentation())
        if current and presentation not in entry['seen']:
            entry['seen'].add(presentation)
            summary['dropped'] += 1
            continue
        frames = int(frames)
        summary['lines'] += 1
        summary['frames'] += frames
        summary['window'] += float(window)
        total = 0.0
        for stage, average, maximum in STAGE.findall(stages):
            average, maximum = float(average), float(maximum)
            total += average
            sums = summary['stages'].setdefault(stage, {'sum': 0.0, 'max': 0.0})
            sums['sum'] += average * frames
            sums['max'] = max(sums['max'], maximum)
        if total > SLOW_MS:
            summary['slow_windows'] += 1
    for entry in groups.values():
        del entry['seen']
    return groups


def report(groups):
    """Plain numbers per group and presentation, for printing or JSON."""
    result = {}
    for name, entry in groups.items():
        presentations = {}
        for presentation, summary in entry['presentations'].items():
            frames = summary['frames']
            stages = {stage: {'avg': sums['sum'] / frames if frames else 0.0, 'max': sums['max']}
                      for stage, sums in summary['stages'].items()}
            presentations[presentation] = {
                'frames': frames,
                'fps': frames * 1000 / summary['window'] if summary['window'] else 0.0,
                'frame_ms': sum(stage['avg'] for stage in stages.values()),
                'stages': stages,
                'windows': summary['lines'],
                'slow_windows': summary['slow_windows'],
            }
        render = entry['render']
        if render:
            lines = max(1, render['lines'])
            render = {'avg': {key: value / lines for key, value in render['sum'].items()},
                      'worst_ms': render['worst_ms'], 'worst': render['worst']}
        main_thread = entry['main']
        if main_thread:
            main_thread = dict(main_thread, busy_share=main_thread['busy'] / main_thread['window'] if main_thread['window'] else 0.0)
        result[name] = {'presentations': presentations, 'page': entry['page'], 'skipped': entry['skipped'],
                        'tiles_kb': entry['tiles_kb'], 'draw': entry['draw'], 'decode': entry['decode'],
                        'main': main_thread, 'long_tasks': entry['long_tasks'], 'render': render, 'jit': entry['jit']}
    return result


def print_report(result, out):
    if not result:
        print('no frame-stats lines (set frame-stats = on in C:\\Data\\WebKit\\webkit.ini and restart the browser)', file=out)
        return
    for name, entry in result.items():
        print(name, file=out)
        if entry['skipped']:
            print('  skipped: ' + entry['skipped'], file=out)
        page = entry['page']
        if page:
            print('  page: %.1f rAF/s, %d frames over 50 ms, worst %d ms' % (
                page.get('fps', 0), page.get('long', 0), page.get('worst', 0)), file=out)
        for presentation, summary in entry['presentations'].items():
            if not summary['windows']:
                continue
            stages = ' '.join('%s=%.1f/%.1f' % (stage, values['avg'], values['max'])
                              for stage, values in summary['stages'].items())
            print('  %-10s %5.1f fps  %4d frames  %5.1f ms/frame  slow %d/%d  [%s]' % (
                presentation, summary['fps'], summary['frames'], summary['frame_ms'],
                summary['slow_windows'], summary['windows'], stages), file=out)
        if entry.get('tiles_kb') is not None:
            print('  GPU tiles: up to %d KB' % entry['tiles_kb'], file=out)
        if entry.get('draw'):
            print('  GPU draw per frame, most in a window: ' + ' '.join(
                '%s=%g' % (key, value) for key, value in entry['draw'].items()), file=out)
        for format_name, sums in entry.get('decode', {}).items():
            print('  decode %-15s %4d images %6d kpx %7.1f ms (%.1f ms/Mpx)  max %.1f ms @%s  '
                  'large %d/%.1f ms  partial %d' % (
                      format_name, sums['n'], sums['kpx'], sums['ms'],
                      sums['ms'] * 1000 / sums['kpx'] if sums['kpx'] else 0.0,
                      sums['max'], sums['max_size'], sums['large'], sums['large_ms'], sums['partial']), file=out)
        main_thread = entry.get('main')
        if main_thread:
            print('  main thread: busy %.0f%% of %.1f s, %d tasks, %d over 100 ms, longest %.0f ms  [%s]' % (
                main_thread['busy_share'] * 100, main_thread['window'] / 1000, main_thread['tasks'], main_thread['long'],
                main_thread['longest'], ' '.join('%s=%.0f' % item for item in main_thread['kinds'].items() if item[1])), file=out)
        for tier, sums in (entry.get('jit') or {}).items():
            if sums['n']:
                print('  jit %-8s %5d compiled (%d on the main thread)  waited %.0f ms each, longest %.0f ms  compiling %.0f ms' % (
                    tier, sums['n'], sums['main'], sums['wait'] / sums['n'], sums['max_wait'], sums['compile']), file=out)
        render = entry.get('render')
        if render:
            print('  render stage per frame: ' + ' '.join('%s=%.1f' % item for item in render['avg'].items() if item[1]), file=out)
            if render['worst_ms']:
                print('  worst render stage: %.0f ms  [%s]' % (render['worst_ms'], ' '.join(
                    '%s=%.0f' % item for item in render['worst'].items() if item[1])), file=out)
        for task in entry.get('long_tasks', []):
            script = task.get('script')
            print('  long task %5.0f ms  [%s]%s' % (task['ms'], ' '.join('%s=%.0f' % item for item in task['kinds'].items() if item[1]),
                  '  script %.0f ms %s %s' % (script['ms'], script['label'], script['detail']) if script else ''), file=out)
        if not entry['skipped'] and not any(s['windows'] for s in entry['presentations'].values()):
            print('  no frames presented', file=out)


def print_comparison(before, after, out):
    print('%-12s %-10s %8s %8s %8s %10s %10s' % ('benchmark', 'path', 'fps', '->', 'change', 'ms/frame', '->'), file=out)
    for name in list(dict.fromkeys(list(before) + list(after))):
        old = before.get(name, {}).get('presentations', {})
        new = after.get(name, {}).get('presentations', {})
        for presentation in list(dict.fromkeys(list(old) + list(new))):
            a, b = old.get(presentation), new.get(presentation)
            if not (a and a['windows']) and not (b and b['windows']):
                continue
            fa = a['fps'] if a and a['windows'] else None
            fb = b['fps'] if b and b['windows'] else None
            change = '%+.0f%%' % ((fb - fa) / fa * 100) if fa and fb is not None else '-'
            cell = lambda v: '%.1f' % v if v is not None else '-'
            print('%-12s %-10s %8s %8s %8s %10s %10s' % (
                name, presentation, cell(fa), cell(fb), change,
                cell(a['frame_ms'] if fa is not None else None),
                cell(b['frame_ms'] if fb is not None else None)), file=out)


def load(path):
    with open(path, encoding='utf-8', errors='replace') as log:
        return report(parse(log))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('logs', nargs='+')
    parser.add_argument('--compare', action='store_true', help='compare two logs: before, after')
    parser.add_argument('--json', action='store_true')
    arguments = parser.parse_args(argv)
    if arguments.compare:
        if len(arguments.logs) != 2:
            parser.error('--compare takes two logs')
        before, after = (load(path) for path in arguments.logs)
        if arguments.json:
            json.dump({'before': before, 'after': after}, sys.stdout, indent=2)
        else:
            print_comparison(before, after, sys.stdout)
        return 0
    for path in arguments.logs:
        result = load(path)
        if arguments.json:
            json.dump(result, sys.stdout, indent=2)
            print()
        else:
            if len(arguments.logs) > 1:
                print('== ' + path)
            print_report(result, sys.stdout)
    return 0


if __name__ == '__main__':
    sys.exit(main())

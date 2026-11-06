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
SLOW_MS = 1000 / 30


def new_presentation():
    return {'lines': 0, 'dropped': 0, 'frames': 0, 'window': 0.0, 'stages': {}, 'slow_windows': 0}


def parse(lines):
    """Returns {group: {'presentations': {...}, 'page': {...}, 'skipped': reason}} in log order."""
    groups = {}
    current = None

    def group(name):
        return groups.setdefault(name, {'presentations': {}, 'page': None, 'skipped': None, 'seen': set()})

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
        result[name] = {'presentations': presentations, 'page': entry['page'], 'skipped': entry['skipped']}
    return result


def print_report(result, out):
    if not result:
        print('no frame-stats lines (create C:\\Data\\WebKit\\frame-stats and restart the browser)', file=out)
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

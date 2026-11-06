#!/usr/bin/env python3
"""Host test for summarize-frame-stats.py on a synthetic browser log."""
import importlib.util
import io
import pathlib
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('summarize', HERE / 'summarize-frame-stats.py')
summarize = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summarize)

LOG = """\
10:00:00.000 WebKit Browser starting
10:00:00.100 frame-stats: enabled, fast counter 1000 Hz
10:00:01.000 console: file:///C:/Data/WebKit/bench/bench.js:48: bench: start scroll
10:00:02.000 frame-stats: screenplay frames=5 window=1900ms begin=1.00/2.00 paint=30.00/40.00 submit=1.00/1.00
10:00:03.000 frame-stats: screenplay frames=20 window=1000ms begin=1.00/3.00 paint=20.00/30.00 submit=1.00/2.00
10:00:04.000 frame-stats: screenplay frames=30 window=1000ms begin=2.00/4.00 paint=10.00/50.00 submit=1.00/1.00
10:00:05.000 frame-stats: screenplay frames=10 window=1000ms begin=1.00/1.00 paint=40.00/60.00 submit=1.00/1.00
10:00:05.500 console: file:///C:/Data/WebKit/bench/bench.js:65: bench: end scroll frames=70 ms=4500 fps=15.6 long=12 worst=180
10:00:07.000 console: file:///C:/Data/WebKit/bench/bench.js:37: bench: skip webgl no WebGL
10:00:08.000 console: file:///C:/Data/WebKit/bench/bench.js:48: bench: start composited
10:00:09.000 frame-stats: egl frames=3 window=2000ms update=1.00/1.00 backing=5.00/5.00 draw=2.00/2.00 swap=4.00/4.00
10:00:10.000 frame-stats: egl frames=25 window=1000ms update=1.00/2.00 backing=10.00/20.00 draw=3.00/4.00 swap=6.00/9.00
10:00:10.500 frame-stats: screenplay frames=2 window=1500ms begin=1.00/1.00 paint=5.00/5.00 submit=1.00/1.00
10:00:11.000 console: file:///C:/Data/WebKit/bench/bench.js:65: bench: end composited frames=30 ms=3000 fps=10.0 long=4 worst=90
"""


def close(a, b):
    assert abs(a - b) < 1e-6, (a, b)


def main():
    groups = summarize.report(summarize.parse(io.StringIO(LOG)))
    assert list(groups) == ['scroll', 'webgl', 'composited'], list(groups)

    scroll = groups['scroll']['presentations']['screenplay']
    # The first window (previous page's tail and idle time) is dropped.
    assert scroll['frames'] == 60 and scroll['windows'] == 3, scroll
    close(scroll['fps'], 20.0)
    close(scroll['stages']['paint']['avg'], (20 * 20 + 10 * 30 + 40 * 10) / 60)
    close(scroll['stages']['paint']['max'], 60.0)
    close(scroll['stages']['begin']['max'], 4.0)
    # Windows averaging over 33.3 ms per frame: only the 42 ms one.
    assert scroll['slow_windows'] == 1, scroll
    assert groups['scroll']['page']['fps'] == 15.6 and groups['scroll']['page']['long'] == 12

    assert groups['webgl']['skipped'] == 'no WebGL'

    composited = groups['composited']['presentations']
    assert composited['egl']['frames'] == 25 and composited['egl']['windows'] == 1
    close(composited['egl']['frame_ms'], 20.0)
    # A single screenplay line is the dropped first one: nothing presented.
    assert composited['screenplay']['windows'] == 0

    out = io.StringIO()
    summarize.print_report(groups, out)
    text = out.getvalue()
    assert 'screenplay  20.0 fps' in text and 'skipped: no WebGL' in text, text

    # Without markers, the whole log is one group and nothing is dropped.
    plain = summarize.report(summarize.parse(io.StringIO(
        '10:00:00.000 frame-stats: bitmap frames=10 window=500ms paint=5.00/9.00 blit=2.00/3.00\n')))
    bitmap = plain['(no benchmark)']['presentations']['bitmap']
    assert bitmap['frames'] == 10 and bitmap['windows'] == 1
    close(bitmap['fps'], 20.0)

    with tempfile.TemporaryDirectory() as directory:
        before = pathlib.Path(directory, 'before.txt')
        after = pathlib.Path(directory, 'after.txt')
        before.write_text(LOG)
        after.write_text(LOG.replace('frames=20 window=1000ms', 'frames=40 window=1000ms'))
        out = io.StringIO()
        summarize.print_comparison(summarize.load(before), summarize.load(after), out)
        row = [line for line in out.getvalue().splitlines() if line.startswith('scroll')][0]
        assert row.split()[2:5] == ['20.0', '26.7', '+33%'], row
    print('PASS: frame-stats summarizer')


if __name__ == '__main__':
    main()

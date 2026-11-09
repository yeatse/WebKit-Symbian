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
10:00:10.000 egl-tiles: 4096 KB of 32768 KB
10:00:10.200 egl-tiles: 6144 KB of 32768 KB
10:00:10.210 egl-draw: per frame quads=30.0 kpx=480 surfaces=9.0 rounded-clips=0.0 stencil-clips=1.0
10:00:10.220 egl-surfaces: per frame opacity=2.0 filter-mask=1.0 flatten=0.0 backdrop=0.0 filter-passes=2.0 kpx=150
10:00:10.300 image-decode: jpg-baseline window=1200ms n=3 partial=1 kpx=900 ms=60.0 max=40.0@800x600 large=1/40.0ms
10:00:10.300 image-decode: png window=1200ms n=2 partial=0 kpx=20 ms=4.0 max=3.0@100x100 large=0/0.0ms
10:00:10.400 image-decode: jpg-baseline window=1100ms n=1 partial=0 kpx=100 ms=10.0 max=10.0@400x250 large=0/0.0ms
10:00:10.410 main-thread: window=1000ms busy=800ms tasks=40 long=2 longest=450ms other=100 script=400 style=150 layout=100 parse=0 paint=30 decode=0 gc=20 frame=0
10:00:10.420 main-thread: window=1000ms busy=600ms tasks=30 long=1 longest=120ms other=50 script=300 style=150 layout=50 parse=0 paint=30 decode=0 gc=20 frame=0
10:00:10.430 long-task: 450ms other=10 script=400 style=30 layout=10 parse=0 paint=0 decode=0 gc=0 frame=0 top-script=420ms event click
10:00:10.440 long-task: 120ms other=120 script=0 style=0 layout=0 parse=0 paint=0 decode=0 gc=0 frame=0
10:00:10.450 long-task: 300ms other=0 script=280 style=20 layout=0 parse=0 paint=0 decode=0 gc=0 frame=0 top-script=290ms eval https://www.youtube.com/s/desktop/base.js
10:00:10.460 egl-render: per frame other=0.5 script=20.0 style=8.0 layout=4.0 parse=0.0 paint=0.0 decode=0.0 gc=1.0 frame=0.0
10:00:10.470 egl-render-worst: 900ms other=0 script=850 style=40 layout=10 parse=0 paint=0 decode=0 gc=0 frame=0
10:00:10.480 egl-render: per frame other=1.5 script=10.0 style=4.0 layout=2.0 parse=0.0 paint=0.0 decode=0.0 gc=3.0 frame=0.0
10:00:10.490 egl-render-worst: 200ms other=0 script=150 style=40 layout=10 parse=0 paint=0 decode=0 gc=0 frame=0
10:00:10.500 frame-stats: screenplay frames=2 window=1500ms begin=1.00/1.00 paint=5.00/5.00 submit=1.00/1.00
10:00:11.000 console: file:///C:/Data/WebKit/bench/bench.js:65: bench: end composited frames=30 ms=3000 fps=10.0 long=4 worst=90
"""


def close(a, b):
    assert abs(a - b) < 1e-6, (a, b)


def main():
    groups = summarize.report(summarize.parse(io.StringIO(LOG)))
    assert list(groups) == ['scroll', 'webgl', 'composited'], list(groups)
    decode = groups['composited']['decode']
    assert list(decode) == ['jpg-baseline', 'png'], list(decode)
    jpeg = decode['jpg-baseline']
    assert (jpeg['n'], jpeg['partial'], jpeg['kpx'], jpeg['large']) == (4, 1, 1000, 1), jpeg
    close(jpeg['ms'], 70.0)
    close(jpeg['max'], 40.0)
    assert jpeg['max_size'] == '800x600'
    assert groups['scroll']['decode'] == {}

    main_thread = groups['composited']['main']
    assert (main_thread['tasks'], main_thread['long'], main_thread['longest']) == (70, 3, 450.0), main_thread
    close(main_thread['busy_share'], 0.7)
    close(main_thread['kinds']['script'], 700.0)
    tasks = groups['composited']['long_tasks']
    assert [task['ms'] for task in tasks] == [450.0, 300.0, 120.0], tasks
    assert tasks[0]['script'] == {'ms': 420.0, 'label': 'event', 'detail': 'click'}, tasks[0]
    assert tasks[1]['script']['detail'] == 'https://www.youtube.com/s/desktop/base.js'
    assert 'script' not in tasks[2] and tasks[2]['kinds']['other'] == 120
    render = groups['composited']['render']
    close(render['avg']['script'], 15.0)
    close(render['avg']['gc'], 2.0)
    assert render['worst_ms'] == 900 and render['worst']['script'] == 850, render
    assert groups['scroll']['render'] is None and groups['scroll']['long_tasks'] == []

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
    assert groups['composited']['tiles_kb'] == 6144 and groups['scroll']['tiles_kb'] is None
    draw = groups['composited']['draw']
    assert draw['kpx'] == 480 and draw['surfaces'] == 9 and draw['stencil-clips'] == 1, draw
    assert draw['surface-opacity'] == 2 and draw['surface-filter-mask'] == 1 and draw['surface-kpx'] == 150, draw
    # A single screenplay line is the dropped first one: nothing presented.
    assert composited['screenplay']['windows'] == 0

    out = io.StringIO()
    summarize.print_report(groups, out)
    text = out.getvalue()
    assert 'screenplay  20.0 fps' in text and 'skipped: no WebGL' in text, text
    assert 'GPU tiles: up to 6144 KB' in text, text
    assert 'main thread: busy 70% of 2.0 s, 70 tasks, 3 over 100 ms, longest 450 ms' in text, text
    assert 'long task   450 ms  [other=10 script=400 style=30 layout=10]  script 420 ms event click' in text, text
    assert 'worst render stage: 900 ms  [script=850 style=40 layout=10]' in text, text

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

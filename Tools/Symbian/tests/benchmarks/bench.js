// Shared runner for the frame-rate benchmarks. Each page runs for a fixed
// time and stops by itself; with ?all the pages run one after another and
// end on the index. Markers go to the browser log through the console:
//   bench: start NAME
//   bench: end NAME frames=N ms=T fps=F long=L worst=W
//   bench: skip NAME REASON
// summarize-frame-stats.py groups the log's frame-stats lines by them.
(function () {
    'use strict';
    var ORDER = ['scroll', 'images', 'animation', 'composited', 'canvas', 'webgl'];
    var all = /[?&]all\b/.test(location.search);

    function status(text) {
        var element = document.getElementById('bench-status');
        if (!element) {
            element = document.createElement('div');
            element.id = 'bench-status';
            element.style.cssText = 'position:fixed;left:0;right:0;bottom:0;padding:4px 8px;' +
                'background:rgba(0,0,0,.75);color:#fff;font:12px/16px monospace;z-index:100';
            document.body.appendChild(element);
        }
        element.textContent = text;
    }

    function next(name) {
        if (!all)
            return;
        var index = ORDER.indexOf(name);
        var target = index >= 0 && index + 1 < ORDER.length ? ORDER[index + 1] + '.html?all' : 'index.html?done';
        setTimeout(function () { location.href = target; }, 1500);
    }

    // frame(elapsedMs) runs once per animation frame. setup() may return a
    // string to skip the page (a missing feature).
    window.bench = function (name, options) {
        var duration = options.duration || 15000;
        window.addEventListener('load', function () {
            var reason = options.setup ? options.setup() : null;
            if (reason) {
                console.log('bench: skip ' + name + ' ' + reason);
                status(name + ': skipped (' + reason + ')');
                next(name);
                return;
            }
            status(name + ': starting');
            // Let layout and the first paint settle before measuring.
            setTimeout(function () {
                console.log('bench: start ' + name);
                var start = performance.now(), last = start, frames = 0, slow = 0, worst = 0;
                function tick(now) {
                    var interval = now - last;
                    last = now;
                    if (frames) {
                        if (interval > 50)
                            ++slow;
                        if (interval > worst)
                            worst = interval;
                    }
                    ++frames;
                    var elapsed = now - start;
                    if (elapsed >= duration) {
                        if (options.finish)
                            options.finish();
                        var fps = frames * 1000 / elapsed;
                        console.log('bench: end ' + name + ' frames=' + frames + ' ms=' + Math.round(elapsed) +
                            ' fps=' + fps.toFixed(1) + ' long=' + slow + ' worst=' + Math.round(worst));
                        status(name + ': ' + fps.toFixed(1) + ' fps, ' + slow + ' frames over 50 ms, worst ' +
                            Math.round(worst) + ' ms');
                        next(name);
                        return;
                    }
                    if (options.frame)
                        options.frame(elapsed);
                    if (frames % 30 === 0)
                        status(name + ': ' + Math.ceil((duration - elapsed) / 1000) + ' s left');
                    requestAnimationFrame(tick);
                }
                requestAnimationFrame(tick);
            }, 1000);
        });
    };

    // Scrolls the page down and back up, a few pixels per frame.
    window.benchScroller = function (step) {
        var direction = 1;
        return function () {
            var bottom = document.documentElement.scrollHeight - window.innerHeight;
            var y = window.pageYOffset + direction * step;
            if (y >= bottom) { y = bottom; direction = -1; }
            if (y <= 0) { y = 0; direction = 1; }
            window.scrollTo(0, y);
        };
    };
})();

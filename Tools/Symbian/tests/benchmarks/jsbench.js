// JavaScript speed by kind of work, to compare the engine's tiers on a
// phone. Each case does a fixed amount of work and returns a checksum, so a
// wrong result shows as well as a slow one. Lines go to the browser log:
//   jsbench: NAME round=R ms=T sum=S
//   jsbench: done total=T
(function (global) {
    'use strict';

    function integers(n) {
        var sum = 0;
        for (var i = 0; i < n; ++i)
            sum = (sum + ((i * 7) ^ (i >>> 3)) % 1013) | 0;
        return sum;
    }

    function doubles(n) {
        var x = 0.5, sum = 0;
        for (var i = 0; i < n; ++i) {
            x = x * 1.0000001 + 0.25 / (i + 1);
            sum += Math.sqrt(x) * 0.5;
        }
        return Math.floor(sum);
    }

    function Point(x, y) { this.x = x; this.y = y; }
    Point.prototype.add = function (other) { return new Point(this.x + other.y, this.y + other.x); };
    Point.prototype.length = function () { return this.x + this.y; };
    function objects(n) {
        var point = new Point(1, 2), step = new Point(3, 1), sum = 0;
        for (var i = 0; i < n; ++i) {
            point = point.add(step);
            if (point.x > 100000)
                point = new Point(1, 2);
            sum = (sum + point.length()) | 0;
        }
        return sum;
    }

    function arrays(n) {
        var values = [], i;
        for (i = 0; i < n; ++i)
            values.push((i * 2654435761) % 100003);
        var mapped = values.map(function (value) { return value % 977; })
            .filter(function (value) { return value & 1; });
        mapped.sort(function (a, b) { return a - b; });
        var sum = 0;
        for (i = 0; i < mapped.length; ++i)
            sum = (sum + mapped[i] * (i & 7)) | 0;
        return sum;
    }

    function strings(n) {
        var words = ['alpha', 'beta', 'gamma', 'delta', 'epsilon'], table = {}, sum = 0;
        for (var i = 0; i < n; ++i) {
            var key = words[i % 5] + '-' + (i % 211);
            table[key] = (table[key] || 0) + 1;
            sum = (sum + key.indexOf('a') + key.charCodeAt(key.length - 1) + key.toUpperCase().length) | 0;
        }
        return sum + Object.keys(table).length;
    }

    function parsing(n) {
        var sum = 0;
        for (var i = 0; i < n; ++i) {
            var text = JSON.stringify({ id: i, name: 'item' + i, tags: ['a', 'b', i % 3], nested: { on: i % 2 === 0 } });
            var value = JSON.parse(text);
            sum = (sum + value.tags.length + value.name.length + (/item(\d+)/.exec(value.name)[1] | 0) % 7) | 0;
        }
        return sum;
    }

    var CASES = [
        ['integers', integers, 1000000],
        ['doubles', doubles, 500000],
        ['objects', objects, 300000],
        ['arrays', arrays, 30000],
        ['strings', strings, 60000],
        ['parsing', parsing, 8000]
    ];
    var ROUNDS = 3;
    global.jsbenchCases = CASES;
    if (typeof document === 'undefined')
        return;

    // Date.now() counted whole seconds in builds before this page's second
    // revision; performance.now() uses the steady clock.
    var clock = global.performance && performance.now ? function () { return performance.now(); } : Date.now;
    var table = document.getElementById('results');
    var total = 0, caseIndex = 0, round = 0, row = null;

    // The smallest step Date.now() takes, in milliseconds: a coarse wall
    // clock shows here.
    function dateStep() {
        var first = Date.now(), next = first, spins = 0;
        while (next === first && spins++ < 2000000)
            next = Date.now();
        return next - first;
    }

    function step() {
        if (caseIndex === CASES.length) {
            console.log('jsbench: done total=' + total + ' date-step=' + dateStep());
            document.getElementById('done').style.display = 'block';
            return;
        }
        var entry = CASES[caseIndex];
        if (!round) {
            row = table.insertRow(-1);
            row.insertCell(-1).textContent = entry[0];
        }
        var start = clock();
        var sum = entry[1](entry[2]);
        var elapsed = Math.round(clock() - start);
        total += elapsed;
        row.insertCell(-1).textContent = elapsed;
        console.log('jsbench: ' + entry[0] + ' round=' + (round + 1) + ' ms=' + elapsed + ' sum=' + sum);
        if (++round === ROUNDS) {
            round = 0;
            ++caseIndex;
        }
        // Between rounds, so that the page paints and compiler threads run.
        setTimeout(step, 100);
    }
    setTimeout(step, 500);
})(typeof window !== 'undefined' ? window : globalThis);

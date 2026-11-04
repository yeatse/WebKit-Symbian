// Run with --useSharedArrayBuffer=true to cover both RAB and GSAB.
(function () {
    function check(condition, label) {
        if (!condition)
            throw new Error(label);
    }
    function throwsRangeError(fn, label) {
        try {
            fn();
        } catch (error) {
            check(error instanceof RangeError, label + " exception type");
            return;
        }
        throw new Error(label + " did not throw");
    }
    const buffer = new ArrayBuffer(100, { maxByteLength: 196608 });
    const tracking = new Uint8Array(buffer);
    const fixed = new Uint8Array(buffer, 0, 100);
    tracking[0] = 17;
    tracking[99] = 29;
    buffer.resize(131073);
    check(buffer.resizable && buffer.maxByteLength === 196608, "RAB properties");
    check(tracking.length === 131073 && fixed.length === 100, "tracking and fixed length");
    check(tracking[0] === 17 && tracking[99] === 29, "growth preserves data");
    check(tracking.slice(100).every(value => value === 0), "growth zeroes data");
    tracking[70000] = 83;
    buffer.resize(50);
    check(tracking.length === 50 && fixed.length === 0, "shrink makes fixed view out of bounds");
    buffer.resize(131073);
    check(tracking[0] === 17 && tracking[99] === 0 && tracking[70000] === 0, "regrowth zeroes truncated bytes");
    check(fixed.length === 100, "fixed view becomes in bounds again");
    buffer.resize(0);
    buffer.resize(65537);
    check(tracking.every(value => value === 0), "zero length regrowth");
    throwsRangeError(() => buffer.resize(196609), "RAB maximum");
    check(buffer.byteLength === 65537, "failed resize keeps length");
    throwsRangeError(() => buffer.resize(-1), "negative RAB length");
    const shared = new SharedArrayBuffer(100, { maxByteLength: 196608 });
    const sharedView = new Uint8Array(shared);
    sharedView[99] = 31;
    shared.grow(131073);
    check(shared.growable && shared.maxByteLength === 196608, "GSAB properties");
    check(sharedView.length === 131073 && sharedView[99] === 31, "GSAB growth preserves data");
    check(sharedView.slice(100).every(value => value === 0), "GSAB growth zeroes data");
    throwsRangeError(() => shared.grow(100), "GSAB cannot shrink");
    throwsRangeError(() => shared.grow(196609), "GSAB maximum");
    check(shared.byteLength === 131073 && sharedView[99] === 31, "failed grow preserves state");
    for (let i = 0; i < 20; ++i) {
        let temporary = new ArrayBuffer(1, { maxByteLength: 131072 });
        temporary.resize(65537);
        temporary.resize(0);
        temporary = null;
        gc();
    }
    gc();
    check(sharedView[99] === 31, "GC preserves live shared memory");
    print("PASS: resizable-arraybuffer");
})();

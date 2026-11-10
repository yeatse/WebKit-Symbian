// Run in jsc with an outer timeout of 180s. Uses real small JS objects, not
// ArrayBuffer backing stores or a large root array, to cross the old 64MiB pool.
(function () {
    "use strict";
    const started = performance.now();
    const target = 72 * 1024 * 1024;
    function check(value, message) { if (!value) throw new Error(message); }
    function deadline() { check(performance.now() - started < 150000, "cell pressure deadline"); }
    function Cell(next, value) {
        this.next = next; this.value = value;
        this.a = value ^ 0x1234; this.b = value + 7;
        this.c = value - 9; this.d = value ^ 0x5678;
    }
    function statistics() {
        const s = memoryUsageStatistics();
        return { objectBytes: s.heapSize - s.extraMemorySize,
            objectCapacity: s.heapCapacity - s.extraMemorySize,
            extraBytes: s.extraMemorySize, objects: s.objectCount };
    }
    function retain() {
        let count = 0;
        globalThis.alignedPressureHead = null;
        let measured;
        do {
            deadline();
            for (let i = 0; i < 131072; ++i) {
                globalThis.alignedPressureHead = new Cell(globalThis.alignedPressureHead, count);
                ++count;
            }
            fullGC(); measured = statistics();
            print("PROGRESS " + JSON.stringify({ count, ...measured }));
            check(count <= 2621440, "object count bound exceeded before target");
        } while (measured.objectBytes < target);
        return { count, ...measured };
    }
    function verify(count) {
        let node = globalThis.alignedPressureHead;
        for (let expected = count - 1; expected >= 0; --expected) {
            check(node && node.value === expected && node.a === (expected ^ 0x1234)
                && node.b === expected + 7 && node.c === expected - 9
                && node.d === (expected ^ 0x5678), "retained cell content or chain corrupt");
            node = node.next;
        }
        check(node === null, "chain length mismatch");
    }
    try {
        check(typeof fullGC === "function" && typeof memoryUsageStatistics === "function", "jsc heap APIs required");
        fullGC(); const baseline = statistics();
        const retained = retain(); verify(retained.count); deadline();
        check(retained.objectBytes >= target && retained.objects >= retained.count, "real cell target not reached");
        globalThis.alignedPressureHead = null;
        fullGC(); const released = statistics();
        check(released.objectBytes < retained.objectBytes / 2, "released object graph remains live");
        // Rebuild the same graph once to exercise post-GC allocation/reuse.
        const reused = retain(); verify(reused.count); deadline();
        globalThis.alignedPressureHead = null;
        fullGC(); const final = statistics();
        check(final.objectBytes < reused.objectBytes / 2, "second graph remains live after GC");
        print("RESULT " + JSON.stringify({ test: "jsc-cell-pressure", target, baseline, retained,
            released, reused, final, elapsedMS: performance.now() - started }));
        print("PASS: jsc-cell-pressure");
    } catch (error) {
        print("FAIL: jsc-cell-pressure " + error.message);
        throw error;
    }
})();

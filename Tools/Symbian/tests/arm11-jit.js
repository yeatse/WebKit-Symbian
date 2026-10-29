// Run through qemu/run-jit-tests.py. These checks fail if a requested tier never executes.
(function () {
    "use strict";
    const mode = argumentsFromShell();
    function argumentsFromShell() { return globalThis.arguments && globalThis.arguments[0]; }
    function check(condition, message) { if (!condition) throw new Error(message); }
    check(["llint", "baseline", "dfg"].includes(mode), "Expected llint, baseline or dfg argument");
    check(typeof $vm === "object", "--useDollarVM=true is required");
    check($vm.useJIT() === (mode !== "llint"), "Requested JIT mode was not enabled");
    check($vm.useDFGJIT() === (mode === "dfg"), "Requested DFG mode was not enabled");
    check(!$vm.useFTLJIT(), "This ARM11 test does not permit FTL");
    const completed = [], digest = [], seen = {}, functions = {};
    function finish(name, value) { completed.push(name); digest.push(value); print("PASS: " + name); }
    function register(name, fn) { noInline(fn); functions[name] = fn; }
    function warmed(name, fn, argumentsForIteration) {
        register(name, fn);
        for (let i = 0; i < 4000; ++i)
            fn(...argumentsForIteration(i));
        if (mode === "dfg") {
            const count = numberOfDFGCompiles(fn);
            check(count > 0 && count !== 1000000, name + " has no genuine DFG compilation count");
            check(seen[name] === true, name + " never executed its DFG body");
        }
    }

    function tierProbe(value) {
        return [$vm.llintTrue(), $vm.baselineJITTrue(), $vm.dfgTrue(), value + 1];
    }
    noInline(tierProbe);
    let tiers = [false, false, false];
    for (let i = 0; i < 5000; ++i) {
        const result = tierProbe(i);
        check(result[3] === i + 1, "tier probe value");
        for (let tier = 0; tier < 3; ++tier) tiers[tier] ||= result[tier];
    }
    check(tiers[{llint: 0, baseline: 1, dfg: 2}[mode]], "Requested execution tier was never observed");
    check(mode === "dfg" || !tiers[2], "DFG ran in a profile that disabled it");
    finish("execution-tier", 5000);

    function enterDuringLoop(count) {
        const startedInDFG = $vm.dfgTrue();
        let sum = 0, enteredDFG = false;
        for (let i = 0; i < count; ++i) {
            sum = (sum + i) | 0;
            if ($vm.dfgTrue()) enteredDFG = true;
        }
        return {sum, startedInDFG, enteredDFG};
    }
    register("osr-entry", enterDuringLoop);
    const entry = enterDuringLoop(200000);
    check(entry.sum === (200000 * 199999 / 2 | 0), "OSR entry lost live loop values");
    if (mode === "dfg") check(!entry.startedInDFG && entry.enteredDFG, "No OSR entry during the first invocation");
    finish("osr-entry", entry.sum);

    function exitWithLiveValues(value, object, forceExit) {
        const integer = value * 3 | 0, fraction = value + 0.125;
        const before = $vm.dfgTrue();
        if (before) seen["osr-exit"] = true;
        if (forceExit) OSRExit();
        return [integer + object.delta, fraction, before, $vm.dfgTrue()];
    }
    warmed("osr-exit", exitWithLiveValues, i => [i & 31, {delta: 7}, false]);
    const exited = exitWithLiveValues(13, {delta: 7}, true);
    check(exited[0] === 46 && exited[1] === 13.125, "OSR exit recovery lost int/double/object values");
    if (mode === "dfg") check(exited[2] && !exited[3], "No observed DFG-to-baseline OSR exit");
    finish("osr-exit", exited[0] + exited[1]);

    function propertyKernel(object, key, value) {
        object[key] = value;
        const answer = object.x + object[key];
        if ($vm.dfgTrue()) seen["inline-caches"] = true;
        return answer;
    }
    const mono = {x: 7, y: 0};
    warmed("inline-caches", propertyKernel, i => [mono, "y", i & 63]);
    const shapes = [{x: 9, y: 0}, {pad: true, x: 11, y: 0}, Object.create({x: 13})];
    let icSum = 0;
    for (let i = 0; i < 1200; ++i) {
        const object = shapes[i % shapes.length], value = i & 31;
        const answer = propertyKernel(object, "y", value);
        check(answer === object.x + value, "get/put IC shape transition");
        icSum += answer;
    }
    const accessor = {get x() { return 17; }};
    check(propertyKernel(accessor, "z", 2) === 19, "get-by-id accessor slow path");
    finish("inline-caches", icSum);

    function add(a, b, c) { return a + b + c; }
    function callKernel(fn, a, b) {
        const answer = fn(a, b, 4);
        if ($vm.dfgTrue()) seen["call-arity"] = true;
        return answer;
    }
    noInline(add);
    warmed("call-arity", callKernel, i => [add, i & 31, 2]);
    const closure = offset => function(a, b) { return a + b + offset + arguments.length; };
    check(callKernel(closure(5), 10, 20) === 38, "call IC/closure/extra arguments");
    check(callKernel(add, 1, 2) === 7, "call IC returns to original target");
    finish("call-arity", 45);

    function softFloatKernel(a, b, c, d, e, f, g, h) {
        const answer = Math.atan2(a, b) + Math.sin(c) + Math.cos(d) + Math.sqrt(e) + Math.log(f) + Math.exp(g) + h;
        if ($vm.dfgTrue()) seen["softfp-math"] = true;
        return answer;
    }
    warmed("softfp-math", softFloatKernel, () => [0, 1, 0, 0, 4, 1, 0, 0.125]);
    check(softFloatKernel(0, 1, 0, 0, 4, 1, 0, 0.125) === 4.125, "softfp register/stack/libm calling convention");
    check(Math.abs(softFloatKernel(0, 1, 0.5, 0, 4, 1, 0, 0.125) - 4.604425538604203) < 1e-12, "nonconstant libm result");
    finish("softfp-math", 4.125);

    function numberKernel(a, b) {
        const answer = (a + b) * 1.25 - a / 2;
        if ($vm.dfgTrue()) seen["number-guards"] = true;
        return answer;
    }
    warmed("number-guards", numberKernel, i => [i & 31, 1]);
    check(numberKernel(2147483647, 1) === 1610612736.5, "int32 overflow recovery");
    check(Number.isNaN(numberKernel(NaN, 1)), "NaN arithmetic");
    check(Object.is(Math.atan2(-0, 1), -0), "negative zero softfp result");
    check(numberKernel(2.5, -0.5) === 1.25, "double speculation recovery");
    check(numberKernel("3", "4") === 41, "math IC int/double/string transition");
    finish("number-guards", 1610612737.75);

    function bitwiseKernel(value, shift) {
        const answer = [value >> 0, value >>> 0, value >> shift, value >>> shift,
            value << shift, (value >>> 0) + 0.5];
        if ($vm.dfgTrue()) seen["bitwise-unsigned"] = true;
        return answer;
    }
    warmed("bitwise-unsigned", bitwiseKernel, i => [i & 1 ? -1 : -2147483648, i & 63]);
    const shifts = [[0, -2147483647, 2147483649, -2147483647],
        [1, -1073741824, 1073741824, 2], [31, -1, 1, -2147483648],
        [32, -2147483647, 2147483649, -2147483647],
        [64, -2147483647, 2147483649, -2147483647], [-1, -1, 1, -2147483648]];
    for (const [shift, signed, unsigned, left] of shifts) {
        const value = bitwiseKernel(-2147483647, shift);
        check(value[0] === -2147483647 && value[1] === 2147483649, "zero-bit signed/unsigned shift");
        check(value[2] === signed && value[3] === unsigned && value[4] === left, "ARM shift count modulo 32: " + shift);
    }
    check(bitwiseKernel(-2147483648, 0)[5] === 2147483648.5, "uint32 0x80000000 to double");
    check(bitwiseKernel(-1, 0)[5] === 4294967295.5, "uint32 0xffffffff to double");
    finish("bitwise-unsigned", 4294967295.5);

    function compareKernel(a, b) {
        const answer = (a == b ? 1 : 0) | (a != b ? 2 : 0) | (a < b ? 4 : 0)
            | (a <= b ? 8 : 0) | (a > b ? 16 : 0) | (a >= b ? 32 : 0);
        // Require a NaN comparison to execute in DFG, not only a finite warm-up.
        if ($vm.dfgTrue() && Number.isNaN(a)) seen["double-comparisons"] = true;
        return answer;
    }
    const comparisons = [NaN, 0.5, -1.25, Infinity, -Infinity, 0, -0];
    warmed("double-comparisons", compareKernel, i => [comparisons[i % 7], comparisons[(i + 3) % 7]]);
    check(compareKernel(NaN, 1) === 2 && compareKernel(1, NaN) === 2 && compareKernel(NaN, NaN) === 2, "unordered ARM floating-point conditions");
    check(compareKernel(1, 1) === 41 && compareKernel(1, 2) === 14 && compareKernel(2, 1) === 50, "ordered floating-point conditions");
    check(compareKernel(-0, 0) === 41 && compareKernel(-Infinity, Infinity) === 14, "signed zero/infinity comparisons");
    finish("double-comparisons", 162);

    function gcKernel(seed, collect) {
        const object = {value: seed + 1, payload: new Float64Array([seed + 0.5, seed + 1.5])};
        const before = $vm.dfgTrue();
        if (before) seen["gc-live-values"] = true;
        if (collect) gc();
        return object.value + object.payload[0] + object.payload[1];
    }
    warmed("gc-live-values", gcKernel, i => [i & 31, false]);
    for (let i = 0; i < 12; ++i)
        check(gcKernel(i, true) === 3 * i + 3, "GC lost live object/typed-array values");
    finish("gc-live-values", 234);

    function typedKernel(view, index) {
        const before = $vm.dfgTrue();
        const answer = view[index];
        const after = $vm.dfgTrue();
        if (after) seen["rab-gsab"] = true;
        return {value: answer, before, after};
    }
    // JSVALUE32_64 deliberately exits optimized typed-array access for RAB/GSAB
    // (UnexpectedResizableArrayBufferView). First prove the ordinary load runs
    // in DFG, then require that guard to recover into baseline for a RAB. The
    // first different view structure can exit at an earlier CheckStructure;
    // bounded warm-up lets the profile include that structure too.
    const ordinary = new Uint8Array(128);
    ordinary[0] = 17;
    warmed("rab-gsab", typedKernel, () => [ordinary, 0]);
    const buffer = new ArrayBuffer(128, {maxByteLength: 256});
    const tracking = new Uint8Array(buffer), fixed = new Uint8Array(buffer, 0, 128);
    tracking[0] = 17; tracking[127] = 29;
    let rabTransition;
    for (let attempt = 1; attempt <= 4000; ++attempt) {
        rabTransition = typedKernel(tracking, 0);
        rabTransition.attempts = attempt;
        check(rabTransition.value === 17, "RAB guard exit lost the loaded value");
        if (mode !== "dfg" || (rabTransition.before && !rabTransition.after)) break;
    }
    if (mode === "dfg") check(rabTransition.before && !rabTransition.after, "No observed 32-bit RAB guard OSR exit");
    buffer.resize(64);
    check(typedKernel(tracking, 127).value === undefined && typedKernel(fixed, 0).value === undefined, "RAB shrink invalidates compiled bounds");
    buffer.resize(256);
    gc();
    check(typedKernel(tracking, 0).value === 17 && typedKernel(tracking, 127).value === 0 && fixed.length === 128, "RAB regrowth/GC preserves only live bytes");
    const shared = new SharedArrayBuffer(16, {maxByteLength: 64}), sharedView = new Int32Array(shared);
    Atomics.store(sharedView, 0, 9); shared.grow(64);
    check(sharedView.length === 16 && Atomics.add(sharedView, 0, 3) === 9 && typedKernel(sharedView, 0).value === 12, "GSAB growth/atomic access");
    finish("rab-gsab", 29);

    function exceptionKernel(object) {
        try {
            const answer = object.get();
            if ($vm.dfgTrue()) seen["exceptions"] = true;
            return answer;
        } catch (error) { return error.value + 1; }
    }
    const normalExceptionObject = {get() { return 41; }};
    warmed("exceptions", exceptionKernel, () => [normalExceptionObject]);
    check(exceptionKernel({get() { throw {value: 41}; }}) === 42, "exception unwinding from optimized call");
    finish("exceptions", 42);

    const compiled = {};
    // TestRunnerUtils returns a 1000000 sentinel when DFG is disabled. Keep raw
    // values for diagnosis; they are compilation evidence only in the DFG run.
    for (const name of Object.keys(functions)) compiled[name] = numberOfDFGCompiles(functions[name]);
    print("RESULT " + JSON.stringify({mode, completed, digest, tiers, compiled, dfgBodies: seen,
        osrEntry: entry, osrExit: {before: exited[2], after: exited[3]}, rabTransition}));
    print("PASS: arm11-jit " + completed.length + "/" + completed.length);
})();

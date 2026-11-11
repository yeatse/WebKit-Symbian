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
        if (forceExit) check(OSRExit() === undefined, "OSR exit result");
        return [integer + object.delta, fraction, before, $vm.dfgTrue()];
    }
    warmed("osr-exit", exitWithLiveValues, i => [i & 31, {delta: 7}, i < 2]);
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

    function truncKernel(value) {
        const answer = Math.trunc(value);
        if ($vm.dfgTrue()) seen["math-trunc"] = true;
        return answer;
    }
    // Include non-int32 results before tiering so this call site retains the
    // double result, including negative zero, rather than an int32-only use.
    const truncWarmup = [0.75, 1.5, 4095.875, -0.25, 2147483648.5];
    warmed("math-trunc", truncKernel, i => [truncWarmup[i % truncWarmup.length]]);
    const truncCases = [[0.75, 0], [1.5, 1], [-1.5, -1], [-0.25, -0], [0, 0], [-0, -0],
        [Number.MIN_VALUE, 0], [-Number.MIN_VALUE, -0],
        [2147483647, 2147483647], [2147483647.75, 2147483647],
        [-2147483648, -2147483648], [-2147483648.75, -2147483648],
        [4294967296.5, 4294967296], [-4294967296.5, -4294967296],
        [9007199254740992, 9007199254740992], [-9007199254740992, -9007199254740992],
        [NaN, NaN], [Infinity, Infinity], [-Infinity, -Infinity]];
    for (const [value, expected] of truncCases) {
        seen["math-trunc"] = false;
        check(Object.is(truncKernel(value), expected), "Math.trunc result for " + value);
        if (mode === "dfg") check(seen["math-trunc"], "Math.trunc result did not execute in DFG");
    }
    finish("math-trunc", truncCases.length);

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

    function powerOfTwoModuloKernel(value) {
        const answer = [value % 2, value % 256, value % 1073741824];
        if ($vm.dfgTrue()) seen["power-of-two-modulo"] = true;
        return answer;
    }
    // Signed odd inputs exercise all three integer paths without -0 exits.
    warmed("power-of-two-modulo", powerOfTwoModuloKernel,
        i => [(i & 1 ? -1 : 1) * ((i & 511) * 2 + 1)]);
    const negativeRemainders = powerOfTwoModuloKernel(-257);
    check(negativeRemainders[0] === -1 && negativeRemainders[1] === -1
        && negativeRemainders[2] === -257, "signed power-of-two modulo");
    const boundaryRemainders = powerOfTwoModuloKernel(2147483647);
    check(boundaryRemainders[0] === 1 && boundaryRemainders[1] === 255
        && boundaryRemainders[2] === 1073741823, "int32 boundary power-of-two modulo");
    check(powerOfTwoModuloKernel(-2147483648).every(value => Object.is(value, -0)),
        "negative multiples preserve negative zero after optimized modulo");
    check(powerOfTwoModuloKernel(-0).every(value => Object.is(value, -0)),
        "negative-zero input survives modulo speculation");
    const fractionalRemainders = powerOfTwoModuloKernel(2.5);
    check(fractionalRemainders[0] === 0.5 && fractionalRemainders[1] === 2.5
        && fractionalRemainders[2] === 2.5, "double fallback after integer modulo warmup");
    finish("power-of-two-modulo", boundaryRemainders[2] + fractionalRemainders[0]);

    // ARM11 has no divide instruction: DFG calls a helper for int32 %, the
    // lower tiers take an integer path inside fmod.
    function referenceMod(a, b) {
        if (Number.isNaN(a) || Number.isNaN(b) || !Number.isFinite(a) || b === 0) return NaN;
        if (!Number.isFinite(b)) return a;
        const result = a - Math.trunc(a / b) * b;
        return result === 0 && (a < 0 || Object.is(a, -0)) ? -0 : result;
    }
    function modKernel(a, b) {
        const answer = a % b;
        if ($vm.dfgTrue()) seen["int-modulo"] = true;
        return answer;
    }
    function modToIntKernel(a, b) { return (a % b) | 0; }
    function modConstantKernel(a) { return [a % 8, a % 10, a % -3, a % 1, (a % 16) | 0, (a % 7) | 0]; }
    noInline(modToIntKernel); noInline(modConstantKernel);
    warmed("int-modulo", modKernel, i => [i * 7919 & 0x7fffffff, (i & 255) + 1]);
    const modValues = [0, 1, 2, 3, 5, 7, 8, 10, 16, 1000, 65536, 2147483646, 2147483647,
        -1, -2, -3, -7, -8, -10, -65536, -2147483647, -2147483648];
    const modOddValues = [-0, 0.5, -2.5, 4294967296, NaN, Infinity, -Infinity];
    let modDigest = 0;
    for (let round = 0; round < 60; ++round) {
        // The last rounds add values that are not int32, after the int32 code is hot.
        const values = round < 55 ? modValues : modValues.concat(modOddValues);
        for (const a of values) {
            for (const b of values) {
                const expected = referenceMod(a, b), actual = modKernel(a, b);
                check(Object.is(actual, expected), "modulo " + a + " % " + b + " gave " + actual + " in round " + round);
                check(modToIntKernel(a, b) === (expected | 0), "int modulo " + a + " % " + b + " in round " + round);
                if (!round && Number.isFinite(expected)) modDigest += expected;
            }
            const constants = modConstantKernel(a);
            [8, 10, -3, 1].forEach((b, index) => check(Object.is(constants[index], referenceMod(a, b)), "constant modulo " + a + " % " + b + " in round " + round));
            check(constants[4] === (referenceMod(a, 16) | 0) && constants[5] === (referenceMod(a, 7) | 0), "constant int modulo of " + a + " in round " + round);
        }
    }
    finish("int-modulo", modDigest);

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

    function strictEqualityValue(a, b) {
        const answer = a === b;
        if ($vm.dfgTrue()) seen["strict-equality-value"] = true;
        return answer;
    }
    function strictEqualityBranch(a, b) {
        if (a === b) {
            if ($vm.dfgTrue()) seen["strict-equality-branch"] = true;
            return 7;
        }
        if ($vm.dfgTrue()) seen["strict-equality-branch"] = true;
        return 11;
    }
    function strictInequalityValue(a, b) {
        const answer = a !== b;
        if ($vm.dfgTrue()) seen["strict-inequality-value"] = true;
        return answer;
    }
    function strictInequalityBranch(a, b) {
        if (a !== b) {
            if ($vm.dfgTrue()) seen["strict-inequality-branch"] = true;
            return 11;
        }
        if ($vm.dfgTrue()) seen["strict-inequality-branch"] = true;
        return 7;
    }
    const equalityObject = {}, otherEqualityObject = {};
    const equalityText = "ARM11 generic equality";
    const equalityString1 = equalityText.split("").join("");
    const equalityString2 = ("!" + equalityText).slice(1);
    const equalityDouble17 = asDoubleNumber(17);
    check(isInt32(17) && !isInt32(equalityDouble17), "strict equality number encodings");
    const equalityBigInt1 = BigInt("9007199254740993"), equalityBigInt2 = BigInt("9007199254740993");
    const equalityCases = [[17, 17, true], [17, 18, false], [true, true, true], [true, false, false],
        [false, null, false], [true, 1, false], [undefined, null, false], [null, null, true],
        [undefined, undefined, true], [equalityObject, equalityObject, true],
        [equalityObject, otherEqualityObject, false], [equalityString1, equalityString2, true],
        [equalityString1, equalityString2 + "!", false], [equalityObject, equalityString1, false],
        [equalityString1, equalityObject, false], [NaN, NaN, false], [0, -0, true],
        [17, equalityDouble17, true], [equalityDouble17, 17, true], [17, 17.5, false],
        [2147483648, 2147483648, true],
        [equalityBigInt1, equalityBigInt2, true], [equalityBigInt1, equalityBigInt2 + 1n, false],
        [equalityBigInt1, 9007199254740992, false]];
    for (const [name, kernel, branch, invert] of [["strict-equality-value", strictEqualityValue, false, false],
        ["strict-equality-branch", strictEqualityBranch, true, false],
        ["strict-inequality-value", strictInequalityValue, false, true],
        ["strict-inequality-branch", strictInequalityBranch, true, true]]) {
        // Both argument profiles include numbers, immediates and cells before
        // compilation, so these are generic comparisons rather than Int32/Cell uses.
        warmed(name, kernel, i => equalityCases[i % equalityCases.length].slice(0, 2));
        let equalityDigest = 0;
        for (const [a, b, equal] of equalityCases) {
            seen[name] = false;
            const answer = kernel(a, b);
            check(answer === (branch ? (equal ? 7 : 11) : invert ? !equal : equal), name + " mixed-value result");
            if (mode === "dfg") check(seen[name], name + " result did not execute in DFG");
            equalityDigest += equal ? 7 : 11;
        }
        finish(name, equalityDigest);
    }

    function fastpathLiveKernel(a, b, value, integer, fraction, object) {
        const liveInt = integer * 3 | 0, liveDouble = fraction + 0.125, liveCell = object.payload;
        const equal = a === b;
        const truncated = Math.trunc(value);
        const answer = [equal, truncated, liveInt + object.delta, liveDouble + liveCell[0], liveCell[1]];
        if ($vm.dfgTrue()) seen["fastpath-live-values"] = true;
        return answer;
    }
    const fastpathLiveObject = {delta: 7, payload: new Float64Array([4.5, 9.5])};
    warmed("fastpath-live-values", fastpathLiveKernel, i => [
        ...equalityCases[i % equalityCases.length].slice(0, 2),
        truncWarmup[i % truncWarmup.length], i & 31, (i & 7) + 0.5, fastpathLiveObject]);
    // Equal strings/BigInts use the equality helper; large/negative-zero trunc
    // results use its cold call. Keep live numeric and cell values across both.
    for (const [a, b, input, equal, truncated] of [
        [equalityString1, equalityString2, 4294967296.5, true, 4294967296],
        [equalityBigInt1, equalityBigInt2 + 1n, -0.25, false, -0]]) {
        seen["fastpath-live-values"] = false;
        const answer = fastpathLiveKernel(a, b, input, 13, 2.5, fastpathLiveObject);
        check(answer[0] === equal && Object.is(answer[1], truncated) && answer[2] === 46
            && answer[3] === 7.125 && answer[4] === 9.5, "fastpath calls lost live int/double/cell values");
        if (mode === "dfg") check(seen["fastpath-live-values"], "fastpath live values did not execute in DFG");
    }
    finish("fastpath-live-values", 62.625);

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

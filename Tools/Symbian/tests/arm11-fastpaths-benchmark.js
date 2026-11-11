// ARM11 generic strict equality and Math.trunc performance workload.
// Run with --useDollarVM=true --useFTLJIT=false and -- baseline or dfg.
(function () {
    'use strict';
    const args = globalThis.arguments || [];
    const mode = args[0] === '--mode' ? args[1] : String(args[0]).replace(/^--mode=/, '');
    const started = performance.now(), deadlineMS = 50000;
    const iterations = {strictValue: 4194304, strictBranch: 4194304, trunc: 8388608};
    const sampleCount = 3, warmupIterations = 512;
    const observed = {}, bodies = {};
    function check(value, message) { if (!value) throw new Error(message); }
    function deadline() { check(performance.now() - started < deadlineMS, '50-second deadline'); }

    // Heterogeneous arrays keep both operands Untyped. Each function owns its
    // comparison site, and noInline prevents a caller from specializing it.
    function strictValueKernel(left, right, count) {
        let sum = 0;
        for (let i = 0; i < count; ++i) {
            const index = i & 15;
            const equal = left[index] === right[index];
            sum = (sum + equal) | 0;
        }
        observed.strictValue = mode === 'dfg' ? $vm.dfgTrue() : $vm.baselineJITTrue();
        bodies.strictValue = $vm.dfgTrue();
        return sum;
    }
    function strictBranchKernel(left, right, count) {
        let sum = 0;
        for (let i = 0; i < count; ++i) {
            const index = i & 15;
            if (left[index] === right[index])
                sum = (sum + 7) | 0;
            else
                sum = (sum + 11) | 0;
        }
        observed.strictBranch = mode === 'dfg' ? $vm.dfgTrue() : $vm.baselineJITTrue();
        bodies.strictBranch = $vm.dfgTrue();
        return sum;
    }
    function truncKernel(values, count) {
        let sum = 0.5;
        for (let i = 0; i < count; ++i)
            sum += Math.trunc(values[i & 7]);
        observed.trunc = mode === 'dfg' ? $vm.dfgTrue() : $vm.baselineJITTrue();
        bodies.trunc = $vm.dfgTrue();
        return sum;
    }
    function truncDirect(value) {
        const result = Math.trunc(value);
        observed.truncDirect = mode === 'dfg' ? $vm.dfgTrue() : $vm.baselineJITTrue();
        bodies.truncDirect = $vm.dfgTrue();
        return result;
    }
    function encoded(value) {
        if (Number.isNaN(value)) return 'NaN';
        if (value === Infinity) return 'Infinity';
        if (value === -Infinity) return '-Infinity';
        return Object.is(value, -0) ? '-0' : value;
    }

    try {
        check(['baseline', 'dfg'].includes(mode), 'expected --mode baseline|dfg');
        check(typeof $vm === 'object' && typeof numberOfDFGCompiles === 'function'
            && typeof sleepSeconds === 'function', '--useDollarVM=true required');
        const options = jscOptions();
        check(options.useJIT && options.useBaselineJIT && !options.useFTLJIT
            && !!options.useDFGJIT === (mode === 'dfg'), 'requested tier options');
        const object = {}, otherObject = {};
        const text = 'ARM11 generic equality';
        const string1 = text.split('').join(''), string2 = ('!' + text).slice(1);
        const big1 = BigInt('9007199254740993'), big2 = BigInt('9007199254740993');
        const double17 = asDoubleNumber(17);
        check(isInt32(17) && !isInt32(double17), 'strict equality number encodings');
        const left = [17, 17, true, true, null, undefined, false, true,
            object, object, object, false, string1, NaN, 17, big1];
        const right = [17, 18, true, false, null, null, null, 1,
            object, otherObject, true, object, string2, NaN, double17, big2];
        const equal = [true, false, true, false, true, false, false, false,
            true, false, false, false, true, false, true, true];
        // Seed negative-zero and large results before compilation. This keeps
        // ArithTrunc's double result instead of choosing its int32-only mode.
        const truncWarmup = [0.75, 1.5, 7.875, 31.25, 255.5, 4095.75, -0.25, 4294967296.5];
        const truncValues = [0.75, 1.5, 7.875, 31.25, 255.5, 4095.75, 65535.25, 1048575.875];
        const truncResults = [0, 1, 7, 31, 255, 4095, 65535, 1048575];
        const warmupTruncSum = 0 + 1 + 7 + 31 + 255 + 4095 + 0 + 4294967296;
        const trueCount = equal.filter(value => value).length;
        const branchSum = trueCount * 7 + (16 - trueCount) * 11;
        const truncSum = truncResults.reduce((sum, value) => sum + value, 0);
        const cases = [
            {name: 'strictValue', kernel: strictValueKernel, inputs: [left, right],
                warmupInputs: [left, right], perCycle: trueCount, warmupPerCycle: trueCount, cycle: 16, seed: 0},
            {name: 'strictBranch', kernel: strictBranchKernel, inputs: [left, right],
                warmupInputs: [left, right], perCycle: branchSum, warmupPerCycle: branchSum, cycle: 16, seed: 0},
            {name: 'trunc', kernel: truncKernel, inputs: [truncValues], warmupInputs: [truncWarmup],
                perCycle: truncSum, warmupPerCycle: warmupTruncSum, cycle: 8, seed: 0.5}
        ];
        const kernels = {strictValue: strictValueKernel, strictBranch: strictBranchKernel,
            trunc: truncKernel, truncDirect};
        for (const name of Object.keys(kernels)) noInline(kernels[name]);
        const tiers = {}, compiled = {}, dfgBodies = {}, warmupBatches = {};
        const timingsMS = {}, mediansMS = {}, checksums = {};
        for (const item of cases) {
            let batches = 0;
            while (batches++ < 512) {
                deadline();
                const result = item.kernel(...item.warmupInputs, warmupIterations);
                check(result === item.seed + item.warmupPerCycle * (warmupIterations / item.cycle),
                    'warmup checksum ' + item.name);
                if (observed[item.name]) break;
                sleepSeconds(0.005);
            }
            check(observed[item.name], 'requested warmup tier ' + item.name);
            warmupBatches[item.name] = batches;
            const expected = item.seed + item.perCycle * (iterations[item.name] / item.cycle), samples = [];
            for (let sample = 0; sample < sampleCount; ++sample) {
                deadline();
                observed[item.name] = bodies[item.name] = false;
                const tick = performance.now();
                const result = item.kernel(...item.inputs, iterations[item.name]);
                samples.push(performance.now() - tick);
                check(result === expected, 'measured checksum ' + item.name);
                check(observed[item.name] && bodies[item.name] === (mode === 'dfg'),
                    'measured requested body ' + item.name);
                checksums[item.name] = result;
            }
            timingsMS[item.name] = samples;
            mediansMS[item.name] = samples.slice().sort((a, b) => a - b)[sampleCount >> 1];
            tiers[item.name] = observed[item.name];
            dfgBodies[item.name] = bodies[item.name];
        }
        // Direct return checks run outside timing and preserve the actual -0,
        // large-number and non-finite results that a sum would hide.
        let directBatches = 0;
        while (directBatches++ < 512) {
            deadline();
            for (let i = 0; i < 128; ++i) truncDirect(truncWarmup[i & 7]);
            if (observed.truncDirect) break;
            sleepSeconds(0.005);
        }
        check(observed.truncDirect, 'requested direct trunc tier');
        warmupBatches.truncDirect = directBatches;
        const fallbackInputs = [0.75, 1.5, -1.5, -0.25, -0, -Number.MIN_VALUE,
            2147483647.75, -2147483648.75, 4294967296.5, -4294967296.5, NaN, Infinity, -Infinity];
        const fallbackExpected = [0, 1, -1, -0, -0, -0,
            2147483647, -2147483648, 4294967296, -4294967296, NaN, Infinity, -Infinity];
        const truncSemantic = [];
        for (let i = 0; i < fallbackInputs.length; ++i) {
            observed.truncDirect = bodies.truncDirect = false;
            const value = truncDirect(fallbackInputs[i]);
            check(Object.is(value, fallbackExpected[i]), 'direct trunc fallback ' + i);
            check(observed.truncDirect && bodies.truncDirect === (mode === 'dfg'), 'direct trunc body ' + i);
            truncSemantic.push(encoded(value));
        }
        tiers.truncDirect = observed.truncDirect;
        dfgBodies.truncDirect = bodies.truncDirect;
        for (const name of Object.keys(kernels)) {
            const count = numberOfDFGCompiles(kernels[name]);
            check(Number.isInteger(count) && count >= 0 && (mode === 'dfg'
                ? count > 0 && count !== 1000000 : count === 0 || count === 1000000),
                'genuine compilation evidence ' + name);
            compiled[name] = count;
        }
        deadline();
        print('BENCH_RESULT ' + JSON.stringify({test: 'fastpaths', kind: 'fastpaths', mode,
            digest: {semantic: {strictEquality: equal, trunc: truncSemantic}, checksums},
            timingsMS, mediansMS, tiers, compiled, dfgBodies,
            options: {useJIT: options.useJIT, useBaselineJIT: options.useBaselineJIT,
                useDFGJIT: options.useDFGJIT, useConcurrentJIT: options.useConcurrentJIT,
                useFTLJIT: options.useFTLJIT}, iterations, sampleCount, warmupBatches,
            deadlineMS, elapsedMS: performance.now() - started}));
        print('PASS: arm11-fastpaths-benchmark');
    } catch (error) {
        print('FAIL: arm11-fastpaths-benchmark ' + error.message);
        throw error;
    }
}());

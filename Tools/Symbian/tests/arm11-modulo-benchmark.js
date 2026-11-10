// ARM11 integer remainder performance workload.
// Run with --useDollarVM=true and -- dfg, baseline or llint.
(function () {
    'use strict';
    const args = globalThis.arguments || [];
    const mode = args[0] === '--mode' ? args[1] : String(args[0]).replace(/^--mode=/, '');
    const started = performance.now(), deadlineMS = 50000;
    const iterations = 4194304, sampleCount = 3, warmupIterations = 512;
    const observed = {}, bodies = {};
    function check(value, message) { if (!value) throw new Error(message); }
    function deadline() { check(performance.now() - started < deadlineMS, '50-second deadline'); }

    // Literal power-of-two divisors exercise the integer DFG lowering.
    // Truncating uses permit signed multiples without negative-zero OSR exits.
    function mod2Kernel(count) {
        let sum = 0x12345;
        for (let i = 0; i < count; ++i) {
            const value = (i << 17) | (i & 255);
            sum = ((sum ^ ((value % 2) | 0)) + i) | 0;
        }
        observed.mod2 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.mod2 = $vm.dfgTrue();
        return sum;
    }
    function mod256Kernel(count) {
        let sum = 0x12345;
        for (let i = 0; i < count; ++i) {
            const value = (i << 17) | (i & 255);
            sum = ((sum ^ ((value % 256) | 0)) + i) | 0;
        }
        observed.mod256 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.mod256 = $vm.dfgTrue();
        return sum;
    }
    function mod1073741824Kernel(count) {
        let sum = 0x12345;
        for (let i = 0; i < count; ++i) {
            const value = (i << 17) | (i & 255);
            sum = ((sum ^ ((value % 1073741824) | 0)) + i) | 0;
        }
        observed.mod1073741824 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.mod1073741824 = $vm.dfgTrue();
        return sum;
    }
    // Separate non-truncating functions check negative-zero and type fallback.
    function mod2Direct(value) {
        const result = value % 2;
        observed.direct2 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.direct2 = $vm.dfgTrue();
        return result;
    }
    function mod256Direct(value) {
        const result = value % 256;
        observed.direct256 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.direct256 = $vm.dfgTrue();
        return result;
    }
    function mod1073741824Direct(value) {
        const result = value % 1073741824;
        observed.direct1073741824 = mode === 'dfg' ? $vm.dfgTrue() : mode === 'baseline' ? $vm.baselineJITTrue() : $vm.llintTrue();
        bodies.direct1073741824 = $vm.dfgTrue();
        return result;
    }
    function checksum(divisor, count) {
        let sum = 0x12345;
        const mask = divisor - 1;
        for (let i = 0; i < count; ++i) {
            const value = (i << 17) | (i & 255);
            const remainder = value < 0 ? -((-value) & mask) : value & mask;
            sum = ((sum ^ remainder) + i) | 0;
        }
        return sum;
    }
    function encoded(value) {
        if (Number.isNaN(value)) return 'NaN';
        return Object.is(value, -0) ? '-0' : value;
    }
    try {
        check(['dfg', 'baseline', 'llint'].includes(mode), 'expected --mode dfg|baseline|llint');
        check(typeof $vm === 'object' && typeof numberOfDFGCompiles === 'function'
            && typeof sleepSeconds === 'function', '--useDollarVM=true required');
        const options = jscOptions();
        check(!options.useFTLJIT && !!options.useJIT === (mode !== 'llint')
            && !!options.useDFGJIT === (mode === 'dfg')
            && (mode === 'llint' || options.useBaselineJIT), 'requested tier options');
        const cases = [
            {name: 'mod2', divisor: 2, kernel: mod2Kernel, directName: 'direct2', direct: mod2Direct},
            {name: 'mod256', divisor: 256, kernel: mod256Kernel, directName: 'direct256', direct: mod256Direct},
            {name: 'mod1073741824', divisor: 1073741824, kernel: mod1073741824Kernel,
                directName: 'direct1073741824', direct: mod1073741824Direct}
        ];
        const kernels = {}, tiers = {}, compiled = {}, dfgBodies = {};
        const timingsMS = {}, mediansMS = {}, checksums = {}, semantic = {}, warmupBatches = {};
        for (const item of cases) {
            kernels[item.name] = item.kernel;
            kernels[item.directName] = item.direct;
            noInline(item.kernel);
            noInline(item.direct);
        }
        // Every warmup burst is outside timing and yields to concurrent compilation.
        for (const item of cases) {
            const expected = checksum(item.divisor, warmupIterations);
            let batches = 0;
            while (batches++ < 512) {
                deadline();
                check(item.kernel(warmupIterations) === expected, 'warmup checksum ' + item.name);
                if (observed[item.name]) break;
                sleepSeconds(0.01);
            }
            check(observed[item.name], 'requested warmup tier ' + item.name);
            tiers[item.name] = observed[item.name];
            dfgBodies[item.name] = bodies[item.name];
            warmupBatches[item.name] = batches;
            const expectedMeasured = checksum(item.divisor, iterations), samples = [];
            for (let sample = 0; sample < sampleCount; ++sample) {
                deadline();
                const tick = performance.now();
                const result = item.kernel(iterations);
                samples.push(performance.now() - tick);
                check(result === expectedMeasured, 'measured checksum ' + item.name);
                check(observed[item.name] && (mode !== 'dfg' || bodies[item.name]),
                    'measured requested tier ' + item.name);
                checksums[item.name] = result;
            }
            timingsMS[item.name] = samples;
            mediansMS[item.name] = samples.slice().sort((a, b) => a - b)[sampleCount >> 1];
        }
        // Warm the direct result before checking cases that can trigger OSR exits.
        for (const item of cases) {
            let batches = 0;
            while (batches++ < 512) {
                deadline();
                for (let i = 0; i < 128; ++i) {
                    const value = (i << 1) + 1;
                    const expected = item.divisor === 2 ? 1 : value;
                    check(item.direct(value) === expected, 'direct warmup ' + item.name);
                }
                if (observed[item.directName]) break;
                sleepSeconds(0.01);
            }
            check(observed[item.directName], 'requested direct tier ' + item.name);
            tiers[item.directName] = observed[item.directName];
            dfgBodies[item.directName] = bodies[item.directName];
            warmupBatches[item.directName] = batches;
            const d = item.divisor;
            const inputs = [-d - 1, -d, -1, -0, 0, 1, d - 1, d, d + 1,
                -2147483648, 2147483647, 2.5, -0.5, NaN, Infinity];
            const expected = [-1, -0, -1, -0, 0, 1, d - 1, 0, 1,
                -0, d - 1, d === 2 ? 0.5 : 2.5, -0.5, NaN, NaN];
            const actual = inputs.map(value => item.direct(value));
            for (let i = 0; i < actual.length; ++i)
                check(Object.is(actual[i], expected[i]), 'signed/negative-zero/double fallback ' + item.name + '/' + i);
            semantic[item.name] = actual.map(encoded);
        }
        for (const name of Object.keys(kernels)) {
            const count = numberOfDFGCompiles(kernels[name]);
            check(Number.isInteger(count) && count >= 0 && (mode === 'dfg'
                ? count > 0 && count !== 1000000 : count === 0 || count === 1000000),
                'genuine compilation evidence ' + name);
            compiled[name] = count;
        }
        deadline();
        print('BENCH_RESULT ' + JSON.stringify({test: 'modulo', kind: 'modulo', mode,
            digest: {semantic, checksums}, timingsMS, mediansMS, tiers, compiled, dfgBodies,
            options: {useJIT: options.useJIT, useBaselineJIT: options.useBaselineJIT,
                useDFGJIT: options.useDFGJIT, useConcurrentJIT: options.useConcurrentJIT,
                useFTLJIT: options.useFTLJIT}, iterations, sampleCount, warmupBatches,
            deadlineMS, elapsedMS: performance.now() - started}));
        print('PASS: arm11-modulo-benchmark');
    } catch (error) {
        print('FAIL: arm11-modulo-benchmark ' + error.message);
        throw error;
    }
}());

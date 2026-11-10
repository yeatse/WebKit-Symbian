// ARM11 RegExp performance workload with default concurrent tiering.
// Run with --useDollarVM=true --useRegExpJIT=false/true and -- dfg.
(function () {
    'use strict';
    const started = performance.now(), deadlineMS = 50000;
    const iterations = {test: 524288, exec: 131072, replace: 65536}, sampleCount = 5;
    const seen = {test: false, exec: false, replace: false};
    function check(value, message) { if (!value) throw new Error(message); }
    function deadline() { check(performance.now() - started < deadlineMS, '50-second deadline'); }
    function testKernel(regex, inputs, count) {
        let total = 0;
        for (let i = 0; i < count; ++i) total += regex.test(inputs[i & 7]);
        seen.test ||= $vm.dfgTrue();
        return total;
    }
    function execKernel(regex, inputs, count) {
        let total = 0;
        for (let i = 0; i < count; ++i) {
            const value = regex.exec(inputs[i & 7]);
            if (value) total += value.index + value[0].length + value[1].length + value[2].length;
        }
        seen.exec ||= $vm.dfgTrue();
        return total;
    }
    function replaceKernel(regex, inputs, count) {
        let total = 0;
        for (let i = 0; i < count; ++i) {
            const value = inputs[i & 7].replace(regex, '#');
            total += value.length + value.charCodeAt(0);
        }
        seen.replace ||= $vm.dfgTrue();
        return total;
    }
    const kernels = {test: testKernel, exec: execKernel, replace: replaceKernel};
    for (const name of Object.keys(kernels)) noInline(kernels[name]);
    function memory() {
        const value = memoryUsageStatistics();
        return {heapSize: value.heapSize, heapCapacity: value.heapCapacity,
            extraMemorySize: value.extraMemorySize, objectCount: value.objectCount};
    }
    function semantics() {
        const output = [];
        const captured = /([A-Za-z]+)=([0-9]+)/.exec('x key=123 z');
        output.push(Array.from(captured), captured.index);
        output.push('red1 green2 blue3'.replace(/(?:red|green|blue)[0-9]/g, '#'));
        output.push('first=17 second=29'.match(/[0-9]+/g));
        output.push('name=12'.search(/=[0-9]+/));
        output.push('alpha, beta;gamma'.split(/[,;]\s*/));
        // ARM restrictions must keep these valid on the interpreter fallback.
        output.push(/^(ab)\1$/.test('abab'), /^(ab)\1$/.test('abac'));
        output.push(/\u{1F642}/u.test('a\uD83D\uDE42b'));
        output.push(Array.from('\uD83D\uDE42'.matchAll(/./gu), match => match[0]));
        output.push(/(?<=prefix:)value/.exec('prefix:value')[0]);
        const global = /[0-9]+/g;
        output.push(global.exec('x17y29')[0], global.lastIndex,
            global.exec('x17y29')[0], global.lastIndex, global.exec('x17y29'), global.lastIndex);
        const expected = [["key=123", "key", "123"], 2, "# # #", ["17", "29"], 4,
            ["alpha", "beta", "gamma"], true, false, true, ["\uD83D\uDE42"], "value",
            "17", 3, "29", 6, null, 0];
        check(JSON.stringify(output) === JSON.stringify(expected), 'semantic corpus');
        return output;
    }
    try {
        check(typeof $vm === 'object' && typeof numberOfDFGCompiles === 'function'
            && typeof sleepSeconds === 'function', '--useDollarVM=true required');
        const options = jscOptions();
        check(options.useJIT && options.useBaselineJIT && options.useDFGJIT && !options.useFTLJIT,
            'DFG-capable ARM11 image required');
        check(options.useConcurrentJIT, 'keep concurrent JIT and default tier policy');
        fullGC();
        const before = memory();
        const specifications = [
            {kernel: 'test', pattern: 'WK[0-9]{3}:[A-Z]{2}$', flags: '', expected: [4, 4],
                inputs: ['WK123:AB', 'WK001:ZX', 'WK999:ZZ', 'wk123:AB',
                    'WK12:AB', 'WK1234:AB', 'WK321:A_', 'WK314:PI']},
            {kernel: 'exec', pattern: '([A-Za-z]+)=([0-9]{2,5})$', flags: '', expected: [73, 73],
                inputs: ['alpha=123', 'beta=42', 'bad=1', 'X=99999',
                    'long=123456', 'skip xyz', 'a=00', 'name=07']},
            {kernel: 'replace', pattern: '(?:red|green|blue)[0-9]', flags: 'g', expected: [306, 2090],
                inputs: ['red1 blue2', 'green3', 'redx', 'red1green2blue3',
                    'none', 'blue9 red0', 'GREEN7', 'green4blue5']}
        ];
        const cases = [], coldMS = {}, timingsMS = {}, checksums = {};
        // The same ASCII pattern sees both Latin1 and forced UTF-16 subjects.
        // Each pair gets its own first-call observation; RegExp code is cached.
        for (const specification of specifications) {
            for (const wide of [false, true]) {
                const item = {name: specification.kernel + (wide ? '-16bit' : '-8bit'),
                    kernel: specification.kernel, iterations: iterations[specification.kernel], regex: new RegExp(specification.pattern, specification.flags),
                    inputs: specification.inputs.map(value => (wide ? '\u0100 ' : '! ') + value)};
                const tick = performance.now();
                const first = kernels[item.kernel](item.regex, item.inputs, 8);
                coldMS[item.name] = performance.now() - tick;
                item.expected = specification.expected[wide ? 1 : 0];
                check(first === item.expected, 'first-call checksum ' + item.name);
                cases.push(item);
            }
        }
        const semantic = semantics();
        let warmupBatches = 0;
        const perCaseDFG = {};
        for (const item of cases) {
            // Finite bursts permit lower-priority compiler threads to run on EKA2.
            // Each subject-width case must observe its own optimized body.
            seen[item.kernel] = false;
            let batches = 0;
            while (batches++ < 512) {
                deadline();
                check(kernels[item.kernel](item.regex, item.inputs, 256) === item.expected * 32,
                    'warmup checksum ' + item.name);
                if (seen[item.kernel]) break;
                sleepSeconds(0.005);
            }
            warmupBatches += batches;
            check(seen[item.kernel], 'DFG warmup body ' + item.name);
            const samples = [];
            for (let sample = 0; sample < sampleCount; ++sample) {
                deadline();
                seen[item.kernel] = false;
                const tick = performance.now();
                const result = kernels[item.kernel](item.regex, item.inputs, item.iterations);
                samples.push(performance.now() - tick);
                check(result === item.expected * (item.iterations / 8), 'measured checksum ' + item.name);
                check(seen[item.kernel], 'measured DFG body ' + item.name);
                checksums[item.name] = result;
            }
            timingsMS[item.name] = samples;
            perCaseDFG[item.name] = true;
        }
        const compiled = {};
        for (const name of Object.keys(kernels)) {
            compiled[name] = numberOfDFGCompiles(kernels[name]);
            check(Number.isInteger(compiled[name]) && compiled[name] > 0 && compiled[name] !== 1000000,
                'genuine DFG compilation count ' + name);
        }
        fullGC();
        const after = memory();
        deadline();
        print('BENCH_RESULT ' + JSON.stringify({test: 'regex', mode: 'dfg',
            digest: {semantic, checksums}, timingsMS, coldMS, tiers: seen, compiled, perCaseDFG,
            options: {useRegExpJIT: options.useRegExpJIT, useJIT: options.useJIT,
                useBaselineJIT: options.useBaselineJIT, useDFGJIT: options.useDFGJIT,
                useConcurrentJIT: options.useConcurrentJIT, useFTLJIT: options.useFTLJIT},
            iterations, sampleCount, warmupBatches, memory: {before, after}, deadlineMS,
            elapsedMS: performance.now() - started}));
        print('PASS: arm11-regexp-benchmark');
    } catch (error) {
        print('FAIL: arm11-regexp-benchmark ' + error.message);
        throw error;
    }
}());

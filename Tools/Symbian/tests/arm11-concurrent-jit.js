// Concurrent DFG smoke; run with --useDollarVM=true and the default
// useConcurrentJIT=true. Do not pass -s, --useConcurrentJIT=false, or override
// compilation thresholds. Use an outer host timeout <= 60 seconds as well:
// native calls (including GC) cannot be interrupted by this JS deadline.
(function () {
    "use strict";
    function check(condition, message) {
        if (!condition)
            throw new Error(message);
    }

    try {
        check(typeof $vm === "object", "--useDollarVM=true is required");
        check(typeof jscOptions === "function" && typeof sleepSeconds === "function",
            "JSC option inspection and sleepSeconds are required");
        check(typeof performance === "object" && typeof performance.now === "function",
            "a monotonic deadline clock is required");
        const started = performance.now();
        const budgetMS = 50000;
        const options = jscOptions();
        // jscOptions() exposes Boolean options as numeric 0/1 in this shell.
        const useConcurrentJIT = options.useConcurrentJIT === 1 || options.useConcurrentJIT === true;
        check(useConcurrentJIT, "concurrent JIT must remain enabled");
        check(options.useJIT && options.useBaselineJIT && options.useDFGJIT,
            "LLInt/Baseline/DFG capable image is required");
        check(!options.useFTLJIT, "this ARM11 smoke does not permit FTL");
        check(Number.isInteger(options.numberOfDFGCompilerThreads)
            && options.numberOfDFGCompilerThreads > 0, "no background DFG compiler threads");

        function deadline() {
            const elapsed = performance.now() - started;
            check(Number.isFinite(elapsed) && elapsed >= 0 && elapsed < budgetMS,
                "concurrent DFG exceeded its 50-second monotonic deadline");
            return elapsed;
        }

        function numericKernel(seed) {
            const integer = (seed * 3 + 7) | 0;
            const fraction = seed + 0.125;
            return [integer + fraction, $vm.dfgTrue()];
        }

        function allocationKernel(seed, collect) {
            const live = {value: seed + 1, payload: new Float64Array(2)};
            live.payload[0] = seed + 0.5;
            live.payload[1] = seed + 1.5;
            const before = $vm.dfgTrue();
            if (collect)
                gc();
            const checksum = live.value + live.payload[0] + live.payload[1];
            return {checksum, live, before, after: $vm.dfgTrue()};
        }
        // Tier intrinsics must describe these bodies, not an optimized caller.
        noInline(numericKernel);
        noInline(allocationKernel);

        const roots = new Array(32);
        let numericDFGExecutions = 0, allocationDFGExecutions = 0;
        let collections = 0, dfgCollections = 0, dfgReturnsAfterGC = 0;
        let collectionsBeforeAllBodiesObserved = 0;
        let calls = 0, batches = 0, yields = 0;
        let compiled = {numeric: 0, allocation: 0};
        let complete = false;

        function compilationCount(fn) {
            const count = numberOfDFGCompiles(fn);
            check(Number.isInteger(count) && count >= 0 && count !== 1000000,
                "invalid/sentinel DFG compilation count: " + count);
            return count;
        }

        function checkLive(result, seed) {
            check(result.checksum === 3 * seed + 3, "GC lost live int/double values");
            check(result.live.value === seed + 1 && result.live.payload.length === 2
                && result.live.payload[0] === seed + 0.5 && result.live.payload[1] === seed + 1.5,
                "GC lost the returned object or typed-array payload");
        }

        // Finite bursts give worker threads time to finish and the mutator time
        // to install ready plans. Neither a fixed invocation count nor a queued
        // compilation is accepted as proof that optimized code actually ran.
        while (batches < 8192) {
            deadline();
            for (let i = 0; i < 64; ++i) {
                deadline();
                const seed = calls & 31;
                const numeric = numericKernel(seed);
                check(numeric[0] === 4 * seed + 7.125, "numeric kernel result");
                if (numeric[1])
                    ++numericDFGExecutions;

                const collect = i === 0;
                if (collect && !(numericDFGExecutions && allocationDFGExecutions))
                    ++collectionsBeforeAllBodiesObserved;
                const result = allocationKernel(seed, collect);
                checkLive(result, seed);
                roots[seed] = result;
                if (result.before)
                    ++allocationDFGExecutions;
                if (collect) {
                    ++collections;
                    if (result.before)
                        ++dfgCollections;
                    if (result.after)
                        ++dfgReturnsAfterGC;
                }
                ++calls;
            }
            ++batches;
            deadline();
            compiled = {numeric: compilationCount(numericKernel), allocation: compilationCount(allocationKernel)};
            if (compiled.numeric > 0 && compiled.allocation > 0 && numericDFGExecutions > 0
                && allocationDFGExecutions > 0 && dfgCollections >= 2) {
                complete = true;
                break;
            }
            sleepSeconds(0.01);
            ++yields;
            deadline();
        }
        check(complete, "bounded warmup finished without genuine DFG execution and GC");
        check(collectionsBeforeAllBodiesObserved > 0, "no GC during concurrent compilation warmup");

        // Retain roots from the tested bodies across one further full GC.
        deadline();
        gc();
        ++collections;
        deadline();
        for (let seed = 0; seed < roots.length; ++seed)
            checkLive(roots[seed], seed);
        compiled = {numeric: compilationCount(numericKernel), allocation: compilationCount(allocationKernel)};
        check(compiled.numeric > 0 && compiled.allocation > 0, "DFG compilation evidence disappeared");
        const elapsedMS = deadline();
        print("RESULT " + JSON.stringify({test: "arm11-concurrent-jit", useConcurrentJIT,
            compilerThreads: options.numberOfDFGCompilerThreads, budgetMS, elapsedMS, batches, calls, yields,
            compiled, numericDFGExecutions, allocationDFGExecutions, collections, dfgCollections,
            dfgReturnsAfterGC, collectionsBeforeAllBodiesObserved, retainedRoots: roots.length}));
        print("PASS: arm11-concurrent-jit");
    } catch (error) {
        print("FAIL: arm11-concurrent-jit " + error.message);
        throw error;
    }
})();

(function () {
    function equal(actual, expected, label) {
        if (!Object.is(actual, expected))
            throw new Error(label + ": expected " + expected + ", got " + actual);
    }
    const object = { a: { b: null } };
    equal(object?.a?.b?.c, undefined, "optional chaining");
    equal(object.x?.y, undefined, "missing property");
    equal(null ?? "default", "default", "nullish value");
    equal(0 ?? 7, 0, "non-nullish zero");
    equal((2n ** 64n).toString(), "18446744073709551616", "BigInt exponent");
    equal(BigInt.asUintN(8, 257n), 1n, "BigInt truncation");
    equal(typeof globalThis, "object", "globalThis");
    equal([..."a1b2c3".matchAll(/[a-z](\d)/g)].map(m => m[1]).join(","), "1,2,3", "matchAll");
    equal(/(?<=prefix:)\w+/u.exec("prefix:value")[0], "value", "regexp lookbehind");
    equal([1, 2, 3].toReversed().join(","), "3,2,1", "toReversed");
    equal([3, 1, 2].toSorted().join(","), "1,2,3", "toSorted");
    equal([1, 2, 3].toSpliced(1, 1, 4).join(","), "1,4,3", "toSpliced");
    equal([1, 2, 3].with(-1, 9).join(","), "1,2,9", "array with");
    equal("a-b-a".replaceAll("a", "z"), "z-b-z", "replaceAll");
    equal(Object.fromEntries([["answer", 42]]).answer, 42, "fromEntries");
    equal(JSON.stringify(JSON.parse('{"k":[1,2,{"z":true}]}')), '{"k":[1,2,{"z":true}]}', "JSON");
    let sum = 0.5;
    for (let i = 0; i < 20000; ++i)
        sum += i * 0.25 - (i & 3);
    equal(sum, 49967500.5, "floating point loop");
    let bigSum = 0n;
    for (let i = 0n; i < 300n; ++i)
        bigSum += i * i;
    equal(bigSum, 8955050n, "BigInt loop");
    Promise.all([
        Promise.allSettled([Promise.resolve(1), Promise.reject(2)]).then(results => {
            equal(results[0].status, "fulfilled", "allSettled fulfillment");
            equal(results[0].value, 1, "allSettled value");
            equal(results[1].status, "rejected", "allSettled rejection");
            equal(results[1].reason, 2, "allSettled reason");
        }),
        Promise.any([Promise.reject("no"), Promise.resolve(7)]).then(value => equal(value, 7, "Promise.any")),
        Promise.any([Promise.reject("first"), Promise.reject("second")]).then(
            () => { throw new Error("Promise.any unexpectedly fulfilled"); },
            error => {
                equal(error instanceof AggregateError, true, "AggregateError");
                equal(error.errors.join(","), "first,second", "AggregateError errors");
            })
    ]).then(() => print("PASS: es2020"), error => {
        print("FAIL: es2020: " + error);
        throw error;
    });
})();

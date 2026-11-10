(function () {
    'use strict';
    function check(condition, message) {
        if (!condition) throw new Error(message);
    }
    let setDFG = false, mapDFG = false;
    function sumSet(values) {
        let sum = 0;
        for (const value of values) sum += value;
        setDFG ||= $vm.dfgTrue();
        return sum;
    }
    function sumMap(values) {
        let sum = 0;
        for (const [key, value] of values) sum += key + value;
        mapDFG ||= $vm.dfgTrue();
        return sum;
    }
    noInline(sumSet);
    noInline(sumMap);

    const populatedSet = new Set([1, 2, 3]), emptySet = new Set();
    const populatedMap = new Map([[1, 2], [3, 4]]), emptyMap = new Map();
    for (let i = 0; i < 12000; ++i) {
        check(sumSet(i & 1 ? populatedSet : emptySet) === (i & 1 ? 6 : 0), 'Set iteration');
        check(sumMap(i & 1 ? populatedMap : emptyMap) === (i & 1 ? 10 : 0), 'Map iteration');
    }
    check(setDFG && mapDFG, 'DFG iteration bodies');
    populatedSet.clear();
    populatedMap.clear();
    check(sumSet(populatedSet) === 0 && sumMap(populatedMap) === 0, 'cleared collection iteration');
    populatedSet.add(7);
    populatedMap.set(5, 6);
    check(sumSet(populatedSet) === 7 && sumMap(populatedMap) === 11, 'collection iteration after insertion');
    print('PASS: arm11-map-iteration');
}());

(function () {
    'use strict';
    function check(condition, message) {
        if (!condition) throw new Error(message);
    }
    function resize(array, length) { array.length = length; }
    function tail(first, ...values) { return values; }
    function sortNumbers(values) { return values.sort((a, b) => a - b); }
    function readKey(object, key) { return object[key]; }
    function equalStrings(left, right) { return left === right; }
    function matchNumbers(text) {
        let sum = 0;
        for (const match of text.matchAll(/[0-9]+/g)) sum += Number(match[0]);
        return sum;
    }

    const keyed = {true: 17, false: 29};
    for (let i = 0; i < 12000; ++i) {
        const first = i % 3 === 0 ? i : i % 3 === 1 ? i + 0.25 : {value: i};
        const array = [first, first, first];
        resize(array, 1);
        check(array.length === 1 && array[0] === first && array[1] === undefined, 'array shrink');
        resize(array, 3);
        check(array[1] === undefined && array[2] === undefined, 'array length growth');
        array.push(first);
        check(array.length === 4 && array[3] === first, 'array append');

        const copied = tail(0, i + 1.5, i - 0.5, i + 0.5);
        check(copied.length === 3 && copied[0] === i + 1.5 && copied[2] === i + 0.5, 'rest array copy');
        const sorted = sortNumbers(copied);
        check(sorted[0] === i - 0.5 && sorted[1] === i + 0.5 && sorted[2] === i + 1.5, 'array sort');
        const key = !!(i & 1);
        check(readKey(keyed, key) === (key ? 17 : 29), 'boolean property key');
        check(equalStrings(String(i), '' + i) && !equalStrings(String(i), String(i + 1)), 'string equality');
        check(matchNumbers(i + ':' + (i + 1)) === 2 * i + 1, 'regular expression iteration');
    }
    print('PASS: arm11-array-operations');
}());

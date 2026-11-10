// Exercises ARM11 RegExp execution with Latin1 and UTF-16 subjects.
(function () {
    'use strict';
    function check(value, message) {
        if (!value) throw new Error(message);
    }
    const options = jscOptions();
    if (options.useJIT)
        check(options.useRegExpJIT === 1 || options.useRegExpJIT === true,
            'RegExp JIT must be enabled in JIT mode');

    for (const prefix of ['! ', '\u0100 ']) {
        const fixed = /WK[0-9]{3}:[A-Z]{2}$/;
        const capture = /([A-Za-z]+)=([0-9]{2,5})$/;
        const colors = /(?:red|green|blue)[0-9]/g;
        for (let i = 0; i < 128; ++i) {
            check(fixed.test(prefix + 'WK123:AB'), 'fixed-length class match');
            check(!fixed.test(prefix + 'WK12:AB'), 'fixed-length class rejection');
            const match = capture.exec(prefix + 'alpha=123');
            check(match && match.index === prefix.length && match[0] === 'alpha=123'
                && match[1] === 'alpha' && match[2] === '123', 'capture values and index');
            check(capture.exec(prefix + 'alpha=1') === null, 'capture rejection');
            check((prefix + 'red1 green2 blue3').replace(colors, '#') === prefix + '# # #',
                'global alternation replacement');
            check(colors.lastIndex === 0, 'replacement resets lastIndex');
        }

        const global = /[0-9]+/g;
        const input = prefix + '17x29';
        check(global.exec(input)[0] === '17' && global.lastIndex === prefix.length + 2,
            'first global match');
        check(global.exec(input)[0] === '29' && global.lastIndex === input.length,
            'second global match');
        check(global.exec(input) === null && global.lastIndex === 0, 'global match completion');

        // These expressions use the ARM interpreter fallback.
        check(/(ab)\1$/.test(prefix + 'abab') && !/(ab)\1$/.test(prefix + 'abac'),
            'backreference fallback');
        check(/\u{1F642}/u.test(prefix + '\uD83D\uDE42')
            && !/\u{1F642}/u.test(prefix + '\uD83D\uDE43'), 'Unicode fallback');
    }
    print('PASS: arm11-regexp');
}());

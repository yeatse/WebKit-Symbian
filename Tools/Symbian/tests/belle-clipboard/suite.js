(function () {
    'use strict';
    var query = new URLSearchParams(location.search);
    var token = query.get('run') || '', mode = query.get('mode') || 'keyboard';
    var source = document.getElementById('source'), target = document.getElementById('target');
    var special = document.getElementById('special'), richSource = document.getElementById('richSource');
    var richTarget = document.getElementById('richTarget'), button = document.getElementById('start');
    var instruction = document.getElementById('instruction'), stage = document.getElementById('stage');
    var output = document.getElementById('log'), rows = [], active = null, busy = false;
    var started = performance.now(), suiteTimer, timedOut = false;
    var unicode = '你好 café 😀\nsecond line\n';
    var cutText = '剪切 café 😀', richText = 'Bold 你好 😀';
    function log(line) { console.log(line); output.textContent += line + '\n'; output.scrollTop = output.scrollHeight; }
    function assert(value, message) { if (!value) throw new Error(message); }
    function delay(ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); }
    function value(element) { return element.isContentEditable ? element.textContent : element.value; }
    function focusRange(element, begin, end) {
        element.focus();
        if (element.isContentEditable) {
            var range = document.createRange(), selection = getSelection();
            range.setStart(element.firstChild, begin); range.setEnd(element.firstChild, end);
            selection.removeAllRanges(); selection.addRange(range);
        } else element.setSelectionRange(begin, end);
    }
    function selectAll(element) {
        element.focus();
        if (element.isContentEditable) {
            var range = document.createRange(); range.selectNodeContents(element);
            getSelection().removeAllRanges(); getSelection().addRange(range);
        } else element.setSelectionRange(0, element.value.length);
    }
    function action(operation, element) {
        var expectedCase = active, expectedStep = rows.length + 1;
        requestAnimationFrame(function () {
            if (rows.length + 1 !== expectedStep || (operation === 'start' ? busy : active !== expectedCase || !active)) return;
            var rect = element.getBoundingClientRect();
            log('ACTION: belle-clipboard ' + JSON.stringify({ run: token, step: expectedStep,
                operation: operation, target: element.id, mode: mode,
                x: rect.left + rect.width / 2, y: rect.top + rect.height / 2,
                width: innerWidth, height: innerHeight }));
        });
    }
    function waitFor(check, timeout, message) {
        return new Promise(function (resolve, reject) {
            var deadline = performance.now() + timeout;
            function poll() {
                try { if (check()) { resolve(); return; } }
                catch (error) { reject(error); return; }
                if (performance.now() >= deadline) { reject(new Error(message)); return; }
                setTimeout(poll, 30);
            }
            poll();
        });
    }
    ['copy', 'cut', 'paste', 'input', 'keydown'].forEach(function (kind) {
        document.addEventListener(kind, function (event) {
            if (!active) return;
            if (kind === 'keydown' && !(event.ctrlKey || event.metaKey)) return;
            var eventOwner = richSource.contains(event.target) ? richSource : richTarget.contains(event.target) ? richTarget : event.target;
            var row = { type: kind, target: eventOwner.id || '', actualTargetTag: event.target.nodeName || '', trusted: event.isTrusted,
                at_ms: Math.round(performance.now() - active.started) };
            if (kind === 'keydown') row.key = String(event.key).toLowerCase();
            if (kind === 'input') row.inputType = event.inputType || '';
            if (kind === 'paste') {
                var data = event.clipboardData;
                row.types = data ? Array.prototype.slice.call(data.types) : [];
                row.files = data ? data.files.length : -1;
                // Never log unknown clipboard contents. Compare only to the
                // known fixture value copied immediately before this paste.
                var text = data ? data.getData('text/plain') : '';
                row.plainTextMatches = text === active.expectedClipboard;
                row.utf16Length = text.length;
            }
            active.events.push(row);
        }, true);
    });
    function events(kind) { return active.events.filter(function (event) { return event.type === kind; }); }
    function trustedCommand(command, element, expectedAfter) {
        var before = events(command).length;
        assert(document.execCommand(command) === true, 'trusted ' + command + ' did not succeed');
        var added = events(command).slice(before);
        assert(added.length === 1 && added[0].trusted && added[0].target === element.id,
            'one trusted ' + command + ' event on the selected source');
        assert(value(element) === expectedAfter, command + ' changed the source incorrectly');
    }
    async function nativeCommand(command, element, checkAfter) {
        var previous = events(command).length;
        instruction.textContent = mode === 'keyboard' ? 'Send real Ctrl+' + (command === 'paste' ? 'V' : command === 'cut' ? 'X' : 'C') + ' to the focused field.'
            : 'Open Options and choose ' + command + '. Keep the page field selected.';
        if (command === 'cut' && mode === 'keyboard') instruction.textContent = 'Send real Ctrl+X to the focused field.';
        action(command, element);
        await waitFor(function () { return events(command).length > previous; }, 45000, 'real native ' + command + ' did not arrive within 45s');
        await delay(50);
        var added = events(command).slice(previous);
        assert(added.length === 1 && added[0].trusted && added[0].target === element.id,
            'one trusted native ' + command + ' event');
        if (mode === 'keyboard') {
            var key = command === 'cut' ? 'x' : command === 'copy' ? 'c' : 'v';
            assert(events('keydown').some(function (event) { return event.trusted && event.target === element.id && event.key === key; }),
                'native keyboard operation requires an observed trusted control key');
        }
        if (checkAfter) checkAfter();
    }
    async function paste(expected, editable) {
        active.expectedClipboard = expected;
        var destination = editable ? richTarget : target;
        target.hidden = !!editable; richTarget.hidden = !editable;
        if (editable) destination.textContent = 'A|B'; else destination.value = 'A|B';
        focusRange(destination, 1, 2);
        var beforeInput = events('input').length;
        await nativeCommand('paste', destination);
        await waitFor(function () { return value(destination) === 'A' + expected + 'B'; }, 3000, 'paste did not insert the exact text at the selected range');
        var pasted = events('paste');
        assert(pasted.length === 1 && pasted[0].plainTextMatches && pasted[0].files === 0,
            'trusted paste exposed exact fixture plain text and no files');
        assert(pasted[0].types.length && pasted[0].types.every(function (type) {
            type = type.toLowerCase(); return type === 'text/plain' || type === 'text/plain;charset=utf-8';
        }),
            'system bridge advertises plain text only');
        var changed = events('input').slice(beforeInput);
        assert(changed.some(function (event) { return event.trusted && event.target === destination.id && event.inputType === 'insertFromPaste'; }),
            'normal trusted insertFromPaste input event');
        if (editable) assert(!destination.querySelector('b,i,script,img'), 'plain-text paste cannot restore source markup');
        active.observations.insertedUTF16Length = expected.length;
    }
    function seed(text) {
        source.value = text; selectAll(source); trustedCommand('copy', source, text);
    }
    var tests = [
        ['trusted Unicode multiline copy and native paste', async function () {
            seed(unicode); await paste(unicode, false);
        }],
        ['trusted partial cut and native paste', async function () {
            source.value = 'left|' + cutText + '|right'; focusRange(source, 5, 5 + cutText.length);
            trustedCommand('cut', source, 'left||right'); await paste(cutText, false);
        }],
        ['contenteditable copy and plain native paste', async function () {
            richSource.hidden = false; source.hidden = true;
            richSource.innerHTML = '<b>Bold</b> <i>你好 😀</i>'; selectAll(richSource);
            trustedCommand('copy', richSource, richText); await paste(richText, true);
        }],
        ['DOM paste remains denied with a trusted click', async function () {
            target.value = 'unchanged'; focusRange(target, 2, 5);
            var beforePaste = events('paste').length, beforeInput = events('input').length;
            assert(document.execCommand('paste') === false, 'DOM paste must return false');
            await delay(350);
            assert(target.value === 'unchanged' && events('paste').length === beforePaste && events('input').length === beforeInput,
                'denied DOM paste must not dispatch a paste/input event or change text');
            active.observations.domPasteDenied = true;
        }],
        ['password copy and cut cannot replace clipboard', async function () {
            var sentinel = 'password-guard-' + token; seed(sentinel);
            special.hidden = false; special.type = 'password'; special.value = 'private fixture password'; selectAll(special);
            var beforeCopy = events('copy').length, beforeCut = events('cut').length;
            assert(document.execCommand('copy') === false && document.execCommand('cut') === false, 'password copy and cut must be disabled');
            assert(special.value === 'private fixture password' && events('copy').length === beforeCopy && events('cut').length === beforeCut,
                'password selection must not be exposed or deleted');
            await paste(sentinel, false);
        }],
        ['readonly cut preserves text and clipboard', async function () {
            var sentinel = 'readonly-guard-' + token; seed(sentinel);
            special.hidden = false; special.type = 'text'; special.readOnly = true; special.value = 'readonly source'; selectAll(special);
            var before = events('cut').length;
            assert(document.execCommand('cut') === false, 'readonly cut must be disabled');
            assert(special.value === 'readonly source' && events('cut').length === before, 'readonly text must remain unchanged');
            await paste(sentinel, false);
        }],
        ['native Copy and native Paste', async function () {
            var text = 'native copy 你好 😀'; source.value = text; selectAll(source);
            await nativeCommand('copy', source, function () { assert(source.value === text, 'native Copy cannot change source'); });
            await paste(text, false);
        }],
        ['native Cut and native Paste', async function () {
            var text = 'native cut café'; source.value = 'L|' + text + '|R'; focusRange(source, 2, 2 + text.length);
            await nativeCommand('cut', source, function () { assert(source.value === 'L||R', 'native Cut must delete exactly the selected text'); });
            await paste(text, false);
        }],
        ['native SelectAll then Copy and Paste', async function () {
            var text = 'all selected 你好 😀'; source.value = text; focusRange(source, 2, 2);
            instruction.textContent = mode === 'keyboard' ? 'Send real Ctrl+A, then follow the next instruction.' : 'Choose Options → Select all, then follow the next instruction.';
            action('select-all', source);
            await waitFor(function () { return source.selectionStart === 0 && source.selectionEnd === text.length; }, 45000, 'native Select all did not select the entire field');
            if (mode === 'keyboard') assert(events('keydown').some(function (event) { return event.trusted && event.key === 'a' && event.target === source.id; }), 'real Ctrl+A key event');
            await nativeCommand('copy', source); await paste(text, false);
        }]
    ];
    function reset() {
        source.hidden = false; richSource.hidden = true; richTarget.hidden = true; special.hidden = true; target.hidden = false;
        special.readOnly = false; source.readOnly = false; target.readOnly = false;
        source.value = ''; target.value = ''; richSource.textContent = ''; richTarget.textContent = '';
    }
    async function publish() {
        clearTimeout(suiteTimer); button.disabled = true;
        var passed = rows.filter(function (row) { return row.status === 'PASS'; }).length;
        var report = { suite: 'belle-clipboard', run: token, mode: mode, origin: location.origin,
            status: passed === tests.length ? 'PASS' : 'FAIL', passed: passed, failed: rows.length - passed,
            total: tests.length, elapsed_ms: Math.round(performance.now() - started), cases: rows };
        log('RESULT: belle-clipboard ' + JSON.stringify(report));
        stage.textContent = report.status + ': ' + passed + '/' + tests.length;
        try {
            var response = await fetch('/report', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(report) });
            assert(response.status === 201, 'report HTTP status ' + response.status);
            log('STORED: belle-clipboard ' + token);
        } catch (error) { log('FAIL: clipboard report storage: ' + String(error)); }
    }
    function next() {
        if (rows.length === tests.length) { publish(); return; }
        reset(); button.disabled = false; button.textContent = 'Start case ' + (rows.length + 1);
        stage.textContent = (rows.length + 1) + '/' + tests.length + ': ' + tests[rows.length][0];
        instruction.textContent = 'Tap Start with real pointer input. The page prepares selection; you perform the requested native command.';
        action('start', button);
    }
    button.addEventListener('click', async function (event) {
        if (busy || rows.length === tests.length) return;
        busy = true; button.disabled = true;
        var test = tests[rows.length];
        active = { started: performance.now(), expectedClipboard: null, events: [], observations: {} };
        log('BEGIN: belle-clipboard ' + JSON.stringify({ run: token, step: rows.length + 1, name: test[0], trustedClick: event.isTrusted }));
        var failure = null;
        try { assert(event.isTrusted, 'case must start from genuine pointer/keyboard input'); await test[1](); assert(!timedOut, 'overall interaction deadline was exceeded'); }
        catch (error) { failure = String(error); }
        var row = { name: test[0], status: failure ? 'FAIL' : 'PASS', elapsed_ms: Math.round(performance.now() - active.started),
            trustedClick: event.isTrusted, events: active.events, observations: active.observations };
        if (failure) row.error = failure;
        rows.push(row); log((failure ? 'FAIL: ' : 'PASS: ') + test[0] + (failure ? ': ' + failure : ''));
        active = null; busy = false; next();
    });
    if (!/^[A-Za-z0-9_-]{1,80}$/.test(token) || !/^(keyboard|menu)$/.test(mode)) {
        log('FAIL: require unique ?run=ASCII_TOKEN&mode=keyboard|menu'); return;
    }
    suiteTimer = setTimeout(function () {
        timedOut = true;
        button.disabled = true;
        log('FAIL: belle-clipboard overall 480s interaction deadline exceeded; incomplete cases cannot pass');
    }, 480000);
    // Native input coordinates must reflect the settled meta-viewport scale.
    window.addEventListener('load', function () {
        requestAnimationFrame(function () { requestAnimationFrame(function () {
            log('START: belle-clipboard ' + JSON.stringify({ run: token, mode: mode, total: tests.length }));
            next();
        }); });
    });
}());

(function () {
    'use strict';
    var sampleName = '上传-你好.txt';
    var sampleText = 'Symbian UTF-8 upload: 你好, café.\n';
    var unicodeDefault = '你好 café — default';
    var runToken = new URLSearchParams(location.search).get('run') || 'manual';
    var passed = 0, failed = 0, step = 0, busy = false;
    var result = document.getElementById('result');
    var button = document.getElementById('next');
    var instruction = document.getElementById('instruction');
    var single = document.getElementById('single');
    var multiple = document.getElementById('multiple');
    function log(message) { console.log(message); result.textContent += '\n' + message; }
    function actionPoint(target, rect) {
        // Observation only: the host sends real X11 mouse/key events. This does
        // not invoke a button, select files, or substitute a dialog result.
        log('ACTION: belle-native-ui ' + JSON.stringify({ run: runToken, step: step + 1,
            target: target, x: rect.left + rect.width / 2, y: rect.top + rect.height / 2,
            width: innerWidth, height: innerHeight }));
    }
    function assert(value, message) { if (!value) throw new Error(message); }
    function report(name, error) {
        if (error) { ++failed; log('FAIL: ' + name + ': ' + error); }
        else { ++passed; log('PASS: ' + name); }
        busy = false;
        ++step;
        show();
    }
    function read(file) {
        return new Promise(function (resolve, reject) {
            var reader = new FileReader();
            reader.onload = function () { resolve(reader.result); };
            reader.onerror = function () { reject(reader.error); };
            reader.readAsText(file);
        });
    }
    async function verifyUpload(input) {
        assert(input.files.length === 1, 'exactly one selected file');
        var file = input.files[0];
        assert(file.name === sampleName, 'Unicode basename; no native path exposure');
        assert(await read(file) === sampleText, 'exact FileReader UTF-8 payload');
        var form = new FormData();
        form.append('file', file);
        var response = await fetch('/upload', { method: 'POST', body: form });
        assert(response.ok, 'HTTP upload status ' + response.status);
        var info = await response.json();
        assert(info.name === sampleName && info.contentMatches && info.bytes === file.size,
            'server received exact filename and body bytes');
        log('UPLOAD: ' + info.sha256 + ' bytes=' + info.bytes);
    }
    function select(input, cancelled) {
        return new Promise(function (resolve, reject) {
            var old = input.files.length ? input.files[0] : null;
            var timer = setTimeout(function () { finish(new Error('dialog did not settle within 90s')); }, 90000);
            function finish(error) {
                clearTimeout(timer);
                input.removeEventListener('change', changed);
                input.removeEventListener('cancel', cancel);
                if (error) reject(error); else resolve();
            }
            function changed() {
                if (cancelled) { finish(new Error('cancel changed the file list')); return; }
                verifyUpload(input).then(function () { finish(); }, finish);
            }
            function cancel() {
                if (!cancelled) { finish(new Error('selection cancelled')); return; }
                if (!old || input.files.length !== 1 || input.files[0] !== old) {
                    finish(new Error('cancel did not preserve prior selection')); return;
                }
                read(input.files[0]).then(function (text) {
                    finish(text === sampleText ? null : new Error('preserved file content changed'));
                }, finish);
            }
            input.addEventListener('change', changed);
            input.addEventListener('cancel', cancel);
            input.click();
        });
    }
    function invalidatedFrame(navigate) {
        return new Promise(function (resolve, reject) {
            var frame = document.createElement('iframe'), detached = false, staleChange = false, navigationStarted = false;
            frame.onload = function () {
                if (navigationStarted) { detached = true; log('LIFECYCLE: iframe navigation completed while picker is open'); }
                else {
                    var outer = frame.getBoundingClientRect();
                    var child = frame.contentDocument.querySelector('button').getBoundingClientRect();
                    actionPoint('child', { left: outer.left + frame.clientLeft + child.left,
                        top: outer.top + frame.clientTop + child.top, width: child.width, height: child.height });
                }
            };
            var timer = setTimeout(function () { finish(new Error('frame test did not finish within 90s')); }, 90000);
            function receive(event) {
                if (event.source !== frame.contentWindow && !detached) return;
                if (event.data === 'belle-ui-opening') {
                    setTimeout(function () {
                        if (navigate) { navigationStarted = true; frame.srcdoc = '<p>Replacement document</p>'; }
                        else {
                            frame.remove(); detached = true;
                            log('LIFECYCLE: iframe detached while native picker is open');
                        }
                    }, 2500);
                } else if (event.data === 'belle-ui-stale-change') staleChange = true;
                else if (event.data === 'belle-ui-returned') {
                    setTimeout(function () {
                        finish(!detached ? new Error('picker closed before frame detached')
                            : staleChange ? new Error('detached input received chosen file') : null);
                    }, 500);
                }
            }
            function finish(error) {
                clearTimeout(timer); window.removeEventListener('message', receive); frame.remove();
                if (error) reject(error); else resolve();
            }
            window.addEventListener('message', receive);
            // Keep a parent reference in the child because window.parent changes
            // after detach. Native chooser completion must not choose a stale file.
            frame.srcdoc = '<meta charset="utf-8"><button style="height:64px;width:100%" onclick="openFile()">Open child picker; wait 5 seconds</button><input type="file" id="f" hidden><script>var host=parent;function openFile(){var f=document.getElementById("f");f.onchange=function(){host.postMessage("belle-ui-stale-change","*")};host.postMessage("belle-ui-opening","*");f.click();host.postMessage("belle-ui-returned","*");}<\/script>';
            document.getElementById('frame').appendChild(frame);
        });
    }
    var tests = [
        ['Unicode prompt default', 'Press OK without editing the Unicode default.', function () {
            assert(prompt('Confirm Unicode default', unicodeDefault) === unicodeDefault, 'exact default value');
        }],
        ['Empty prompt accepted', 'Press OK with the empty input. Do not press Cancel.', function () {
            assert(prompt('Accept empty text', '') === '', 'empty string differs from null');
        }],
        ['Prompt cancellation', 'Press Cancel. The existing default must not be returned.', function () {
            assert(prompt('Cancel this prompt', 'must not return') === null, 'cancel returns null');
        }],
        ['Unicode single-file upload', 'Choose the supplied ' + sampleName + ' from the native picker.', function () { return select(single, false); }],
        ['Single file on multiple input', 'Choose the same file. This UI selects one file even when HTML permits multiple.', function () { return select(multiple, false); }],
        ['Cancel preserves selected file', 'Open the picker, then press Cancel. The earlier selected file must remain.', function () { return select(single, true); }],
        ['Detached-frame chooser lifetime', 'Tap Start, then the child button. Keep the picker open for 5 seconds before choosing the sample. The child frame disappears during the wait.', function () { return invalidatedFrame(false); }],
        ['Navigated-frame chooser lifetime', 'Tap Start, then the child button. Keep the picker open for 5 seconds before choosing the sample. The child navigates to a replacement document.', function () { return invalidatedFrame(true); }]
    ];
    function show() {
        if (step === tests.length) {
            button.disabled = true; button.textContent = 'Finished';
            instruction.textContent = 'Native interaction sequence finished.';
            log('RESULT: belle-native-ui passed=' + passed + ' failed=' + failed + ' total=' + tests.length);
            log((failed ? 'FAIL: ' : 'PASS: ') + 'belle-native-ui');
            document.title = (failed ? 'FAIL' : 'PASS') + ' Belle native UI';
            return;
        }
        instruction.textContent = tests[step][1];
        button.textContent = (step + 1) + '/' + tests.length + ': ' + tests[step][0];
        button.disabled = false;
        requestAnimationFrame(function () { actionPoint('next', button.getBoundingClientRect()); });
    }
    button.onclick = function () {
        if (busy || step === tests.length) return;
        busy = true; button.disabled = true;
        var test = tests[step];
        log('BEGIN: ' + test[0]);
        try { Promise.resolve(test[2]()).then(function () { report(test[0]); }, function (e) { report(test[0], e); }); }
        catch (e) { report(test[0], e); }
    };
    log('READY: belle-native-ui ' + JSON.stringify({ run: runToken }));
    show();
}());

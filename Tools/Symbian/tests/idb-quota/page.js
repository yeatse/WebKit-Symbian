'use strict';
(async function () {
    const args = new URL(location.href).searchParams;
    const run = args.get('run');
    const phase = args.get('phase') || 'write';
    const basePort = Number(args.get('basePort') || 8790);
    const started = performance.now();
    const prefix = 'symbian-idb-quota-' + run + '-';
    const MiB = 1024 * 1024;
    const block = new Uint8Array(MiB);
    block[0] = 17; block[MiB - 1] = 239;
    const output = document.getElementById('output');
    const results = [];
    const connections = new Set();
    if (!/^[a-zA-Z0-9_-]{8,64}$/.test(run || '')) throw new Error('Explicit unique run token required');
    function assert(condition, message) { if (!condition) throw new Error(message); }
    function request(req) { return new Promise((resolve, reject) => { req.onsuccess = () => resolve(req.result); req.onerror = () => reject(req.error); }); }
    function complete(tx) { return new Promise((resolve, reject) => { tx.oncomplete = () => resolve(); tx.onabort = () => reject(tx.error || new DOMException('Aborted', 'AbortError')); tx.onerror = () => {}; }); }
    async function open(name) {
        const req = indexedDB.open(prefix + name, 1);
        req.onupgradeneeded = () => req.result.createObjectStore('data');
        const db = await request(req); connections.add(db); return db;
    }
    function close(db) { db.close(); connections.delete(db); }
    async function remove(name) {
        for (const db of connections) if (db.name === prefix + name) close(db);
        await request(indexedDB.deleteDatabase(prefix + name));
    }
    async function put(db, key, value) { const tx = db.transaction('data', 'readwrite'); const done = complete(tx); tx.objectStore('data').put(value, key); await done; }
    async function get(db, key) { const tx = db.transaction('data'); const done = complete(tx); const value = await request(tx.objectStore('data').get(key)); await done; return value; }
    async function count(db) { const tx = db.transaction('data'); const done = complete(tx); const value = await request(tx.objectStore('data').count()); await done; return value; }
    async function clear(db, abort) {
        const tx = db.transaction('data', 'readwrite'); const done = complete(tx);
        tx.objectStore('data').clear().onsuccess = () => { if (abort) tx.abort(); };
        try { await done; assert(!abort, 'clear abort must not commit'); } catch (error) { if (!abort) throw error; }
    }
    async function blobText(blob) { return new Promise((resolve, reject) => { const reader = new FileReader(); reader.onload = () => resolve(reader.result); reader.onerror = () => reject(reader.error); reader.readAsText(blob); }); }
    async function assertBlobPattern(blob, blocks) {
        assert(blob.size === blocks * MiB, 'Blob metadata length matches expected blocks');
        for (let index = 0; index < blocks; ++index) {
            const bytes = await new Promise((resolve, reject) => {
                const reader = new FileReader();
                reader.onload = () => resolve(new Uint8Array(reader.result));
                reader.onerror = () => reject(reader.error);
                reader.readAsArrayBuffer(blob.slice(index * MiB, (index + 1) * MiB));
            });
            assert(bytes.length === MiB && bytes[0] === 17 && bytes[MiB - 1] === 239, 'physical Blob block length/endpoints ' + index);
            for (let byte = 1; byte < MiB - 1; ++byte)
                if (bytes[byte] !== 0) throw new Error('physical Blob payload block ' + index + ' byte ' + byte);
        }
    }
    async function fill(name, maximum) {
        const db = await open(name); let stored = 0; let errorName = '';
        for (let index = 0; index < maximum; ++index) {
            try { await put(db, index, block); ++stored; } catch (error) { errorName = error.name; break; }
        }
        assert(await count(db) === stored, 'failed write cannot add a record'); close(db);
        return { stored, errorName };
    }
    async function atomicOverflow() {
        const db = await open('atomic'); let requestError = ''; let added = 0;
        const tx = db.transaction('data', 'readwrite'); const done = complete(tx);
        function step() {
            if (added === 40) return;
            const req = tx.objectStore('data').put(block, added);
            req.onerror = () => { requestError = req.error.name; };
            req.onsuccess = () => { ++added; step(); };
        }
        step(); let aborted = false;
        try { await done; } catch (_) { aborted = true; }
        assert(aborted && requestError === 'QuotaExceededError' && added >= 24 && added < 32, 'oversize transaction must reach the expected 32 MiB boundary, not a smaller per-transaction cap');
        assert(await count(db) === 0, 'oversize transaction must roll back all earlier writes');
        close(db); await remove('atomic'); return { acceptedBeforeAbort: added };
    }
    async function parallelPending() {
        const databases = await Promise.all([open('pending-a'), open('pending-b')]);
        const transactions = databases.map(db => db.transaction('data', 'readwrite'));
        let stopped = false; let quotaError = ''; let unexpectedCommit = false;
        const added = [0, 0];
        const abortErrors = ['', ''];
        const outcomes = transactions.map((tx, index) => new Promise(resolve => {
            tx.onabort = () => { abortErrors[index] = tx.error ? tx.error.name : 'explicit'; resolve(); }; tx.onerror = () => {};
            tx.oncomplete = () => { unexpectedCommit = true; resolve(); };
        }));
        function stop() { if (stopped) return; stopped = true; for (const tx of transactions) { try { tx.abort(); } catch (_) {} } }
        function step(index) {
            if (stopped) return;
            const store = transactions[index].objectStore('data');
            // Keep a completed writer alive while its peer still holds space.
            const req = added[index] < 20 ? store.put(block, added[index]) : store.get(0);
            req.onerror = event => { event.preventDefault(); if (!quotaError) quotaError = req.error.name; stop(); };
            req.onsuccess = () => { if (added[index] < 20) ++added[index]; if (added[0] === 20 && added[1] === 20) stop(); else step(index); };
        }
        step(0); step(1); await Promise.all(outcomes);
        assert(!unexpectedCommit && quotaError === 'QuotaExceededError' && added[0] > 0 && added[1] > 0 && added[0] + added[1] >= 24 && added[0] + added[1] < 32, 'same-origin databases must share pending reservations at the expected boundary: ' + JSON.stringify({ added, quotaError, unexpectedCommit, abortErrors }));
        assert(await count(databases[0]) === 0 && await count(databases[1]) === 0, 'both explicit aborts must release all pending writes');
        databases.forEach(close); await remove('pending-a'); await remove('pending-b'); return { added };
    }
    if (args.get('peer') === '1') {
        const expectedOrigin = location.protocol + '//' + location.hostname + ':' + basePort;
        window.addEventListener('message', async event => {
            if (event.source !== parent || event.origin !== expectedOrigin || !event.data || event.data.run !== run) return;
            const message = event.data; let value; let error;
            try {
                if (message.command === 'fill') value = await fill(message.name, message.maximum);
                else if (message.command === 'delete') { await remove(message.name); value = true; }
                else if (message.command === 'stageBlob') {
                    const db = await open('blob-persistent');
                    await put(db, 'payload', new Blob(new Array(20).fill(block)));
                    close(db); value = true;
                } else if (message.command === 'verifyBlob') {
                    const db = await open('blob-persistent');
                    assert(await count(db) === 1, 'one committed large Blob after process restart');
                    await assertBlobPattern(await get(db, 'payload'), 20);
                    close(db); value = true;
                } else if (message.command === 'verifyBlobDeleted') {
                    const db = await open('blob-persistent');
                    assert(await count(db) === 0, 'large Blob deletion survives second process restart');
                    close(db); await remove('blob-persistent'); value = true;
                }
                else throw new Error('Unknown peer command');
            } catch (exception) { error = String(exception); }
            parent.postMessage({ run, id: message.id, value, error }, expectedOrigin);
        });
        parent.postMessage({ run, ready: true }, expectedOrigin); return;
    }
    let nextMessage = 0;
    async function peer(index) {
        const origin = location.protocol + '//' + location.hostname + ':' + (basePort + index);
        const frame = document.createElement('iframe'); frame.hidden = true;
        const waiting = new Map();
        const ready = new Promise(resolve => window.addEventListener('message', event => {
            if (event.source !== frame.contentWindow || event.origin !== origin || !event.data || event.data.run !== run) return;
            if (event.data.ready) return resolve();
            const entry = waiting.get(event.data.id); if (!entry) return;
            waiting.delete(event.data.id); event.data.error ? entry.reject(new Error(event.data.error)) : entry.resolve(event.data.value);
        }));
        frame.src = origin + '/?run=' + encodeURIComponent(run) + '&peer=1&basePort=' + basePort;
        document.body.appendChild(frame); await ready;
        return (command, data) => new Promise((resolve, reject) => { const id = ++nextMessage; waiting.set(id, { resolve, reject }); frame.contentWindow.postMessage(Object.assign({ run, id, command }, data), origin); });
    }
    async function test(name, body) {
        let details; let error;
        try { details = await body(); } catch (exception) { error = String(exception) + (exception && exception.stack ? '\n' + exception.stack : ''); }
        const result = { name, pass: !error, details: details || null, error: error || null }; results.push(result);
        const line = 'IDB_QUOTA_CASE run=' + run + ' phase=' + phase + ' ' + (result.pass ? 'PASS ' : 'FAIL ') + name + (error ? ' ' + error : '');
        console.log(line); output.textContent += '\n' + line;
        // Continue independent cases, but each case cleans only its token's DBs.
    }
    const deadline = setTimeout(() => { console.log('IDB_QUOTA_TIMEOUT run=' + run + ' phase=' + phase); output.className = 'fail'; }, 240000);
    if (phase === 'write') {
        await test('blob replacement and repeated put are atomic', async () => {
            const db = await open('blob'); await put(db, 'key', new Blob(['old-value']));
            await put(db, 'key', new Blob(['replacement']));
            assert(await blobText(await get(db, 'key')) === 'replacement', 'same-key replacement payload');
            const tx = db.transaction('data', 'readwrite'); const done = complete(tx);
            tx.objectStore('data').put(new Blob(['intermediate']), 'key');
            tx.objectStore('data').put(new Blob(['final-value']), 'key'); await done;
            assert(await blobText(await get(db, 'key')) === 'final-value', 'same-transaction replacement payload');
            close(db);
        });
        await test('Blob clear abort preserves old committed payload', async () => {
            const db = await open('blob'); await clear(db, true);
            assert(await blobText(await get(db, 'key')) === 'final-value', 'aborted clear retains Blob'); close(db);
        });
        await test('Blob clear commit and put-clear release references', async () => {
            const db = await open('blob'); await clear(db, false); assert(await count(db) === 0, 'clear removes records');
            const tx = db.transaction('data', 'readwrite'); const done = complete(tx);
            tx.objectStore('data').put(new Blob(['never committed']), 'temporary'); tx.objectStore('data').clear(); await done;
            assert(await count(db) === 0, 'put-clear leaves no record'); close(db); await remove('blob');
        });
        await test('Blob payload quota rejection preserves source and old record', async () => {
            const db = await open('bigblob'); await put(db, 'key', new Blob(['old-blob']));
            const source = new Blob(new Array(34).fill(block));
            let errorName = '';
            try { await put(db, 'key', source); } catch (error) { errorName = error.name; }
            assert(errorName === 'QuotaExceededError', '34 MiB Blob payload must count toward quota');
            assert(await blobText(await get(db, 'key')) === 'old-blob', 'rejected replacement preserves committed Blob');
            const first = await blobText(source.slice(0, 1));
            assert(first.charCodeAt(0) === 17 && source.size === 34 * MiB, 'source Blob remains readable after rejected copy');
            close(db); await remove('bigblob');
        });
        await test('Blob clear commit returns quota to another database', async () => {
            const source = new Blob(new Array(20).fill(block));
            const first = await open('clear-capacity-a'); await put(first, 'large', source); await clear(first, false); close(first);
            const second = await open('clear-capacity-b'); await put(second, 'large', source);
            assert((await get(second, 'large')).size === 20 * MiB, 'cleared 20 MiB Blob must no longer consume origin quota');
            close(second); await remove('clear-capacity-a'); await remove('clear-capacity-b');
        });
        await test('one transaction cannot exceed origin quota', atomicOverflow);
        await test('parallel databases share pending origin reservations', parallelPending);
        await test('durable writes stop below 32 MiB plus SQLite metadata', async () => {
            const filled = await fill('full', 40);
            assert(filled.errorName === 'QuotaExceededError' && filled.stored >= 16 && filled.stored < 32, '32 MiB origin boundary ' + JSON.stringify(filled));
            return filled;
        });
        const independent = await peer(1);
        await test('another origin remains writable at origin quota', async () => {
            const filled = await independent('fill', { name: 'isolated', maximum: 2 });
            assert(filled.stored === 2 && !filled.errorName, 'origin must retain independent capacity'); await independent('delete', { name: 'isolated' }); return filled;
        });
        await test('deleteDatabase returns space for subsequent writes', async () => {
            await remove('full'); const filled = await fill('reuse', 2);
            assert(filled.stored === 2 && !filled.errorName, 'space reused after deletion'); await remove('reuse'); return filled;
        });
        await test('multiple origins share 128 MiB port quota', async () => {
            const peers = [independent]; for (let index = 2; index <= 5; ++index) peers.push(await peer(index));
            let stored = 0; let refused = false; const values = [];
            try {
                for (const remote of peers) {
                    const value = await remote('fill', { name: 'port-limit', maximum: 28 }); values.push(value); stored += value.stored;
                    if (value.errorName) { assert(value.errorName === 'QuotaExceededError', 'port error must be quota'); refused = true; break; }
                }
                assert(refused && stored >= 100 && stored < 128, '128 MiB shared boundary ' + JSON.stringify(values));
            } finally { for (const remote of peers) await remote('delete', { name: 'port-limit' }); }
            return { storedMiB: stored, origins: values };
        });
        await test('stage committed data for independent process restart', async () => {
            const db = await open('persistent'); await put(db, 'array', block);
            for (let index = 1; index < 24; ++index) await put(db, index, block);
            await put(db, 'blob', new Blob(['persist:' + run]));
            await put(db, 'marker', run); close(db);
            // A distinct origin keeps the original 24 MiB restart accounting
            // unchanged. Verify the large sidecar only after the process exits:
            // same-process IDB reads may reuse the source Blob's memory URL.
            await independent('stageBlob', {});
        });
    } else if (phase === 'read') {
        await test('restart recovers committed array and Blob payloads', async () => {
            const db = await open('persistent'); const value = await get(db, 'array');
            assert(value.length === MiB && value[0] === 17 && value[MiB - 1] === 239, 'persisted array exact endpoints');
            assert(await count(db) === 26, 'all 24 payload blocks and both metadata records persisted');
            for (let index = 1; index < 24; ++index) {
                const stored = await get(db, index);
                assert(stored.length === MiB && stored[0] === 17 && stored[MiB - 1] === 239, 'persisted block ' + index);
            }
            assert(await blobText(await get(db, 'blob')) === 'persist:' + run && await get(db, 'marker') === run, 'persisted Blob and run marker'); close(db);
            const remote = await peer(1); await remote('verifyBlob', {});
        });
        await test('restart reloads committed origin usage before granting space', async () => {
            const db = await open('persistent'); let added = 0; let errorName = '';
            for (let index = 24; index < 40; ++index) {
                try { await put(db, index, block); ++added; } catch (error) { errorName = error.name; break; }
            }
            assert(errorName === 'QuotaExceededError' && added > 0 && added < 8, 'persisted 24 MiB must constrain next process quota without denying all writes');
            assert(await count(db) === 26 + added, 'failed restarted write must not add a record');
            close(db); return { addedMiB: added };
        });
        await test('delete committed data before second restart', async () => {
            await remove('persistent');
            const remote = await peer(1); await remote('delete', { name: 'blob-persistent' });
        });
    } else if (phase === 'erased') {
        await test('second restart observes deletion and allows fresh write', async () => {
            const db = await open('persistent'); assert(await count(db) === 0, 'deleted database stays empty'); await put(db, 'new', block);
            assert((await get(db, 'new')).length === MiB, 'fresh allocation after restart'); close(db); await remove('persistent');
            const remote = await peer(1); await remote('verifyBlobDeleted', {});
        });
    } else if (phase === 'cleanup') {
        await test('remove only this run token across all test origins', async () => {
            const names = ['blob', 'bigblob', 'clear-capacity-a', 'clear-capacity-b', 'atomic', 'pending-a', 'pending-b', 'full', 'reuse', 'persistent', 'blob-persistent', 'isolated', 'port-limit'];
            for (const name of names) await remove(name);
            for (let index = 1; index <= 5; ++index) {
                const remote = await peer(index);
                for (const name of names) await remote('delete', { name });
            }
        });
    } else throw new Error('Unknown phase');
    clearTimeout(deadline);
    for (const db of connections) close(db);
    const passed = results.filter(item => item.pass).length;
    const report = { schema: 1, run, phase, origin: location.origin, elapsed_ms: Math.round(performance.now() - started), passed, total: results.length, results };
    const response = await fetch('/report?run=' + encodeURIComponent(run) + '&phase=' + phase, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(report) });
    assert(response.status === 201, 'report must be stored once successfully');
    const final = 'IDB_QUOTA_DONE run=' + run + ' phase=' + phase + ' passed=' + passed + ' total=' + results.length;
    console.log(final); output.textContent += '\n' + final; if (passed !== results.length) output.className = 'fail';
})().catch(error => { console.log('IDB_QUOTA_FATAL ' + String(error && error.stack || error)); document.getElementById('output').textContent += '\nFATAL ' + error; });

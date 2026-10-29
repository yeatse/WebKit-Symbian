'use strict';
(async function () {
    const run = new URL(location.href).searchParams.get('run');
    if (!/^[A-Za-z0-9_-]{8,64}$/.test(run || '')) throw new Error('Explicit unique run token required');
    const started = performance.now();
    const output = document.getElementById('output');
    const results = [];
    const subtle = crypto.subtle;
    function assert(value, message) { if (!value) throw new Error(message); }
    function bytes(hex) { return Uint8Array.from(hex.match(/../g) || [], value => parseInt(value, 16)); }
    function equal(left, right) { left = new Uint8Array(left); right = new Uint8Array(right); return left.length === right.length && left.every((value, index) => value === right[index]); }
    async function rejects(name, body) {
        let error;
        try { await body(); } catch (exception) { error = exception; }
        assert(error && error.name === name, 'expected ' + name + ', received ' + (error ? error.name : 'success'));
    }
    async function test(name, body) {
        let error;
        try { await body(); } catch (exception) { error = String(exception.stack || exception); }
        results.push({ name, pass: !error, error: error || null });
        const line = 'RSA_CASE run=' + run + ' ' + (error ? 'FAIL ' : 'PASS ') + name + (error ? ' ' + error : '');
        console.log(line); output.textContent += '\n' + line;
    }
    const timeout = setTimeout(() => console.log('RSA_TIMEOUT run=' + run), 180000);
    const vectors = await (await fetch('vectors.json')).json();
    for (const vector of vectors) {
        await test('known answer ' + vector.name, async () => {
            const algorithm = vector.algorithm.name;
            const privateUsage = algorithm === 'RSA-OAEP' ? 'decrypt' : 'sign';
            const publicUsage = algorithm === 'RSA-OAEP' ? 'encrypt' : 'verify';
            const privateKey = await subtle.importKey('pkcs8', bytes(vector.pkcs8), { name: algorithm, hash: vector.hash }, true, [privateUsage]);
            const publicKey = await subtle.importKey('spki', bytes(vector.spki), { name: algorithm, hash: vector.hash }, true, [publicUsage]);
            const parameters = Object.assign({}, vector.algorithm);
            if (parameters.label) parameters.label = new Uint8Array(parameters.label);
            if (algorithm === 'RSA-OAEP') {
                const plaintext = await subtle.decrypt(parameters, privateKey, bytes(vector.result));
                assert(equal(plaintext, bytes(vector.data)), 'independent OAEP known plaintext');
                const encrypted = await subtle.encrypt(parameters, publicKey, bytes(vector.data));
                assert(equal(await subtle.decrypt(parameters, privateKey, encrypted), bytes(vector.data)), 'OAEP roundtrip');
            } else {
                assert(await subtle.verify(parameters, publicKey, bytes(vector.result), bytes(vector.data)), 'independent known signature');
                const signature = await subtle.sign(parameters, privateKey, bytes(vector.data));
                assert(await subtle.verify(parameters, publicKey, signature, bytes(vector.data)), 'RSA signature roundtrip');
                if (algorithm === 'RSASSA-PKCS1-v1_5' || !parameters.saltLength)
                    assert(equal(signature, bytes(vector.result)), 'exact deterministic signature');
            }
        });
    }
    const source = vectors[0];
    const message = new Uint8Array([1, 0, 255, 7]);
    async function pair(name, hash = 'SHA-256', extractable = true) {
        return {
            privateKey: await subtle.importKey('pkcs8', bytes(source.pkcs8), { name, hash }, extractable, name === 'RSA-OAEP' ? ['decrypt', 'unwrapKey'] : ['sign']),
            publicKey: await subtle.importKey('spki', bytes(source.spki), { name, hash }, true, name === 'RSA-OAEP' ? ['encrypt', 'wrapKey'] : ['verify'])
        };
    }
    await test('generateKey metadata usages extractability and asynchronous completion', async () => {
        let timerRan = false; setTimeout(() => { timerRan = true; }, 0);
        const keys = await subtle.generateKey({ name: 'RSA-PSS', modulusLength: 2048, publicExponent: new Uint8Array([0, 1, 0, 1]), hash: 'SHA-256' }, false, ['sign', 'verify']);
        assert(timerRan, 'key generation must yield the script thread');
        assert(keys.publicKey.extractable && !keys.privateKey.extractable, 'public/private extractability');
        assert(keys.privateKey.algorithm.modulusLength === 2048 && keys.privateKey.algorithm.hash.name === 'SHA-256', 'key algorithm metadata');
        assert(equal(keys.publicKey.algorithm.publicExponent, new Uint8Array([1, 0, 1])), 'minimal public exponent');
        assert(keys.privateKey.usages.join() === 'sign' && keys.publicKey.usages.join() === 'verify', 'split key usages');
        await rejects('InvalidAccessError', () => subtle.exportKey('pkcs8', keys.privateKey));
        const signature = await subtle.sign({ name: 'RSA-PSS', saltLength: 32 }, keys.privateKey, message);
        assert(await subtle.verify({ name: 'RSA-PSS', saltLength: 32 }, keys.publicKey, signature, message), 'generated private key really signs');
    });
    await test('all three algorithms support JWK SPKI PKCS8 roundtrips', async () => {
        for (const name of ['RSA-OAEP', 'RSA-PSS', 'RSASSA-PKCS1-v1_5']) {
            const keys = await pair(name);
            const spki = await subtle.exportKey('spki', keys.publicKey);
            const pkcs8 = await subtle.exportKey('pkcs8', keys.privateKey);
            assert(equal(spki, bytes(source.spki)) && equal(pkcs8, bytes(source.pkcs8)), 'standard key DER exact roundtrip');
            for (const key of [keys.publicKey, keys.privateKey]) {
                const jwk = await subtle.exportKey('jwk', key);
                const imported = await subtle.importKey('jwk', jwk, { name, hash: 'SHA-256' }, true, key.usages);
                assert(imported.type === key.type && imported.algorithm.modulusLength === 2048, 'JWK key metadata');
                assert(JSON.stringify(await subtle.exportKey('jwk', imported)) === JSON.stringify(jwk), 'JWK exact component roundtrip');
            }
        }
    });
    await test('minimal private JWK works and malformed CRT or partial fields are rejected', async () => {
        const keys = await pair('RSASSA-PKCS1-v1_5');
        const full = await subtle.exportKey('jwk', keys.privateKey);
        const minimal = Object.assign({}, full); for (const field of ['p', 'q', 'dp', 'dq', 'qi']) delete minimal[field];
        const imported = await subtle.importKey('jwk', minimal, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']);
        assert(equal(await subtle.sign('RSASSA-PKCS1-v1_5', imported, message), await subtle.sign('RSASSA-PKCS1-v1_5', keys.privateKey, message)), 'N/E/D completed key signs identically');
        for (const field of ['dp', 'dq', 'qi']) {
            const changed = Object.assign({}, full); changed[field] = 'AQ';
            await rejects('DataError', () => subtle.importKey('jwk', changed, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']));
        }
        minimal.dq = full.dq;
        await rejects('DataError', () => subtle.importKey('jwk', minimal, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']));
        for (const oth of [[], [{ r: full.p, d: full.dp, t: full.qi }]])
            await rejects('DataError', () => subtle.importKey('jwk', Object.assign({}, full, { oth }), { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, true, ['sign']));
    });
    await test('DER rejects trailing truncated noncanonical and wrong formats', async () => {
        const original = bytes(source.pkcs8);
        const trailing = new Uint8Array(original.length + 1); trailing.set(original);
        const noncanonical = new Uint8Array(original.length + 1); noncanonical.set([0x30, 0x83, 0]); noncanonical.set(original.slice(2), 3);
        for (const input of [trailing, original.slice(0, -1), noncanonical, bytes(source.spki), new Uint8Array([0x30, 0x84, 255, 255, 255, 255])])
            await rejects('DataError', () => subtle.importKey('pkcs8', input, { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']));
        function withAttributes(attributes) {
            const prefix = 2 + (original[1] & 0x80 ? original[1] & 0x7f : 0);
            const content = Array.from(original.slice(prefix)).concat(attributes);
            const length = content.length;
            return new Uint8Array([0x30, 0x82, length >> 8, length & 255].concat(content));
        }
        const friendlyName = [0xa0, 21, 0x30, 19, 6, 9, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 9, 20, 0x31, 6, 0x1e, 4, 0, 65, 0, 66];
        for (const attributes of [[0xa0, 0], friendlyName]) {
            const key = await subtle.importKey('pkcs8', withAttributes(attributes), { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']);
            assert(equal(await subtle.exportKey('pkcs8', key), original), 'optional attributes do not change RSA components');
        }
        for (const attributes of [friendlyName.slice(0, -1), friendlyName.concat([0xa0, 0]), [0xa1, 0]])
            await rejects('DataError', () => subtle.importKey('pkcs8', withAttributes(attributes), { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']));
    });
    await test('PSS exact salt bounds and unsigned overflow', async () => {
        const keys = await pair('RSA-PSS');
        for (const saltLength of [0, 32, 33, 222]) {
            const signature = await subtle.sign({ name: 'RSA-PSS', saltLength }, keys.privateKey, message);
            assert(await subtle.verify({ name: 'RSA-PSS', saltLength }, keys.publicKey, signature, message), 'exact salt verifies');
            assert(!await subtle.verify({ name: 'RSA-PSS', saltLength: saltLength + 1 }, keys.publicKey, signature, message), 'different salt is rejected');
        }
        for (const saltLength of [223, 2147483647, 4294967295]) {
            await rejects('OperationError', () => subtle.sign({ name: 'RSA-PSS', saltLength }, keys.privateKey, message));
            assert(!await subtle.verify({ name: 'RSA-PSS', saltLength }, keys.publicKey, new Uint8Array(256), message), 'large salt must not become auto salt');
        }
    });
    await test('OAEP empty maximum label mismatch ciphertext checks and wrap unwrap', async () => {
        const keys = await pair('RSA-OAEP'); const parameters = { name: 'RSA-OAEP', label: new Uint8Array([0, 65, 0, 255]) };
        for (const length of [0, 190]) {
            const data = new Uint8Array(length); const encrypted = await subtle.encrypt(parameters, keys.publicKey, data);
            assert(equal(await subtle.decrypt(parameters, keys.privateKey, encrypted), data), 'OAEP maximum/empty plaintext');
            await rejects('OperationError', () => subtle.decrypt({ name: 'RSA-OAEP' }, keys.privateKey, encrypted));
        }
        await rejects('OperationError', () => subtle.encrypt(parameters, keys.publicKey, new Uint8Array(191)));
        for (const length of [0, 255, 257]) await rejects('OperationError', () => subtle.decrypt(parameters, keys.privateKey, new Uint8Array(length)));
        const secret = await subtle.importKey('raw', new Uint8Array(16).fill(13), 'AES-GCM', true, ['encrypt']);
        const wrapped = await subtle.wrapKey('raw', secret, keys.publicKey, parameters);
        const unwrapped = await subtle.unwrapKey('raw', wrapped, keys.privateKey, parameters, 'AES-GCM', true, ['encrypt']);
        assert(equal(await subtle.exportKey('raw', unwrapped), new Uint8Array(16).fill(13)), 'OAEP really wraps and unwraps AES key');
    });
    await test('usage extractability hash and key-type rejection', async () => {
        const keys = await pair('RSA-PSS');
        await rejects('InvalidAccessError', () => subtle.sign({ name: 'RSA-PSS', saltLength: 32 }, keys.publicKey, message));
        await rejects('InvalidAccessError', () => subtle.sign('RSASSA-PKCS1-v1_5', keys.privateKey, message));
        await rejects('SyntaxError', () => subtle.importKey('spki', bytes(source.spki), { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']));
        const jwk = await subtle.exportKey('jwk', keys.privateKey); jwk.ext = false;
        await rejects('DataError', () => subtle.importKey('jwk', jwk, { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']));
        jwk.ext = true; jwk.alg = 'PS512';
        await rejects('DataError', () => subtle.importKey('jwk', jwk, { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']));
    });
    await test('parallel operations on one private key keep independent state', async () => {
        const keys = await pair('RSA-PSS');
        await Promise.all([0, 1, 32, 33, 222, 5, 7, 11].map(async saltLength => {
            const signature = await subtle.sign({ name: 'RSA-PSS', saltLength }, keys.privateKey, message);
            assert(await subtle.verify({ name: 'RSA-PSS', saltLength }, keys.publicKey, signature, message), 'parallel sign verify');
        }));
    });
    await test('DedicatedWorker imports and performs actual RSA operations', async () => {
        const worker = new Worker('worker.js');
        try {
            const result = await new Promise((resolve, reject) => {
                const timer = setTimeout(() => reject(new Error('worker RSA deadline')), 30000);
                worker.onmessage = event => { clearTimeout(timer); resolve(event.data); };
                worker.onerror = event => { clearTimeout(timer); reject(new Error(event.message)); };
                worker.postMessage({ pkcs8: source.pkcs8, spki: source.spki, run });
            });
            assert(result.run === run && result.pass === true, result.error || 'worker RSA response');
        } finally { worker.terminate(); }
    });
    clearTimeout(timeout);
    const passed = results.filter(result => result.pass).length;
    console.log('RSA_RESULT ' + JSON.stringify({ run, passed, total: results.length, elapsed_ms: Math.round(performance.now() - started), results }));
    console.log('RSA_DONE run=' + run + ' passed=' + passed + ' total=' + results.length);
    output.textContent += '\nDONE ' + passed + '/' + results.length;
})().catch(error => console.log('RSA_FATAL ' + String(error.stack || error)));

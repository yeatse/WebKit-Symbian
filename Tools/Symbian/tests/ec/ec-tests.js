'use strict';
globalThis.runECContractSuite = async function (subtle, vectors, record) {
    const results = [];
    const bytes = hex => Uint8Array.from(hex.match(/../g) || [], value => parseInt(value, 16));
    const equal = (left, right) => {
        left = new Uint8Array(left); right = new Uint8Array(right);
        return left.length === right.length && left.every((value, index) => value === right[index]);
    };
    const assert = (value, message) => { if (!value) throw new Error(message); };
    async function rejects(name, operation) {
        let error;
        try { await operation(); } catch (caught) { error = caught; }
        assert(error && error.name === name, 'expected ' + name + ', received ' + (error ? error.name : 'success'));
    }
    async function test(name, operation) {
        let error = null;
        try { await operation(); } catch (caught) { error = String(caught.stack || caught); }
        const result = { name, pass: error === null, error };
        results.push(result);
        if (record) record(result);
    }
    async function pair(vector, name, extractable = true, privateUsages) {
        const algorithm = { name, namedCurve: vector.curve };
        return {
            privateKey: await subtle.importKey('pkcs8', bytes(vector.pkcs8), algorithm, extractable, privateUsages || (name === 'ECDSA' ? ['sign'] : ['deriveBits', 'deriveKey'])),
            publicKey: await subtle.importKey('spki', bytes(vector.spki), algorithm, true, name === 'ECDSA' ? ['verify'] : [])
        };
    }
    for (const vector of vectors.ecdsa) {
        await test('known ECDSA ' + vector.name, async () => {
            const keys = await pair(vector, 'ECDSA');
            const parameters = { name: 'ECDSA', hash: vector.hash };
            const message = bytes(vector.data), expected = bytes(vector.signature);
            assert(await subtle.verify(parameters, keys.publicKey, expected, message), 'independent raw r||s known signature');
            const signed = await subtle.sign(parameters, keys.privateKey, message);
            assert(signed.byteLength === expected.length && await subtle.verify(parameters, keys.publicKey, signed, message), 'fixed-width raw r||s signature');
            message[0] ^= 1;
            assert(!await subtle.verify(parameters, keys.publicKey, expected, message), 'changed message rejects');
            message[0] ^= 1;
            for (const invalid of [expected.slice(0, -1), new Uint8Array(expected.length + 1), new Uint8Array(expected.length)])
                assert(!await subtle.verify(parameters, keys.publicKey, invalid, message), 'invalid signature returns false');
        });
    }
    for (const vector of vectors.ecdh) {
        await test('known ECDH ' + vector.curve, async () => {
            const keys = await pair(vector, 'ECDH');
            const parameters = { name: 'ECDH', public: keys.publicKey }, expected = bytes(vector.secret);
            assert(equal(await subtle.deriveBits(parameters, keys.privateKey, expected.length * 8), expected), 'independent full-width shared secret');
            assert(equal(await subtle.deriveBits(parameters, keys.privateKey, null), expected), 'null length returns whole shared secret');
            assert((await subtle.deriveBits(parameters, keys.privateKey, 0)).byteLength === 0, 'zero bits returns empty');
            const partial = new Uint8Array(await subtle.deriveBits(parameters, keys.privateKey, 13));
            // Preserve the current shared WebKit ECDH contract: ceil to bytes,
            // without masking the final byte (WebCrypto issue 369).
            assert(equal(partial, expected.slice(0, 2)), 'non-byte bit count returns ceil-length byte prefix');
            await rejects('OperationError', () => subtle.deriveBits(parameters, keys.privateKey, expected.length * 8 + 1));
            const aes = await subtle.deriveKey(parameters, keys.privateKey, { name: 'AES-GCM', length: 128 }, true, ['encrypt']);
            assert(equal(await subtle.exportKey('raw', aes), expected.slice(0, 16)), 'deriveKey derives the expected AES key');
        });
    }
    for (const curve of ['P-256', 'P-384', 'P-521']) {
        const vector = vectors.ecdsa.find(item => item.curve === curve && item.hash === 'SHA-256');
        await test(curve + ' key formats', async () => {
            for (const name of ['ECDSA', 'ECDH']) {
                const keys = await pair(vector, name);
                assert(equal(await subtle.exportKey('spki', keys.publicKey), bytes(vector.spki)), 'independent SPKI encoding');
                assert(equal(await subtle.exportKey('pkcs8', keys.privateKey), bytes(vector.pkcs8)), 'independent PKCS8 encoding');
                const raw = await subtle.exportKey('raw', keys.publicKey);
                const rawKey = await subtle.importKey('raw', raw, { name, namedCurve: curve }, true, keys.publicKey.usages);
                assert(equal(await subtle.exportKey('raw', rawKey), raw), 'raw SEC1 roundtrip');
                for (const key of [keys.publicKey, keys.privateKey]) {
                    const jwk = await subtle.exportKey('jwk', key);
                    const imported = await subtle.importKey('jwk', jwk, { name, namedCurve: curve }, true, key.usages);
                    const again = await subtle.exportKey('jwk', imported);
                    assert(again.x === jwk.x && again.y === jwk.y && again.d === jwk.d && again.crv === curve, 'fixed-width JWK component roundtrip');
                }
            }
        });
        await test(curve + ' JWK validation', async () => {
            const keys = await pair(vector, 'ECDSA');
            const jwk = await subtle.exportKey('jwk', keys.privateKey);
            const algorithm = { name: 'ECDSA', namedCurve: curve };
            const width = curve === 'P-256' ? 32 : curve === 'P-384' ? 48 : 66;
            const zero = btoa(String.fromCharCode(...new Uint8Array(width))).replace(/=/g, '').replace(/\+/g, '-').replace(/\//g, '_');
            for (const patch of [{ x: zero }, { d: zero }, { d: 'AQ' }, { kty: 'RSA' }, { crv: 'P-192' }, { alg: 'RS256' }, { key_ops: ['verify'] }, { ext: false }, { use: 'enc' }])
                await rejects('DataError', () => subtle.importKey('jwk', Object.assign({}, jwk, patch), algorithm, true, ['sign']));
            const missing = Object.assign({}, jwk); delete missing.x;
            await rejects('DataError', () => subtle.importKey('jwk', missing, algorithm, true, ['sign']));
        });
        await test(curve + ' generation metadata', async () => {
            for (const name of ['ECDSA', 'ECDH']) {
                const usages = name === 'ECDSA' ? ['sign', 'verify'] : ['deriveBits', 'deriveKey'];
                const keys = await subtle.generateKey({ name, namedCurve: curve }, false, usages);
                assert(keys.publicKey.extractable && !keys.privateKey.extractable, 'generated key extractability');
                assert(keys.publicKey.type === 'public' && keys.privateKey.type === 'private', 'generated key types');
                assert(keys.privateKey.algorithm.name === name && keys.privateKey.algorithm.namedCurve === curve, 'generated key metadata');
                assert(keys.publicKey.usages.join() === (name === 'ECDSA' ? 'verify' : '') && keys.privateKey.usages.slice().sort().join() === (name === 'ECDSA' ? 'sign' : 'deriveBits,deriveKey'), 'generated usage split');
                await rejects('InvalidAccessError', () => subtle.exportKey('pkcs8', keys.privateKey));
                if (name === 'ECDSA') {
                    const message = new Uint8Array([1, 0, 255]);
                    const signature = await subtle.sign({ name, hash: 'SHA-256' }, keys.privateKey, message);
                    assert(await subtle.verify({ name, hash: 'SHA-256' }, keys.publicKey, signature, message), 'generated key signs');
                } else {
                    const secret = await subtle.deriveBits({ name, public: keys.publicKey }, keys.privateKey, null);
                    assert(secret.byteLength === (curve === 'P-256' ? 32 : curve === 'P-384' ? 48 : 66), 'generated key derives');
                }
            }
        });
    }
    const vector = vectors.ecdsa.find(item => item.curve === 'P-256' && item.hash === 'SHA-256');
    await test('usage and key type rejection', async () => {
        const keys = await pair(vector, 'ECDSA'), data = bytes(vector.data), algorithm = { name: 'ECDSA', namedCurve: 'P-256' };
        await rejects('SyntaxError', () => subtle.importKey('spki', bytes(vector.spki), algorithm, true, ['sign']));
        await rejects('SyntaxError', () => subtle.importKey('pkcs8', bytes(vector.pkcs8), algorithm, true, ['verify']));
        await rejects('SyntaxError', () => subtle.generateKey(algorithm, true, ['deriveBits']));
        await rejects('InvalidAccessError', () => subtle.sign({ name: 'ECDSA', hash: 'SHA-256' }, keys.publicKey, data));
        await rejects('InvalidAccessError', () => subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, keys.privateKey, bytes(vector.signature), data));
        await rejects('InvalidAccessError', () => subtle.exportKey('raw', keys.privateKey));
        const noBits = await pair(vectors.ecdh[0], 'ECDH', true, ['deriveKey']);
        await rejects('InvalidAccessError', () => subtle.deriveBits({ name: 'ECDH', public: noBits.publicKey }, noBits.privateKey, 128));
        await rejects('SyntaxError', () => subtle.importKey('spki', bytes(vectors.ecdh[0].spki), { name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits']));
    });
    await test('curve and algorithm rejection', async () => {
        const base = await pair(vectors.ecdh[0], 'ECDH');
        const other = await pair(vectors.ecdh[1], 'ECDH');
        const signing = await pair(vector, 'ECDSA');
        for (const key of [other.publicKey, signing.publicKey, base.privateKey])
            await rejects('InvalidAccessError', () => subtle.deriveBits({ name: 'ECDH', public: key }, base.privateKey, 128));
        await rejects('InvalidAccessError', () => subtle.deriveBits({ name: 'ECDH', public: base.publicKey }, base.publicKey, 128));
    });
    await test('DER format length and curve rejection', async () => {
        const original = bytes(vector.pkcs8), algorithm = { name: 'ECDSA', namedCurve: 'P-256' };
        const trailing = new Uint8Array(original.length + 1); trailing.set(original);
        const nonminimal = new Uint8Array(original.length + 1); nonminimal.set([0x30, 0x82, 0]); nonminimal.set(original.slice(2), 3);
        for (const invalid of [trailing, nonminimal, original.slice(0, -1), bytes(vector.spki)])
            await rejects('DataError', () => subtle.importKey('pkcs8', invalid, algorithm, true, ['sign']));
        for (const format of ['spki', 'pkcs8'])
            await rejects('DataError', () => subtle.importKey(format, bytes(vector[format]), { name: 'ECDSA', namedCurve: 'P-384' }, true, format === 'spki' ? ['verify'] : ['sign']));
    });
    await test('unsupported curve and hash rejection', async () => {
        await rejects('NotSupportedError', () => subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-192' }, true, ['sign']));
        const keys = await pair(vector, 'ECDSA');
        await rejects('NotSupportedError', () => subtle.sign({ name: 'ECDSA', hash: 'MD5' }, keys.privateKey, bytes(vector.data)));
        assert(!await subtle.verify({ name: 'ECDSA', hash: 'SHA-384' }, keys.publicKey, bytes(vector.signature), bytes(vector.data)), 'signature made with different hash returns false');
    });
    await test('same-key parallel operations', async () => {
        const keys = await pair(vector, 'ECDSA'), message = bytes(vector.data), algorithm = { name: 'ECDSA', hash: 'SHA-256' };
        const signatures = await Promise.all([0, 1, 2, 3].map(() => subtle.sign(algorithm, keys.privateKey, message)));
        const valid = await Promise.all(signatures.map(signature => subtle.verify(algorithm, keys.publicKey, signature, message)));
        assert(valid.every(Boolean), 'concurrent operations preserve immutable key state');
    });
    return results;
};

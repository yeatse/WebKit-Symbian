'use strict';
onmessage = async event => {
    const { run, ecdsa, ecdh } = event.data;
    const bytes = hex => Uint8Array.from(hex.match(/../g), value => parseInt(value, 16));
    try {
        const signing = await crypto.subtle.importKey('pkcs8', bytes(ecdsa.pkcs8), { name: 'ECDSA', namedCurve: ecdsa.curve }, false, ['sign']);
        const verifying = await crypto.subtle.importKey('spki', bytes(ecdsa.spki), { name: 'ECDSA', namedCurve: ecdsa.curve }, false, ['verify']);
        const parameters = { name: 'ECDSA', hash: ecdsa.hash }, message = bytes(ecdsa.data);
        if (!await crypto.subtle.verify(parameters, verifying, bytes(ecdsa.signature), message)) throw new Error('worker known ECDSA signature');
        const signature = await crypto.subtle.sign(parameters, signing, message);
        if (!await crypto.subtle.verify(parameters, verifying, signature, message)) throw new Error('worker ECDSA roundtrip');
        const base = await crypto.subtle.importKey('pkcs8', bytes(ecdh.pkcs8), { name: 'ECDH', namedCurve: ecdh.curve }, false, ['deriveBits']);
        const peer = await crypto.subtle.importKey('spki', bytes(ecdh.spki), { name: 'ECDH', namedCurve: ecdh.curve }, false, []);
        const secret = new Uint8Array(await crypto.subtle.deriveBits({ name: 'ECDH', public: peer }, base, null));
        const expected = bytes(ecdh.secret);
        if (secret.length !== expected.length || !secret.every((value, index) => value === expected[index])) throw new Error('worker known ECDH secret');
        postMessage({ run, pass: true });
    } catch (error) { postMessage({ run, pass: false, error: String(error.stack || error) }); }
};

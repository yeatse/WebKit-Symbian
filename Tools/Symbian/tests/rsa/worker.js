'use strict';
onmessage = async event => {
    const { pkcs8, spki, run } = event.data;
    const bytes = hex => Uint8Array.from(hex.match(/../g), value => parseInt(value, 16));
    try {
        const privateKey = await crypto.subtle.importKey('pkcs8', bytes(pkcs8), { name: 'RSA-PSS', hash: 'SHA-256' }, false, ['sign']);
        const publicKey = await crypto.subtle.importKey('spki', bytes(spki), { name: 'RSA-PSS', hash: 'SHA-256' }, false, ['verify']);
        const message = new Uint8Array([65, 0, 255]); const algorithm = { name: 'RSA-PSS', saltLength: 33 };
        const signature = await crypto.subtle.sign(algorithm, privateKey, message);
        if (!await crypto.subtle.verify(algorithm, publicKey, signature, message)) throw new Error('worker signature mismatch');
        postMessage({ run, pass: true });
    } catch (error) { postMessage({ run, pass: false, error: String(error.stack || error) }); }
};

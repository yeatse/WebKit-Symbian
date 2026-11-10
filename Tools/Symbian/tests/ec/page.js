'use strict';
(async function () {
    const run = new URL(location.href).searchParams.get('run');
    if (!/^[A-Za-z0-9_-]{8,64}$/.test(run || '')) throw new Error('Explicit unique run token required');
    const started = performance.now(), output = document.getElementById('output');
    const timeout = setTimeout(() => console.log('EC_TIMEOUT run=' + run), 180000);
    const record = result => {
        const line = 'EC_CASE run=' + run + ' ' + (result.pass ? 'PASS ' : 'FAIL ') + result.name + (result.error ? ' ' + result.error : '');
        console.log(line); output.textContent += '\n' + line;
    };
    try {
        const vectors = await (await fetch('vectors.json')).json();
        const results = await runECContractSuite(crypto.subtle, vectors, record);
        const workerResult = { name: 'DedicatedWorker EC operations', pass: false, error: null };
        try {
            await new Promise((resolve, reject) => {
                const worker = new Worker('worker.js');
                const timer = setTimeout(() => { worker.terminate(); reject(new Error('worker deadline')); }, 30000);
                worker.onmessage = event => {
                    clearTimeout(timer); worker.terminate();
                    if (event.data.run === run && event.data.pass === true) resolve();
                    else reject(new Error(event.data.error || 'worker identity or operation failure'));
                };
                worker.onerror = event => { clearTimeout(timer); worker.terminate(); reject(new Error(event.message)); };
                worker.postMessage({ run, ecdsa: vectors.ecdsa.find(vector => vector.curve === 'P-256' && vector.hash === 'SHA-256'), ecdh: vectors.ecdh[0] });
            });
            workerResult.pass = true;
        } catch (error) { workerResult.error = String(error.stack || error); }
        results.push(workerResult); record(workerResult);
        const passed = results.filter(result => result.pass).length;
        const report = { run, total: results.length, passed, elapsed_ms: performance.now() - started, results };
        console.log('EC_RESULT ' + JSON.stringify(report));
        console.log('EC_DONE run=' + run + ' passed=' + passed + ' total=' + results.length);
    } catch (error) { console.log('EC_FATAL run=' + run + ' ' + String(error.stack || error)); }
    finally { clearTimeout(timeout); }
})();

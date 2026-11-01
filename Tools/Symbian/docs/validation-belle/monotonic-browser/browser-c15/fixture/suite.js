/* Ordinary browser API acceptance; no native/test hooks or precision bypass. */
(async function () {
    'use strict';
    const run = new URL(location.href).searchParams.get('run');
    const status = document.getElementById('status');
    const timerCount = 8;
    const requestedDelayMS = 40;
    const subsecondBoundMS = 750;
    const requiredSubsecondYields = 6;
    const totalBoundMS = 6000;
    function check(value, reason) {
        if (!value) throw new Error(reason);
    }
    function emit(kind, value) {
        const line = 'SYMBIAN CLOCK ' + run + ' ' + kind + ' ' + JSON.stringify(value);
        status.textContent = line;
        console.log(line);
    }
    function monotonic(samples) {
        return samples.every((value, index) => Number.isFinite(value) && value >= 0
            && (!index || value >= samples[index - 1]));
    }
    try {
        check(/^[a-f0-9]{32}$/.test(run || ''), 'unique 32-hex run required');
        check(typeof performance.now === 'function', 'performance.now unavailable');
        emit('READY', { run, timerCount, requestedDelayMS });
        const syncSamples = [];
        for (let i = 0; i < 64; ++i)
            syncSamples.push(performance.now());
        const timerSamples = [performance.now()];
        for (let i = 0; i < timerCount; ++i) {
            await new Promise(resolve => setTimeout(resolve, requestedDelayMS));
            timerSamples.push(performance.now());
        }
        const deltas = timerSamples.slice(1).map((value, index) => value - timerSamples[index]);
        const totalMS = timerSamples[timerCount] - timerSamples[0];
        const subsecondYields = deltas.filter(value => value > 0 && value < subsecondBoundMS).length;
        check(monotonic(syncSamples.concat(timerSamples)), 'non-finite, negative or backward monotonic clock');
        check(deltas.every(value => value > 0), 'no positive clock progress across timer yield');
        check(subsecondYields >= requiredSubsecondYields, '40ms yields repeatedly rounded toward a second');
        check(totalMS > 0 && totalMS < totalBoundMS, 'bounded timer sequence exceeded 6s');
        emit('DONE PASS', { run, timerCount, requestedDelayMS, syncSamples, timerSamples,
            deltas, totalMS, subsecondYields });
    } catch (error) {
        emit('DONE FAIL', { run, error: String(error && error.message || error) });
    }
})();

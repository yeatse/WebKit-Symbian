/* The runner must separately prove actual native pressure and measured JSCell
 * bytes. Object counts or a synthetic JavaScript event alone cannot pass. */
(async function () {
    'use strict';
    const token = new URL(location.href).searchParams.get('run');
    const started = performance.now();
    const count = 1572864;
    const bufferBytes = 8 * 1024 * 1024;
    const output = document.getElementById('status');
    const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
    function check(condition, message) {
        if (!condition) throw new Error(message);
    }
    function deadline() {
        if (performance.now() - started > 110000) throw new Error('bounded test deadline');
    }
    function log(kind, value) {
        const line = 'MEMORY_PRESSURE_' + kind + ': ' + JSON.stringify({ token, ...value });
        output.textContent = line;
        console.log(line);
    }
    function Cell(next, value) {
        this.next = next; this.value = value;
        this.a = value ^ 0x1234; this.b = value + 7;
        this.c = value - 9; this.d = value ^ 0x5678;
    }
    try {
        check(/^[a-f0-9]{32}$/.test(token || ''), 'unique run token required');
        // Keep roots externally reachable. Never call a GC testing API, dispatch
        // a pressure event or infer actual heap usage from this allocation count.
        globalThis.pressureLiveRoot = null;
        for (let base = 0; base < count; base += 16384) {
            deadline();
            for (let i = base; i < Math.min(base + 16384, count); ++i)
                globalThis.pressureLiveRoot = new Cell(globalThis.pressureLiveRoot, i);
            await pause(0);
        }
        const buffer = globalThis.pressureLiveBytes = new Uint8Array(bufferBytes);
        for (let i = 0; i < buffer.length; i += 4096) {
            buffer[i] = (i / 4096) % 251 + 1;
            buffer[i + 4095] = buffer[i] ^ 0xA5;
        }
        const canvas = document.querySelector('canvas');
        const context = canvas.getContext('2d');
        function verify() {
            let node = globalThis.pressureLiveRoot;
            for (let value = count - 1; value >= 0; --value) {
                if (!(value & 16383)) deadline();
                check(node && node.value === value && node.a === (value ^ 0x1234)
                    && node.b === value + 7 && node.c === value - 9
                    && node.d === (value ^ 0x5678), 'live cell chain/value corrupt');
                node = node.next;
            }
            check(node === null, 'live chain length differs');
            check(globalThis.pressureLiveBytes === buffer && buffer.length === bufferBytes, 'buffer identity/length');
            for (let i = 0; i < buffer.length; i += 4096) {
                const value = (i / 4096) % 251 + 1;
                check(buffer[i] === value && buffer[i + 4095] === (value ^ 0xA5), 'live buffer page corrupt');
            }
            context.fillStyle = '#2371c5';
            context.fillRect(0, 0, 32, 32);
            const pixel = context.getImageData(16, 16, 1, 1).data;
            check(pixel[0] === 35 && pixel[1] === 113 && pixel[2] === 197 && pixel[3] === 255, 'canvas after cache relief');
        }
        verify();
        log('READY', { count, bufferBytes, requiredMeasuredCellBytes: 80 * 1024 * 1024 });
        const readyAt = performance.now();
        let verifications = 0;
        do {
            await pause(4000);
            verify();
            ++verifications;
            log('VERIFIED', { verifications, elapsedMS: performance.now() - readyAt });
        } while (performance.now() - readyAt < 40000);
        // Roots deliberately remain live through the final observation; releasing
        // them here could make a late pressure callback appear to preserve them.
        log('PASS', { verifications, count, bufferBytes, elapsedMS: performance.now() - started });
    } catch (error) {
        log('FAIL', { error: String(error && error.message || error) });
    }
})();

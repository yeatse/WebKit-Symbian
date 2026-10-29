'use strict';
// The server supplies only observations. Expected cookie policies live here.
const results = [];
const prefix = 'ss_' + config.run + '_';
const baseKeys = ['strict', 'lax', 'none', 'default', 'mixed', 'unknown', 'duplicate', 'session', 'httpOnly', 'badnone'];
const all = ['strict', 'lax', 'none', 'default', 'mixed', 'unknown', 'duplicate', 'session', 'httpOnly'];
const lax = ['lax', 'none', 'default', 'unknown', 'duplicate'];
const none = ['none'];
const out = document.getElementById('output');
function assert(ok, detail) { if (!ok) throw new Error(detail); }
function url(origin, path, parameters = {}) {
    const result = new URL(path, origin);
    result.searchParams.set('run', config.run);
    result.searchParams.set('phase', config.phase);
    for (const [key, value] of Object.entries(parameters)) result.searchParams.set(key, value);
    return result.href;
}
function keys(value) {
    return value.split(';').map(part => part.trim().split('=')[0]).filter(key => key.startsWith(prefix)).map(key => key.slice(prefix.length));
}
function policy(value, expected, namespace = '', candidates = baseKeys) {
    const present = keys(value);
    for (const key of candidates) {
        const name = namespace + key;
        assert(present.includes(name) === expected.includes(key), name + ' expected ' + expected.includes(key) + '; actual ' + JSON.stringify(present));
    }
}
async function test(name, callback) {
    const result = { name, phase: config.phase, status: 'PASS', detail: '' };
    try { await callback(); } catch (error) { result.status = 'FAIL'; result.detail = String(error); }
    results.push(result);
    const text = result.status + ': ' + name + (result.detail ? ' ' + result.detail : '');
    out.textContent += text + '\n'; console.log(text);
}
async function request(target, options = {}) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 10000);
    try {
        const response = await fetch(target, { credentials: 'include', ...options, signal: controller.signal });
        assert(response.ok, 'HTTP ' + response.status);
        return await response.json();
    } finally { clearTimeout(timer); }
}
async function save() {
    if (!results.length) return;
    const response = await fetch(url(location.origin, '/record'), {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(results)
    });
    assert(response.ok, 'result recording failed');
}
async function navigate(origin, path, parameters = {}, post = false) {
    await save();
    const target = url(origin, path, parameters);
    if (!post) { location.href = target; return; }
    const form = document.createElement('form');
    form.method = 'POST'; form.action = target;
    const input = document.createElement('input'); input.name = 'fixture'; input.value = 'unsafe-method';
    form.appendChild(input); document.body.appendChild(form); form.submit();
}
function setDOM(namespace) {
    for (const [key, attribute] of Object.entries({ strict: '; SameSite=Strict', lax: '; SameSite=Lax', none: '; SameSite=None', default: '' }))
        document.cookie = prefix + namespace + key + '=1; Path=/; Max-Age=3600; Secure' + attribute;
    document.cookie = prefix + namespace + 'badnone=1; Path=/; Max-Age=3600; SameSite=None';
}
function socket(origin) {
    return new Promise((resolve, reject) => {
        const ws = new WebSocket(url(origin.replace(/^https/, 'wss'), '/ws'));
        const timer = setTimeout(() => { ws.close(); reject(new Error('WS timeout')); }, 10000);
        ws.onerror = () => { clearTimeout(timer); reject(new Error('WS error')); };
        ws.onmessage = event => {
            clearTimeout(timer);
            try { resolve(JSON.parse(event.data)); } catch (error) { reject(error); }
            ws.close();
        };
    });
}
function frame(path = '/frame') {
    return new Promise((resolve, reject) => {
        const iframe = document.createElement('iframe');
        const finish = (error, value) => { clearTimeout(timer); window.removeEventListener('message', message); iframe.remove(); error ? reject(error) : resolve(value); };
        const message = event => {
            if (event.origin === config.b && event.source === iframe.contentWindow && event.data.run === config.run)
                finish(null, event.data);
        };
        const timer = setTimeout(() => finish(new Error('iframe timeout')), 10000);
        window.addEventListener('message', message);
        iframe.src = url(config.b, path); document.body.appendChild(iframe);
    });
}
async function main() {
    if (config.phase === 'seed-a') return navigate(config.b, '/seed-b');
    if (config.phase === 'seed-b') return navigate(config.a, '/main');
    if (config.phase === 'frame') {
        const before = document.cookie;
        setDOM('frame_');
        parent.postMessage({ run: config.run, observed: config.observed, before, after: document.cookie }, config.a);
        return;
    }
    if (config.phase === 'nested-parent') {
        const child = document.createElement('iframe');
        window.addEventListener('message', event => {
            if (event.source === child.contentWindow && event.origin === config.a && event.data.run === config.run)
                parent.postMessage(event.data, config.a);
        });
        child.src = url(config.a, '/nested-child'); document.body.appendChild(child);
        return;
    }
    if (config.phase === 'nested-child') {
        const before = document.cookie;
        setDOM('nested_');
        parent.postMessage({ run: config.run, observed: config.observed, before, after: document.cookie,
            fetched: await request(url(config.a, '/echo')), socket: await socket(config.a) }, config.b);
        return;
    }
    if (config.phase === 'main') {
        await test('top-level DOM reads Strict/Lax/default; HttpOnly hidden; None without Secure rejected', () => policy(document.cookie, all.filter(key => key !== 'httpOnly')));
        setDOM('dom_');
        await test('top-level DOM writes preserve SameSite; None without Secure rejected', () => policy(document.cookie, ['strict', 'lax', 'none', 'default'], 'dom_', ['strict', 'lax', 'none', 'default', 'badnone']));
        await test('same-site fetch sends every policy', async () => policy((await request(url(config.a, '/echo'))).cookie, all));
        await test('cross-port same-site control sends every policy', async () => policy((await request(url(config.sameSiteOtherPort, '/echo'))).cookie, all));
        await test('cross-site fetch GET sends only None', async () => policy((await request(url(config.b, '/echo'))).cookie, none));
        await test('cross-site fetch POST sends only None', async () => policy((await request(url(config.b, '/echo'), { method: 'POST', body: 'body' })).cookie, none));
        await test('cross-site iframe request and DOM read/send restrictions', async () => {
            const value = await frame(); policy(value.observed.cookie, none); policy(value.before, none);
            policy(value.after, none, 'frame_', ['strict', 'lax', 'none', 'default', 'badnone']);
        });
        await test('same-site WebSocket includes Strict/Lax', async () => policy((await socket(config.a)).cookie, all));
        await test('cross-site WebSocket includes only None', async () => policy((await socket(config.b)).cookie, none));
        await test('fetch redirect A to B to A keeps cross-site taint', async () => {
            const back = url(config.b, '/redirect', { to: url(config.a, '/echo') });
            policy((await request(url(config.a, '/redirect', { to: back }))).cookie, none);
        });
        await test('cross-site response may set None; Strict/Lax/default deferred rejection verified on top-level B', async () => {
            await request(url(config.b, '/set-response'));
            policy((await request(url(config.b, '/echo'))).cookie, none, 'response_');
        });
        await test('A to B to A iframe ancestry stays cross-site for DOM, fetch and WebSocket', async () => {
            const value = await frame('/nested-parent');
            policy(value.observed.cookie, none); policy(value.before, none);
            policy(value.fetched.cookie, none); policy(value.socket.cookie, none);
            policy(value.after, none, 'nested_', ['strict', 'lax', 'none', 'default', 'badnone']);
            policy(document.cookie, none, 'nested_', ['strict', 'lax', 'none', 'default', 'badnone']);
        });
        return navigate(config.b, '/nav', { step: 'get' });
    }
    if (config.phase === 'nav') {
        const step = config.extra;
        if (step === 'get') {
            await test('cross-site top-level GET permits Lax/default/None, rejects Strict', () => policy(config.observed.cookie, lax));
            await test('top-level DOM can read Strict after cross-site navigation', () => policy(document.cookie, all.filter(key => key !== 'httpOnly')));
            await test('cross-site iframe DOM writes really rejected, not merely hidden', () => policy(document.cookie, none, 'frame_', ['strict', 'lax', 'none', 'default', 'badnone']));
            await test('cross-site subresource Set-Cookie Strict/Lax/default really rejected', () => policy(document.cookie, none, 'response_'));
            return navigate(config.a, '/nav', { step: 'post' }, true);
        }
        if (step === 'post') {
            await test('fresh default cookie excluded on top-level cross-site POST (no two-minute exception)', () => policy(config.observed.cookie, none));
            await test('DOM-created cookie policies enforced for cross-site POST', () => policy(config.observed.cookie, none, 'dom_', ['strict', 'lax', 'none', 'default', 'badnone']));
            return navigate(config.a, '/nav', { step: 'same' });
        }
        if (step === 'same') {
            await test('same-site top-level GET permits Strict', () => policy(config.observed.cookie, all));
            return navigate(config.b, '/redirect', { code: '307', to: url(config.b, '/nav', { step: 'post307' }) }, true);
        }
        if (step === 'post307') {
            await test('307 preserves cross-site POST restrictions', () => { assert(config.observed.method === 'POST', 'POST not preserved'); policy(config.observed.cookie, none); });
            return navigate(config.a, '/nav', { step: 'chain-start' });
        }
        if (step === 'chain-start')
            return navigate(config.b, '/redirect', { to: url(config.a, '/nav', { step: 'chain-end' }) });
        if (step === 'chain-end') {
            await test('top-level A to B to A GET chain still excludes Strict', () => policy(config.observed.cookie, lax));
            return navigate(config.httpA, '/nav', { step: 'scheme-post-source' });
        }
        if (step === 'scheme-post-source')
            return navigate(config.a, '/nav', { step: 'scheme-post' }, true);
        if (step === 'scheme-post') {
            await test('HTTP to HTTPS same-host POST is schemefully cross-site', () => policy(config.observed.cookie, none));
            return navigate(config.httpA, '/nav', { step: 'scheme-get-source' });
        }
        if (step === 'scheme-get-source')
            return navigate(config.a, '/nav', { step: 'scheme-get' });
        if (step === 'scheme-get') {
            await test('HTTP to HTTPS same-host GET permits Lax, excludes Strict', () => policy(config.observed.cookie, lax));
            return navigate(config.a, '/done');
        }
        throw new Error('unknown navigation step: ' + step);
    }
    if (config.phase === 'resume') {
        await test('restart A preserves policy and persistent cookies, removes session cookie', () => policy(config.observed.cookie, all.filter(key => key !== 'session')));
        await test('restart preserves DOM-created cookie policy', () => policy(document.cookie, ['strict', 'lax', 'none', 'default'], 'dom_', ['strict', 'lax', 'none', 'default', 'badnone']));
        await test('restart cross-site request still filters Strict/Lax/default', async () => policy((await request(url(config.b, '/echo'))).cookie, none));
        return navigate(config.b, '/resume-b');
    }
    if (config.phase === 'resume-b') {
        await test('restart B cross-site top-level request still filters Strict', () => policy(config.observed.cookie, lax));
        await test('restart B DOM persistent state and no session cookie', () => policy(document.cookie, all.filter(key => key !== 'session' && key !== 'httpOnly')));
        return navigate(config.a, '/done');
    }
    if (config.phase === 'done') {
        const recorded = await request(url(config.a, '/summary'));
        const passed = recorded.filter(value => value.status === 'PASS').length;
        const expectedCount = recorded.some(value => value.phase === 'resume') ? 28 : 23;
        const complete = passed === expectedCount && recorded.length === expectedCount;
        const text = (complete ? 'PASS' : 'FAIL') + ': cookie-samesite ' + passed + '/' + expectedCount + ' (' + recorded.length + ' outcomes)';
        document.title = text; out.textContent = text + '\n' + JSON.stringify(recorded, null, 2);
        console.log('COOKIE_FINAL_URL: ' + location.href);
        console.log(text);
        return;
    }
    throw new Error('unknown phase: ' + config.phase);
}
main().catch(async error => {
    console.log('FAIL: cookie-samesite harness ' + error);
    document.title = 'FAIL: cookie-samesite harness';
    out.textContent += String(error);
    results.push({ name: 'harness', phase: config.phase, status: 'FAIL', detail: String(error) });
    try { await save(); } catch (_) { }
});

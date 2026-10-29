"use strict";
(async () => {
    const query = new URLSearchParams(location.search);
    const run = query.get("run");
    const phase = query.get("phase") || "write";
    const output = document.getElementById("output");
    const cases = [];
    const resources = [];
    const prefix = "belle-storage:" + run + ":";
    const persisted = prefix + "persisted";
    const session = prefix + "session";
    const value = "Belle \u00e9 \u4e2d\u6587 \ud83d\ude80 " + run;
    let finished = false;
    const started = Date.now();
    function log(text) { output.textContent += text + "\n"; console.log(text); }
    function assert(condition, reason) { if (!condition) throw new Error(reason); }
    const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
    function bounded(promise, description, ms = 5000) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error(description + " timeout")), ms);
            promise.then(value => { clearTimeout(timer); resolve(value); }, error => { clearTimeout(timer); reject(error); });
        });
    }
    async function check(name, body) {
        try { await body(); cases.push({ name, status: "PASS" }); log("PASS " + name); }
        catch (error) { cases.push({ name, status: "FAIL", error: String(error) }); log("FAIL " + name + ": " + error); }
    }
    async function finish(error) {
        if (finished) return;
        finished = true;
        clearTimeout(watchdog);
        for (const close of resources) { try { close(); } catch (_) { } }
        if (error) cases.push({ name: "suite completion", status: "FAIL", error: String(error) });
        const report = { suite: "belle-storage", run, phase, origin: location.origin,
            status: cases.length && cases.every(item => item.status === "PASS") ? "PASS" : "FAIL",
            cases, elapsed_ms: Date.now() - started };
        try {
            const response = await bounded(fetch("/report", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(report) }), "report", 4000);
            assert(response.ok, "report rejected: " + response.status);
            log("BELLE_STORAGE_" + report.status + " " + phase + " " + run + " " + cases.filter(item => item.status === "PASS").length + "/" + cases.length);
        } catch (error) { log("BELLE_STORAGE_FAIL report " + error); }
    }
    const watchdog = setTimeout(() => finish("45 second page deadline exceeded"), 45000);
    function rpcPeer(origin) {
        return new Promise((resolve, reject) => {
            const frame = document.createElement("iframe");
            const pending = new Map();
            let serial = 0;
            const timeout = setTimeout(() => reject(new Error("iframe ready timeout: " + origin)), 5000);
            function receive(event) {
                if (event.source !== frame.contentWindow || event.origin !== origin || event.data.token !== run) return;
                if (event.data.ready) {
                    clearTimeout(timeout);
                    resolve((action, extra = {}) => bounded(new Promise((resolve, reject) => {
                        const id = ++serial;
                        pending.set(id, { resolve, reject });
                        frame.contentWindow.postMessage(Object.assign({ token: run, id, action }, extra), origin);
                    }), "iframe " + action));
                } else if (pending.has(event.data.id)) {
                    const request = pending.get(event.data.id); pending.delete(event.data.id);
                    if (event.data.error) request.reject(new Error(event.data.error)); else request.resolve(event.data.result);
                }
            }
            addEventListener("message", receive);
            resources.push(() => { clearTimeout(timeout); removeEventListener("message", receive); frame.remove(); });
            frame.src = origin + "/peer.html?run=" + encodeURIComponent(run) + "&parent=" + encodeURIComponent(location.origin);
            document.body.appendChild(frame);
        });
    }
    function rpcWorker() {
        return new Promise((resolve, reject) => {
            const worker = new Worker("worker.js");
            const pending = new Map();
            let serial = 0;
            const timeout = setTimeout(() => reject(new Error("worker ready timeout")), 5000);
            resources.push(() => { clearTimeout(timeout); worker.terminate(); });
            worker.onerror = event => { reject(new Error(event.message)); };
            worker.onmessage = event => {
                if (event.data.ready) {
                    clearTimeout(timeout);
                    resolve((action, extra = {}, transfer = []) => bounded(new Promise((resolve, reject) => {
                        const id = ++serial;
                        pending.set(id, { resolve, reject });
                        worker.postMessage(Object.assign({ id, action }, extra), transfer);
                    }), "worker " + action));
                } else if (pending.has(event.data.id)) {
                    const request = pending.get(event.data.id); pending.delete(event.data.id);
                    if (event.data.error) request.reject(new Error(event.data.error)); else request.resolve(event.data);
                }
            };
        });
    }
    function message(channel) {
        return bounded(new Promise(resolve => channel.addEventListener("message", event => resolve(event.data), { once: true })), "BroadcastChannel delivery");
    }
    try {
        assert(/^[A-Za-z0-9_-]{1,80}$/.test(run || ""), "run token required");
        assert(["write", "read", "erased"].includes(phase), "unknown phase");
        if (phase === "read") {
            await check("localStorage survives browser restart", () => assert(localStorage.getItem(persisted) === value, "persistent value missing or different"));
            await check("sessionStorage does not survive browser restart", () => assert(sessionStorage.getItem(session) === null, "old page session leaked"));
            await check("persistent removal", () => { localStorage.removeItem(persisted); assert(localStorage.getItem(persisted) === null, "remove failed"); });
        } else if (phase === "erased") {
            await check("removal survives browser restart", () => assert(localStorage.getItem(persisted) === null, "deleted value returned"));
        } else {
            await check("localStorage string operations", () => {
                const key = prefix + "operations";
                assert(localStorage.getItem(key) === null, "fresh run already has data");
                localStorage.setItem(key, value); assert(localStorage.getItem(key) === value, "UTF-16 roundtrip");
                localStorage.setItem(key, 314); assert(localStorage.getItem(key) === "314", "string conversion/overwrite");
                assert(Array.from({ length: localStorage.length }, (_, i) => localStorage.key(i)).includes(key), "key enumeration");
                localStorage.removeItem(key); assert(localStorage.getItem(key) === null, "remove");
            });
            await check("localStorage quota rejection is atomic", () => {
                const key = prefix + "quota";
                try {
                    localStorage.setItem(key, "old");
                    let name;
                    // Latin-1 uses one byte per character in WebCore's quota
                    // accounting; exceed 5 MiB even with that representation.
                    try { localStorage.setItem(key, "q".repeat(6 * 1024 * 1024)); } catch (error) { name = error.name; }
                    assert(name === "QuotaExceededError", "6 MiB ASCII value must exceed configured 5 MiB quota");
                    assert(localStorage.getItem(key) === "old", "failed write replaced old value");
                } finally { localStorage.removeItem(key); }
            });
            await check("sessionStorage string operations", () => { sessionStorage.setItem(session, value); assert(sessionStorage.getItem(session) === value, "session readback"); });
            const config = await (await fetch("/config.json")).json();
            const same = await rpcPeer(location.origin);
            const other = await rpcPeer(config.otherOrigin);
            await check("same origin localStorage sharing", async () => {
                const key = prefix + "shared";
                localStorage.setItem(key, "parent"); assert(await same("storage-get", { key }) === "parent", "iframe cannot read parent");
                await same("storage-set", { key, value: "peer" }); assert(localStorage.getItem(key) === "peer", "parent cannot read iframe");
                localStorage.removeItem(key);
            });
            await check("different origin localStorage isolation", async () => {
                const key = prefix + "isolated";
                localStorage.setItem(key, "primary"); assert(await other("storage-get", { key }) === null, "cross-origin value leaked");
                await other("storage-set", { key, value: "secondary" }); assert(localStorage.getItem(key) === "primary", "other origin overwrote primary");
                await other("storage-remove", { key }); localStorage.removeItem(key);
            });
            await check("sessionStorage origin isolation", async () => {
                assert(await same("session-get", { key: session }) === value, "same frame tree session not shared");
                assert(await other("session-get", { key: session }) === null, "cross-origin session leaked");
                await other("session-set", { key: session, value: "other" }); assert(sessionStorage.getItem(session) === value, "other session overwrote parent");
            });
            await check("storage event and no self notification", async () => {
                const key = prefix + "event";
                await same("events"); let own = 0;
                const listener = event => { if (event.key === key) ++own; };
                addEventListener("storage", listener);
                try {
                    localStorage.setItem(key, "new"); await delay(300);
                    const events = (await same("events")).filter(event => event.key === key);
                    assert(events.length === 1 && events[0].oldValue === null && events[0].newValue === "new" && events[0].local && events[0].url === location.href, "storage event payload");
                    assert(own === 0, "storage sender got own event");
                } finally { removeEventListener("storage", listener); localStorage.removeItem(key); }
            });
            const name = prefix + "channel";
            const sender = new BroadcastChannel(name);
            const receiver = new BroadcastChannel(name);
            resources.push(() => sender.close(), () => receiver.close());
            await check("BroadcastChannel structured clone and no self echo", async () => {
                let own = 0; sender.onmessage = () => ++own;
                const received = message(receiver);
                const payload = { array: [1, "two"], bytes: new Uint8Array([3, 4]), map: new Map([["a", 5]]) };
                sender.postMessage(payload); payload.array[0] = 99;
                const result = await received;
                assert(result.array[0] === 1 && result.array[1] === "two" && result.bytes[1] === 4 && result.map.get("a") === 5, "cloned payload");
                await delay(100); assert(own === 0, "source received itself"); sender.onmessage = null;
            });
            await check("BroadcastChannel rejects uncloneable values", () => {
                let errorName; try { sender.postMessage(() => 1); } catch (error) { errorName = error.name; }
                assert(errorName === "DataCloneError", "uncloneable value was accepted");
            });
            await check("BroadcastChannel same origin iframe transport", async () => {
                await same("bc-open", { name });
                sender.postMessage({ from: "parent" }); await delay(300);
                const received = await same("bc-messages"); assert(received.length === 1 && received[0].from === "parent", "iframe delivery");
                const reply = message(sender); await same("bc-send", { value: { from: "iframe" } }); assert((await reply).from === "iframe", "iframe reply");
            });
            await check("BroadcastChannel different origin isolation", async () => {
                await other("bc-open", { name });
                let unexpected = 0;
                const listener = event => { if (event.data.from === "other-origin") ++unexpected; };
                sender.addEventListener("message", listener);
                try {
                    sender.postMessage({ from: "primary-only" }); await other("bc-send", { value: { from: "other-origin" } });
                    await delay(350); assert((await other("bc-messages")).length === 0 && unexpected === 0, "cross-origin channel leaked");
                } finally { sender.removeEventListener("message", listener); await other("bc-close"); }
            });
            await check("BroadcastChannel close suppresses queued delivery", async () => {
                const closed = new BroadcastChannel(name); let received = 0;
                closed.onmessage = () => ++received;
                sender.postMessage("queued"); closed.close(); sender.postMessage("after-close");
                let errorName; try { closed.postMessage("invalid"); } catch (error) { errorName = error.name; }
                await delay(300); assert(received === 0 && errorName === "InvalidStateError", "closed channel remained active");
                await same("bc-close");
            });
            const worker = await rpcWorker();
            await check("DedicatedWorker transfer and storage exposure", async () => {
                const buffer = new Uint8Array([7, 8, 9]).buffer;
                const pending = worker("transfer", { value: buffer }, [buffer]); assert(buffer.byteLength === 0, "sender buffer was not detached");
                const result = await pending;
                assert(result.result.bytes.join(",") === "7,8,9" && new Uint8Array(result.buffer)[0] === 73, "real worker transfer roundtrip");
                assert(result.result.localStorage === "undefined" && result.result.sessionStorage === "undefined", "DOM storage exposed in worker");
            });
            await check("BroadcastChannel document worker roundtrip", async () => {
                const ready = message(sender);
                await worker("bc-open", { name });
                assert((await ready).workerReady === true, "worker BC registration handshake");
                const reply = message(sender); sender.postMessage({ workerInput: [17, "ok"] });
                const result = await reply;
                assert(result.worker === true && result.received === 1 && result.echo.workerInput[0] === 17 && result.echo.workerInput[1] === "ok", "worker BC roundtrip");
            });
            await check("BroadcastChannel worker close stops messages", async () => {
                await worker("bc-close"); sender.postMessage({ afterClose: true }); await delay(300);
                assert((await worker("bc-state")).result === 1, "closed worker channel received a message");
                assert((await worker("bc-closed-send")).result === "InvalidStateError", "closed worker sender accepted message");
            });
            await check("persistent value staged for restart", () => {
                assert(localStorage.getItem(persisted) === null, "run token reused");
                localStorage.setItem(persisted, value); assert(localStorage.getItem(persisted) === value, "persistent write");
            });
        }
        await finish();
    } catch (error) { await finish(error); }
})();

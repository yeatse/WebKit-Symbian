"use strict";
(async () => {
    const query = new URLSearchParams(location.search);
    const run = query.get("run");
    const phase = query.get("phase") || "write";
    const databaseName = "belle-backends:" + run;
    const output = document.getElementById("output");
    const cases = [];
    const cleanup = [];
    const started = Date.now();
    let db;
    let finished = false;
    const deferred = () => { let resolve; const promise = new Promise(done => { resolve = done; }); return { promise, resolve }; };
    const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
    const assert = (condition, reason) => { if (!condition) throw new Error(reason); };
    const log = text => { output.textContent += text + "\n"; console.log(text); };
    const request = operation => new Promise((resolve, reject) => { operation.onsuccess = () => resolve(operation.result); operation.onerror = () => reject(operation.error); });
    const completed = transaction => new Promise((resolve, reject) => { transaction.oncomplete = resolve; transaction.onabort = () => reject(transaction.error || new Error("transaction aborted")); });
    const bounded = (promise, label) => new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error(label + " timeout")), 6000);
        promise.then(value => { clearTimeout(timer); resolve(value); }, error => { clearTimeout(timer); reject(error); });
    });
    async function check(name, body) {
        try { await bounded(Promise.resolve().then(body), name); cases.push({ name, status: "PASS" }); log("PASS " + name); }
        catch (error) { cases.push({ name, status: "FAIL", error: String(error) }); log("FAIL " + name + ": " + error); }
    }
    async function finish(error) {
        if (finished) return;
        finished = true; clearTimeout(watchdog);
        if (db) db.close();
        for (const action of cleanup) { try { action(); } catch (_) { } }
        if (error) cases.push({ name: "suite completion", status: "FAIL", error: String(error) });
        const report = { suite: "belle-backends", run, phase, origin: location.origin,
            status: cases.length && cases.every(row => row.status === "PASS") ? "PASS" : "FAIL", cases, elapsed_ms: Date.now() - started };
        try {
            const response = await bounded(fetch("/report", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(report) }), "report");
            assert(response.ok, "report rejected");
            log("BELLE_BACKENDS_" + report.status + " " + phase + " " + run + " " + cases.filter(row => row.status === "PASS").length + "/" + cases.length);
        } catch (error) { log("BELLE_BACKENDS_FAIL report " + error); }
    }
    const watchdog = setTimeout(() => finish("55 second page deadline exceeded"), 55000);
    async function get(key) {
        const tx = db.transaction("items", "readonly"); const done = completed(tx);
        const value = await request(tx.objectStore("items").get(key)); await done; return value;
    }
    async function worker() {
        const instance = new Worker("worker.js");
        cleanup.push(() => instance.terminate());
        const ready = deferred(); const pending = new Map(); let serial = 0;
        instance.onmessage = event => {
            if (event.data.ready) { ready.resolve(); return; }
            const item = pending.get(event.data.id);
            if (!item) return;
            pending.delete(event.data.id);
            if (event.data.error) item.reject(new Error(event.data.error)); else item.resolve(event.data.result);
        };
        await bounded(ready.promise, "worker ready");
        return { instance, call(action, name) { return bounded(new Promise((resolve, reject) => {
            const id = ++serial; pending.set(id, { resolve, reject }); instance.postMessage({ id, action, name });
        }), "worker " + action); } };
    }
    async function peer(origin) {
        const frame = document.createElement("iframe"); const ready = deferred(); const pending = new Map(); let serial = 0;
        function receive(event) {
            if (event.origin !== origin || event.source !== frame.contentWindow || event.data.token !== run) return;
            if (event.data.ready) { ready.resolve(); return; }
            const item = pending.get(event.data.id);
            if (!item) return;
            pending.delete(event.data.id);
            if (event.data.error) item.reject(new Error(event.data.error)); else item.resolve(event.data.result);
        }
        addEventListener("message", receive);
        cleanup.push(() => { removeEventListener("message", receive); frame.remove(); });
        frame.src = origin + "/peer.html?run=" + encodeURIComponent(run) + "&parent=" + encodeURIComponent(location.origin);
        document.body.appendChild(frame); await bounded(ready.promise, "peer ready");
        return (action, name) => bounded(new Promise((resolve, reject) => {
            const id = ++serial; pending.set(id, { resolve, reject }); frame.contentWindow.postMessage({ id, token: run, action, name }, origin);
        }), "peer " + action);
    }
    try {
        assert(/^[A-Za-z0-9_-]{1,80}$/.test(run || ""), "fresh run token required");
        assert(["write", "read", "erased"].includes(phase), "unknown phase");
        if (phase === "read") {
            await check("IndexedDB survives browser restart", async () => {
                db = await request(indexedDB.open(databaseName, 2));
                assert((await get(1)).value === run && (await get(4)).value === "worker-value", "persistent committed records missing");
            });
            await check("IndexedDB persistent deletion", async () => { if (db) db.close(); db = null; await request(indexedDB.deleteDatabase(databaseName)); });
        } else if (phase === "erased") {
            await check("IndexedDB deletion survives browser restart", async () => {
                let created = false; const opening = indexedDB.open(databaseName);
                opening.onupgradeneeded = event => { created = event.oldVersion === 0; };
                db = await request(opening); assert(created && db.objectStoreNames.length === 0, "deleted database returned");
                db.close(); db = null; await request(indexedDB.deleteDatabase(databaseName));
            });
        } else {
            await check("IndexedDB schema and committed records", async () => {
                let created = false; const opening = indexedDB.open(databaseName, 1);
                opening.onupgradeneeded = event => {
                    created = event.oldVersion === 0;
                    opening.result.createObjectStore("items", { keyPath: "id" }).createIndex("byTag", "tag", { unique: true });
                };
                db = await request(opening); assert(created, "database already existed");
                const tx = db.transaction("items", "readwrite"); const done = completed(tx); const store = tx.objectStore("items");
                store.add({ id: 3, tag: "three", value: "third" }); store.add({ id: 1, tag: "one", value: run }); store.add({ id: 2, tag: "two", value: "second" });
                await done; assert((await get(1)).value === run, "committed readback");
            });
            await check("IndexedDB index and cursor ordering", async () => {
                const tx = db.transaction("items", "readonly"); const done = completed(tx); const store = tx.objectStore("items");
                const indexed = request(store.index("byTag").get("two"));
                const ids = []; const cursor = store.openCursor();
                cursor.onsuccess = () => { if (cursor.result) { ids.push(cursor.result.key); cursor.result.continue(); } };
                assert((await indexed).id === 2, "index lookup"); await done; assert(ids.join(",") === "1,2,3", "cursor order");
            });
            await check("IndexedDB explicit abort preserves data", async () => {
                const tx = db.transaction("items", "readwrite"); const aborted = new Promise(resolve => { tx.onabort = resolve; });
                tx.objectStore("items").put({ id: 1, tag: "changed", value: "must roll back" }); tx.abort();
                await aborted; assert((await get(1)).value === run, "aborted overwrite persisted");
            });
            await check("IndexedDB constraint abort preserves data", async () => {
                const tx = db.transaction("items", "readwrite"); const aborted = new Promise(resolve => { tx.onabort = () => resolve(tx.error); });
                tx.objectStore("items").add({ id: 5, tag: "one", value: "duplicate unique tag" });
                assert((await aborted).name === "ConstraintError", "wrong transaction error"); assert(await get(5) === undefined, "constraint failure committed");
            });
            await check("IndexedDB close and reopen", async () => { db.close(); db = await request(indexedDB.open(databaseName, 1)); assert((await get(2)).value === "second", "reopened data"); });
            await check("IndexedDB versionchange and blocked handoff", async () => {
                let changed = false, blocked = false;
                db.onversionchange = event => { changed = event.oldVersion === 1 && event.newVersion === 2; };
                const opening = indexedDB.open(databaseName, 2);
                opening.onblocked = () => { blocked = true; db.close(); };
                opening.onupgradeneeded = () => opening.result.createObjectStore("version2");
                db = await request(opening); assert(changed && blocked && db.objectStoreNames.contains("version2"), "upgrade coordination");
            });
            const background = await worker();
            await check("IndexedDB DedicatedWorker read write", async () => {
                assert(await background.call("idb", databaseName) === run, "worker cannot read page data");
                assert((await get(4)).value === "worker-value", "page cannot read committed worker data");
            });
            const config = await (await fetch("/config.json")).json(); const other = await peer(config.otherOrigin);
            await check("IndexedDB origin isolation", async () => { assert(await other("idb", databaseName), "foreign origin saw database"); assert((await get(1)).value === run, "foreign write escaped origin"); });
            await check("Web Locks exclusive ordering", async () => {
                const name = databaseName + ":exclusive", acquired = deferred(), release = deferred(); const order = [];
                const first = navigator.locks.request(name, async () => { order.push(1); acquired.resolve(); await release.promise; order.push(2); });
                await acquired.promise;
                const second = navigator.locks.request(name, () => { order.push(3); });
                try { await delay(150); assert(order.join(",") === "1", "exclusive waiter ran early"); }
                finally { release.resolve(); }
                await Promise.all([first, second]); assert(order.join(",") === "1,2,3", "lock queue order");
            });
            await check("Web Locks shared holders and exclusive waiter", async () => {
                const name = databaseName + ":shared", ready1 = deferred(), ready2 = deferred(), release1 = deferred(), release2 = deferred();
                const first = navigator.locks.request(name, { mode: "shared" }, async () => { ready1.resolve(); await release1.promise; });
                const second = navigator.locks.request(name, { mode: "shared" }, async () => { ready2.resolve(); await release2.promise; });
                await Promise.all([ready1.promise, ready2.promise]); let acquired = false;
                const exclusive = navigator.locks.request(name, () => { acquired = true; });
                try { release1.resolve(); await first; await delay(100); assert(!acquired, "exclusive ignored second shared holder"); }
                finally { release1.resolve(); release2.resolve(); }
                await Promise.all([second, exclusive]); assert(acquired, "exclusive waiter not resumed");
            });
            await check("Web Locks ifAvailable and abort", async () => {
                const name = databaseName + ":abort", ready = deferred(), release = deferred();
                const held = navigator.locks.request(name, async () => { ready.resolve(); await release.promise; }); await ready.promise;
                try {
                    assert(await navigator.locks.request(name, { ifAvailable: true }, lock => lock === null), "ifAvailable granted held lock");
                    const controller = new AbortController(); let entered = false;
                    const queued = navigator.locks.request(name, { signal: controller.signal }, () => { entered = true; }).then(() => "resolved", error => error.name);
                    controller.abort(); assert(await queued === "AbortError" && !entered, "abort did not cancel pending request");
                } finally { release.resolve(); await held; }
            });
            await check("Web Locks DedicatedWorker lifetime", async () => {
                const name = databaseName + ":worker";
                assert(await background.call("hold-lock", name) === "held", "worker lock not acquired");
                let entered = false; const queued = navigator.locks.request(name, () => { entered = true; });
                await delay(150); assert(!entered, "page ignored worker holder");
                background.instance.terminate(); await queued; assert(entered, "terminated worker did not release lock");
            });
            await check("Web Locks origin isolation", async () => {
                const name = databaseName + ":origin", ready = deferred(), release = deferred();
                const held = navigator.locks.request(name, async () => { ready.resolve(); await release.promise; }); await ready.promise;
                try { assert(await other("lock", name) === "acquired", "different origin shared lock namespace"); }
                finally { release.resolve(); await held; }
            });
            await check("IndexedDB persistent value staged", async () => { assert((await get(1)).value === run && (await get(4)).value === "worker-value", "records not ready for restart"); db.close(); db = null; });
        }
        await finish();
    } catch (error) { await finish(error); }
})();

"use strict";
let releaseLock;
let lockCompletion;
const request = operation => new Promise((resolve, reject) => { operation.onsuccess = () => resolve(operation.result); operation.onerror = () => reject(operation.error); });
const completed = transaction => new Promise((resolve, reject) => { transaction.oncomplete = resolve; transaction.onabort = () => reject(transaction.error || new Error("transaction aborted")); });
onmessage = async event => {
    const { id, action, name } = event.data;
    try {
        let result;
        if (action === "idb") {
            const db = await request(indexedDB.open(name, 2));
            try {
                const read = db.transaction("items", "readonly");
                const readDone = completed(read);
                const previous = await request(read.objectStore("items").get(1));
                await readDone;
                const write = db.transaction("items", "readwrite");
                const writeDone = completed(write);
                write.objectStore("items").put({ id: 4, tag: "worker", value: "worker-value" });
                await writeDone;
                result = previous.value;
            } finally { db.close(); }
        } else if (action === "hold-lock") {
            let acquired;
            const ready = new Promise(resolve => { acquired = resolve; });
            lockCompletion = navigator.locks.request(name, async () => {
                acquired();
                await new Promise(resolve => { releaseLock = resolve; });
            });
            await ready;
            result = "held";
        } else if (action === "release-lock") {
            releaseLock(); await lockCompletion; result = "released";
        } else throw new Error("unknown action");
        postMessage({ id, result });
    } catch (error) { postMessage({ id, error: String(error) }); }
};
postMessage({ ready: true });

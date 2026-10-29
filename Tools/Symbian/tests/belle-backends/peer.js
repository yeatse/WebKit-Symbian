"use strict";
const query = new URLSearchParams(location.search);
const origin = query.get("parent");
const token = query.get("run");
const request = operation => new Promise((resolve, reject) => { operation.onsuccess = () => resolve(operation.result); operation.onerror = () => reject(operation.error); });
addEventListener("message", async event => {
    if (event.source !== parent || event.origin !== origin || event.data.token !== token) return;
    const { id, action, name } = event.data;
    try {
        let result;
        if (action === "idb") {
            const opening = indexedDB.open(name, 1);
            opening.onupgradeneeded = () => opening.result.createObjectStore("items", { keyPath: "id" });
            const db = await request(opening);
            try {
                const tx = db.transaction("items", "readwrite");
                const done = new Promise((resolve, reject) => { tx.oncomplete = resolve; tx.onabort = () => reject(tx.error); });
                const read = tx.objectStore("items").get(1);
                read.onsuccess = () => { result = read.result === undefined; tx.objectStore("items").put({ id: 1, value: "foreign" }); };
                await done;
            } finally { db.close(); }
            await request(indexedDB.deleteDatabase(name));
        } else if (action === "lock") result = await navigator.locks.request(name, () => "acquired");
        else throw new Error("unknown peer action");
        parent.postMessage({ token, id, result }, origin);
    } catch (error) { parent.postMessage({ token, id, error: String(error) }, origin); }
});
parent.postMessage({ token, ready: true }, origin);

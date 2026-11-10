"use strict";
const options = new URLSearchParams(location.search);
const parentOrigin = options.get("parent");
const token = options.get("run");
let channel;
let channelMessages = [];
let storageEvents = [];
addEventListener("storage", event => {
    if (event.key && event.key.startsWith("belle-storage:" + token + ":"))
        storageEvents.push({ key: event.key, oldValue: event.oldValue, newValue: event.newValue,
            url: event.url, local: event.storageArea === localStorage });
});
addEventListener("message", async event => {
    if (event.source !== parent || event.origin !== parentOrigin || event.data.token !== token)
        return;
    const { id, action, key, value, name } = event.data;
    try {
        let result;
        switch (action) {
        case "storage-get": result = localStorage.getItem(key); break;
        case "storage-set": localStorage.setItem(key, value); result = true; break;
        case "storage-remove": localStorage.removeItem(key); result = true; break;
        case "session-get": result = sessionStorage.getItem(key); break;
        case "session-set": sessionStorage.setItem(key, value); result = true; break;
        case "events": result = storageEvents.splice(0); break;
        case "bc-open":
            channel = new BroadcastChannel(name);
            channel.onmessage = message => channelMessages.push(message.data);
            result = true;
            break;
        case "bc-send": channel.postMessage(value); result = true; break;
        case "bc-messages": result = channelMessages.splice(0); break;
        case "bc-close": channel.close(); result = true; break;
        default: throw new Error("unknown peer command");
        }
        parent.postMessage({ token, id, result }, parentOrigin);
    } catch (error) {
        parent.postMessage({ token, id, error: String(error) }, parentOrigin);
    }
});
parent.postMessage({ token, ready: true }, parentOrigin);

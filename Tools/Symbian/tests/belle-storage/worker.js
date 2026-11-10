"use strict";
let channel;
let received = 0;
onmessage = event => {
    const { id, action, name, value } = event.data;
    try {
        let result;
        switch (action) {
        case "transfer": {
            const view = new Uint8Array(value);
            result = { bytes: Array.from(view), localStorage: typeof localStorage,
                sessionStorage: typeof sessionStorage };
            view[0] = 73;
            postMessage({ id, result, buffer: value }, [value]);
            return;
        }
        case "bc-open":
            channel = new BroadcastChannel(name);
            channel.onmessage = message => {
                ++received;
                channel.postMessage({ worker: true, echo: message.data, received });
            };
            // The ordinary Worker message pipe and BroadcastChannel can be
            // registered independently. A BC message provides the barrier.
            channel.postMessage({ workerReady: true });
            result = true;
            break;
        case "bc-close": channel.close(); result = true; break;
        case "bc-state": result = received; break;
        case "bc-closed-send":
            try { channel.postMessage("closed"); result = "no exception"; }
            catch (error) { result = error.name; }
            break;
        default: throw new Error("unknown worker command");
        }
        postMessage({ id, result });
    } catch (error) {
        postMessage({ id, error: String(error) });
    }
};
postMessage({ ready: true });

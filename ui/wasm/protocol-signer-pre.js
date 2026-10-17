// Emscripten browser event bridge for the native QML presentation shell.
// The browser protocol service is imported from the generated WASM output
// directory; events are polled asynchronously on Qt's GUI thread. No Promise
// blocks the WebAssembly stack and no user private key crosses this boundary.
(function () {
    "use strict";
    if (typeof window === "undefined") return;
    const events = [];
    let client = null, initializedBase = "", epoch = 0, owner = "";
    let inFlight = false;
    function report(payload) {
        // No unbounded event-buffer growth if the Qt GUI is paused.
        events.push(payload);
        if (events.length > 40) events.splice(0, events.length - 40);
    }
    async function createClient(base) {
        if (client && initializedBase === base) return client;
        if (client) client.signedOut();
        client = null;
        const module = await import('./browser_protocol_signer.mjs');
        const current = module.createBrowserProtocolSigner({
            browser: window,
            snapchainBase: base,
            onState(status) {
                report({kind: "signer", ...status});
            },
        });
        client = current;
        initializedBase = base;
        return current;
    }
    function start(action, fid, base, payloadString = "") {
        // Runtime identities are authoritative in Qt, never JS localStorage.
        // If the identity changes while a wallet is prompting, pending
        // requests are invalidated and cannot submit protocol messages.
        if (action === "signed-out") {
            ++epoch;
            inFlight = false;
            owner = "";
            client?.signedOut();
            report({kind: "signer", phase: "signed-out", fid: "",
                busy: false, ready: false, error: ""});
            return;
        }
        if (owner && owner !== fid) {
            // Identity changes must invalidate any pending custody prompt.
            ++epoch;
            client?.signedOut();
            inFlight = false;
        }
        if (inFlight) {
            // The first signed submission may already be at Snapchain.
            // Do not start or cancel a second write and lose its outcome.
            report({kind: action === "restore" || action === "provision"
                || action === "revoke" ? "signer" : "write",
                phase: "failed", fid, busy: true, ready: false,
                error: "Another signer operation is in progress."});
            return;
        }
        owner = fid;
        const thisEpoch = ++epoch;
        inFlight = true;
        (async () => {
            try {
                const current = await createClient(base);
                if (epoch !== thisEpoch || owner !== fid) return;
                if (action === "restore") {
                    await current.restore(fid);
                } else if (action === "provision") {
                    await current.provision(fid);
                } else if (action === "revoke") {
                    await current.revoke(fid);
                } else {
                    report({kind: "write", phase: "submitting", fid});
                    const payload = JSON.parse(payloadString);
                    let hash;
                    switch(action) {
                    case "cast":
                        hash = await current.publishCast(fid,payload); break;
                    case "thread":
                        hash = await current.publishThread(fid,payload); break;
                    case "remove":
                        hash = await current.castRemove(fid,payload.target); break;
                    case "reaction":
                        hash = await current.reaction(fid,payload.target,
                            payload.reaction,payload.active); break;
                    case "follow":
                        hash = await current.follow(fid,payload.targetFid,payload.active); break;
                    default:
                        throw new Error("Unsupported browser signer action");
                    }
                    if (epoch === thisEpoch && owner === fid)
                        report({kind: "write", phase: "confirmed", fid, hash,
                            operation: action});
                }
            } catch(error) {
                if (epoch !== thisEpoch || owner !== fid) return;
                report({kind: action === "restore" || action === "provision"
                       || action === "revoke" ? "signer" : "write",
                    phase: action === "restore" ? "unavailable"
                         : action === "provision" ? "authorization-required"
                         : action === "revoke" ? "failed" : "failed",
                    busy:false,ready:false,fid,
                    error:String(error?.message || error).slice(0,700)});
            } finally {
                if (epoch === thisEpoch) inFlight = false;
            }
        })();
    }
    window.__genycasterBrowserSigning = Object.freeze({start,events});
}());

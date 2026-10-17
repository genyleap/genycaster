// Emscripten browser event bridge for the native QML presentation shell.
// The browser protocol service is imported from the generated WASM output
// directory; events are polled asynchronously on Qt's GUI thread. No Promise
// blocks the WebAssembly stack and no user private key crosses this boundary.
(function () {
    "use strict";
    if (typeof window === "undefined") return;
    // emrun can present the bind-all address (0.0.0.0) as the page origin.
    // This is NOT a potentially trustworthy origin. The browser then hides
    // SubtleCrypto, so Farcaster key storage must fail closed. Normalize only
    // this explicit *local* listen address; never rewrite remote HTTP hosts.
    if (window.location?.protocol === "http:"
        && window.location.hostname === "0.0.0.0") {
        try {
            const safeUrl = new URL(window.location.href);
            safeUrl.hostname = "127.0.0.1";
            window.location.replace(safeUrl.href);
            return;
        } catch (error) {
            console.warn("Genycaster: cannot switch 0.0.0.0 to a secure loopback origin", error);
        }
    }
    // A read-only diagnostic, with no wallet or signer secrets.
    window.__genycasterSignerEnvironment = Object.freeze({
        get origin() { return window.location?.origin || ""; },
        get secureContext() { return window.isSecureContext === true; },
        get webCrypto() { return typeof window.crypto?.subtle?.sign === "function"; },
        get indexedDB() {
            try { return typeof window.indexedDB?.open === "function"; }
            catch { return false; }
        },
    });
    const events = [];
    let client = null, initializedBase = "", epoch = 0, owner = "";
    let inFlight = false;
    let recoveryController = null;
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
    async function authorizeRecoveryInSetup(current, fid, thisEpoch) {
        const controller = new AbortController();
        recoveryController = controller;
        const { promptForFarcasterRecovery } =
            await import('./recovery-phrase-dialog.mjs');
        if (controller.signal.aborted ||
            epoch !== thisEpoch || owner !== fid) return;
        const phrase = await promptForFarcasterRecovery(
            window, controller.signal);
        if (epoch !== thisEpoch || owner !== fid) return;
        await current.provisionFromRecovery(fid, phrase);
    }

    function start(action, fid, base, payloadString = "") {
        // Runtime identities are authoritative in Qt, never JS localStorage.
        // If the identity changes while a wallet is prompting, pending
        // requests are invalidated and cannot submit protocol messages.
        if (action === "signed-out") {
            ++epoch;
            recoveryController?.abort();
            recoveryController = null;
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
            recoveryController?.abort();
            recoveryController = null;
            client?.signedOut();
            inFlight = false;
        }
        if (inFlight) {
            // The first signed submission may already be at Snapchain.
            // Do not start or cancel a second write and lose its outcome.
            report({kind: action === "restore" || action === "restore-for-setup"
                || action === "provision" || action === "revoke" ? "signer" : "write",
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
                if (action === "recover-account") {
                    // This event is accepted by Qt only for the current one-shot
                    // recovery request. No phrase or custody address is sent to Qt.
                    const {requestId} = JSON.parse(payloadString);
                    if(typeof requestId !== 'string' ||
                       !/^[0-9a-f-]{36}$/i.test(requestId))
                        throw new Error('Invalid recovery request');
                    const {promptForFarcasterRecovery} =
                        await import('./recovery-phrase-dialog.mjs');
                    const controller=new AbortController();
                    recoveryController=controller;
                    if(epoch!==thisEpoch || owner!==fid) return;
                    const phrase=await promptForFarcasterRecovery(window,controller.signal);
                    if(epoch!==thisEpoch || owner!==fid) return;
                    const recoveredFid=await current.recoverAccountFromPhrase(phrase,
                        ()=>epoch===thisEpoch && owner===fid && !controller.signal.aborted);
                    if(epoch!==thisEpoch || owner!==fid) return;
                    report({kind:'setup-account',phase:'ready',fid:recoveredFid,requestId});
                } else if (action === "restore" || action === "restore-for-setup") {
                    const existing = await current.restore(fid);
                    if (action === "restore-for-setup" && !existing) {
                        // Never collect a phrase on app reload or a restored session.
                        // This command is scoped to a verified, explicit setup login.
                        if (epoch !== thisEpoch || owner !== fid) return;
                        await authorizeRecoveryInSetup(current, fid, thisEpoch);
                    }
                } else if (action === "provision") {
                    // Explicit retry in Setup or Account Settings.
                    await authorizeRecoveryInSetup(current, fid, thisEpoch);
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
                const cancelled = error?.code === 'user-cancelled' ||
                    error?.message === 'Recovery import cancelled';
                if (action === "recover-account") {
                    const requestId=JSON.parse(payloadString).requestId;
                    report({kind:'setup-account',phase:cancelled?'cancelled':'failed',
                        requestId,fid:'',error:cancelled?'':String(error?.message||error).slice(0,350)});
                    return;
                }
                report({kind: action === "restore" || action === "restore-for-setup"
                       || action === "provision" || action === "revoke" ? "signer" : "write",
                    phase: cancelled ? "missing"
                         : action === "restore" ? "unavailable"
                         : action === "provision" || action === "restore-for-setup"
                           ? "authorization-required"
                         : action === "revoke" ? "failed" : "failed",
                    busy:false,ready:false,fid,
                    error:cancelled ? '' : String(error?.message || error).slice(0,700)});
            } finally {
                if (epoch === thisEpoch) {
                    recoveryController = null;
                    inFlight = false;
                }
            }
        })();
    }
    window.__genycasterBrowserSigning = Object.freeze({start,events});
}());

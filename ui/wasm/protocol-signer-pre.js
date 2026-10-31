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
    let rememberedWrite = Promise.resolve();
    const signOutKey = 'genycaster.device-session.signed-out';
    const signedOutLocally=()=>{
        try{return window.localStorage?.getItem(signOutKey)==='1';}
        catch{return false;}
    };
    const deviceSession = async () =>
        (await import('./browser_device_session.mjs')).createDeviceSession(window);
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
                // The registered Farcaster signer can independently prove
                // the account owner for private Genycaster database access.
                // This is automatic after verified signer restoration.
                if(status.phase==="ready" && status.ready && status.fid) {
                    const fid=String(status.fid);
                    const currentEpoch=epoch;
                    if(owner===fid) {
                        void import('./browser_account_session.mjs')
                            .then(module=>module.ensureBrowserAccountSession({
                                fid,origin:window.location.origin,
                            }))
                            .then(result=>{
                                if(epoch===currentEpoch && owner===fid)
                                    report({kind:'account-session',phase:'ready',
                                        fid,csrfToken:result.csrf_token});
                            })
                            .catch(error=>{
                                if(epoch===currentEpoch && owner===fid)
                                    report({kind:'account-session',phase:'failed',
                                        fid,error:String(error?.message||error).slice(0,140)});
                            });
                    }
                }
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
        await promptForFarcasterRecovery(window,controller.signal,
            phrase=>current.provisionFromRecovery(fid,phrase));
    }

    function start(action, fid, base, payloadString = "") {
        // Runtime identities are authoritative in Qt, never JS localStorage.
        // If the identity changes while a wallet is prompting, pending
        // requests are invalidated and cannot submit protocol messages.
        if (action === "forget-account") {
            try{window.localStorage?.setItem(signOutKey,'1');}
            catch{/* IndexedDB erase still runs below */}
            ++epoch;
            recoveryController?.abort();recoveryController=null;
            owner="";inFlight=false;client?.signedOut();
            // Serialize with an in-flight remembered identity write to
            // prevent a late async write resurrecting an explicitly logged-out user.
            void rememberedWrite.catch(()=>{}).then(async()=>{
                try{await (await deviceSession()).clear();}
                catch(error){console.warn('[Genycaster] Local sign-out cleanup failed',error);}
            });
            return;
        }
        if (action === "resume-account") {
            if(signedOutLocally())return;
            const {requestId}=JSON.parse(payloadString);
            if(!/^[0-9a-f-]{36}$/i.test(requestId||""))return;
            const thisEpoch=++epoch;
            void (async()=>{
                try{
                    await rememberedWrite.catch(()=>{});
                    const account=await (await deviceSession()).restore();
                    if(epoch===thisEpoch && account && !owner)
                        report({kind:'setup-account',phase:'ready',fid:account,
                            requestId,localDevice:true});
                }catch(error){
                    if(epoch===thisEpoch)
                        report({kind:'device-session',phase:'unavailable',
                            error:String(error?.message||error).slice(0,200)});
                }
            })();
            return;
        }
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
            // Never cancel an in-flight signed write. A Setup recovery is
            // different from an ordinary signer action: Qt waits for a
            // one-shot setup-account event, not a "write" error. Otherwise
            // the tap appears to do absolutely nothing.
            if (action === "recover-account") {
                let requestId = "";
                try { requestId = JSON.parse(payloadString).requestId || ""; }
                catch { /* handled as an invalid request below */ }
                report({kind: "setup-account", phase: "failed",
                    requestId, fid: "",
                    error: "Another signer operation is in progress. Please retry."});
            } else {
                report({kind: action === "restore" || action === "restore-for-setup"
                    || action === "provision" || action === "revoke" ? "signer" : "write",
                    phase: "failed", fid, busy: true, ready: false,
                    error: "Another signer operation is in progress."});
            }
            return;
        }
        owner = fid;
        const thisEpoch = ++epoch;
        inFlight = true;
        (async () => {
            try {
                if (epoch !== thisEpoch || owner !== fid) return;
                // Present the explicit local recovery UI before constructing
                // the network signer. Failure to initialize Snapchain must not
                // reject a click before any recovery words were entered.
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
                    const {openProofGatewayOrigin}=JSON.parse(payloadString);
                    let verifiedOpenProof=null;
                    const recoveredFid=await promptForFarcasterRecovery(
                        window,controller.signal,async phrase=>{
                            if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted)
                                throw new Error("Recovery was cancelled");
                            const current=await createClient(base);
                            if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted)
                                throw new Error("Recovery was cancelled");
                            return current.recoverAccountFromPhrase(
                                phrase,()=>epoch===thisEpoch && owner===fid && !controller.signal.aborted,
                                async (verifiedFid,custody)=>{
                                    if(!openProofGatewayOrigin) return;
                                    const {verifyRecoveredFarcasterIdentity}=
                                        await import('./openproof_recovery.mjs');
                                    verifiedOpenProof=await verifyRecoveredFarcasterIdentity({
                                        fid:verifiedFid,custody,gatewayOrigin:openProofGatewayOrigin,
                                        signal:controller.signal,
                                    });
                                });
                        });
                    if(epoch!==thisEpoch || owner!==fid) return;
                    rememberedWrite=rememberedWrite.catch(()=>{}).then(async()=>{
                        const device=await deviceSession();
                        await device.remember(recoveredFid);
                        if(epoch===thisEpoch && owner===fid)
                            try{window.localStorage?.removeItem(signOutKey);}catch{}
                    });
                    await rememberedWrite.catch(error=>
                        console.warn('[Genycaster] Device session could not be saved',error));
                    if(epoch!==thisEpoch || owner!==fid) return;
                    // Native Qt may mark this as canonical OpenProof identity
                    // only when the verified subject and issuer accompany the
                    // exact one-shot recovery request. Missing proof means a
                    // functional, explicitly local Farcaster session.
                    report({kind:'setup-account',phase:'ready',fid:recoveredFid,requestId,
                        identityIssuer:verifiedOpenProof?.issuer || '',
                        identitySubject:verifiedOpenProof?.subject || '',
                        accountTicket:verifiedOpenProof?.accountTicket || '',
                        accountExpiresSeconds:verifiedOpenProof?.expiresSeconds || 0});
                } else {
                    const current=await createClient(base);
                    if(epoch!==thisEpoch || owner!==fid)return;
                    if (action === "restore" || action === "restore-for-setup") {
                    if(action==="restore" && payloadString){
                        const options=JSON.parse(payloadString);
                        if(options.rememberLocal===true){
                            rememberedWrite=rememberedWrite.catch(()=>{}).then(async()=>{
                                if(epoch!==thisEpoch || owner!==fid)return;
                                await (await deviceSession()).remember(fid);
                                if(epoch===thisEpoch && owner===fid)
                                    try{window.localStorage?.removeItem(signOutKey);}catch{}
                            });
                            await rememberedWrite.catch(error=>
                                console.warn('[Genycaster] Device session could not be saved',error));
                        }
                    }
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
                    case "profile":
                        hash = await current.updateProfile(fid,payload); break;
                    default:
                        throw new Error("Unsupported browser signer action");
                    }
                    if (epoch === thisEpoch && owner === fid)
                        report({kind: "write", phase: "confirmed", fid, hash,
                            operation: action,
                            targetCastId: action==='remove'
                                ? String(payload.target?.hash||'') : ''});
                    }
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
                    operation: action,
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
    // Browser image picker + upload stays in JS; the QML bridge receives
    // bounded metadata/progress only, never a browser File or private key.
    const mediaEvents=[];
    let mediaEpoch=0, mediaAbort=null;
    const mediaReport=entry=>{
        mediaEvents.push(entry);
        if(mediaEvents.length>32)mediaEvents.splice(0,mediaEvents.length-32);
    };
    function cancelMedia(){
        ++mediaEpoch;
        mediaAbort?.abort();
        mediaAbort=null;
    }
    function pickMedia(fid,afterIndex) {
        if(typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid))return;
        const picker=document.createElement('input');
        picker.type='file';picker.accept='image/jpeg,image/png,image/webp,image/gif,image/avif';
        picker.multiple=false;picker.style.display='none';
        picker.addEventListener('change',()=>{
            const file=picker.files?.[0];
            picker.remove();
            if(!file)return;
            cancelMedia();
            const current=mediaEpoch;
            const controller=new AbortController();
            mediaAbort=controller;
            mediaReport({kind:'selected',fid,afterIndex,
                fileName:String(file.name).slice(0,160),
                contentType:String(file.type).slice(0,60),sizeBytes:file.size});
            // Use a bounded, low-resolution, sanitized JPEG thumbnail.
            // Never pass original File/blob: URLs or full media bytes to QML.
            (async()=>{
                try{
                    const bitmap=await createImageBitmap(file);
                    const scale=Math.min(1,96/Math.max(bitmap.width,bitmap.height));
                    const canvas=document.createElement('canvas');
                    canvas.width=Math.max(1,Math.round(bitmap.width*scale));
                    canvas.height=Math.max(1,Math.round(bitmap.height*scale));
                    const ctx=canvas.getContext('2d',{alpha:false});
                    ctx.drawImage(bitmap,0,0,canvas.width,canvas.height);
                    bitmap.close?.();
                    let preview=canvas.toDataURL('image/jpeg',0.48);
                    if(preview.length>6200){
                        canvas.width=Math.max(1,Math.floor(canvas.width*0.64));
                        canvas.height=Math.max(1,Math.floor(canvas.height*0.64));
                        // Use the source file again; rescaling an already
                        // resized canvas would clear its pixels.
                        const smaller=await createImageBitmap(file);
                        canvas.getContext('2d',{alpha:false}).drawImage(
                            smaller,0,0,canvas.width,canvas.height);
                        smaller.close?.();
                        preview=canvas.toDataURL('image/jpeg',0.40);
                    }
                    if(current===mediaEpoch && preview.length<=6200)
                        mediaReport({kind:'preview',fid,previewUrl:preview});
                }catch{ /* Preview support must never block upload. */ }
            })();
            (async()=>{
                try {
                    const {uploadBrowserImage}=await import('./browser_media_upload.mjs');
                    const result=await uploadBrowserImage({
                        file,fid,signal:controller.signal,
                        onProgress:(phase,transferred,total)=>{
                            if(current===mediaEpoch)
                                mediaReport({kind:'progress',fid,phase,transferred,total});
                        },
                    });
                    if(current===mediaEpoch)
                        mediaReport({kind:'completed',fid,
                            assetUrl:result.assetUrl,
                            contentType:result.contentType,
                            fileName:result.fileName,
                            managementToken:result.managementToken,
                            sizeBytes:result.sizeBytes});
                }catch(error){
                    if(current===mediaEpoch)
                        mediaReport({kind:'failed',fid,
                            error:String(error?.message||error).slice(0,240)});
                }finally{
                    if(current===mediaEpoch)mediaAbort=null;
                }
            })();
        },{once:true});
        document.body.appendChild(picker);
        // Must remain synchronous inside the actual Qt button tap: Safari
        // rejects file picker prompts after an await/dynamic import.
        picker.click();
    }
    const pendingRemovals=new Set();
    async function removeManagedMedia(fid,managementToken){
        if(typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid)
            ||typeof managementToken!=='string'||managementToken.length>4096
            ||pendingRemovals.has(managementToken))return;
        pendingRemovals.add(managementToken);
        try{
            const {deleteBrowserAsset}=await import('./browser_media_upload.mjs');
            await deleteBrowserAsset({fid,managementToken});
            mediaReport({kind:'cleanup-completed',fid,managementToken});
        }catch(error){
            mediaReport({kind:'cleanup-failed',fid,managementToken,
                error:String(error?.message||error).slice(0,150)});
        }finally{
            pendingRemovals.delete(managementToken);
        }
    }
    window.__genycasterBrowserMedia=Object.freeze({
        pick:pickMedia,cancel:cancelMedia,remove:removeManagedMedia,
        events:mediaEvents
    });
    window.__genycasterBrowserSigning = Object.freeze({start,events});
}());

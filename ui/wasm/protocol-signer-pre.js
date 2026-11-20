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
    let nativeClient = null;
    let ownerProtocol = "";
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
    let accountSessionPromise = null;
    let accountSessionOwner = "";
    // The Pro/Streak retry button must use the browser's registered signer,
    // not Qt's native-only vault signer. This is single-flight per identity.
    function retryAccountSession(fid) {
        if (ownerProtocol === "hypersnap-v2"
            || typeof fid !== "string" || !/^[1-9][0-9]{0,18}$/.test(fid)
            || !Number.isSafeInteger(Number(fid))
            || owner !== fid || window.location.origin !== "https://genycaster.xyz")
            return false;
        if (accountSessionPromise && accountSessionOwner === fid) return true;
        const currentEpoch = epoch;
        accountSessionOwner = fid;
        const operation = import('./browser_account_session.mjs')
            .then(module => module.ensureBrowserAccountSession({
                fid, origin:window.location.origin,
            }))
            .then(result => {
                if (epoch === currentEpoch && owner === fid)
                    report({kind:'account-session',phase:'ready',
                        fid,csrfToken:result.csrf_token});
            })
            .catch(error => {
                if (epoch === currentEpoch && owner === fid)
                    report({kind:'account-session',phase:'failed',fid,
                        error:String(error?.message||error).slice(0,140)});
            })
            .finally(() => {
                if (accountSessionPromise === operation) {
                    accountSessionPromise = null;
                    accountSessionOwner = "";
                }
            });
        accountSessionPromise = operation;
        return true;
    }
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
                    if(owner===fid) retryAccountSession(fid);
                }
            },
        });
        client = current;
        initializedBase = base;
        return current;
    }
    async function createNativeClient() {
        if(nativeClient)return nativeClient;
        const {deployment}=await import('./deployment_config.mjs');
        const {createBrowserSignerVault}=await import('./secure_signer_vault.mjs');
        const {createBrowserProtocolSigner}=await import('./browser_protocol_signer.mjs');
        nativeClient=createBrowserProtocolSigner({
            browser:window,snapchainBase:deployment.hypersnapOrigin,
            vault:createBrowserSignerVault(window,{scope:'hypersnap-v2'}),
            onState(status) {
                report({kind:'signer',protocol:'hypersnap-v2',...status});
            }
        });
        return nativeClient;
    }
    async function nativeCustodyForFid(fid,phrase){
        const {deriveFarcasterCustody}=await import('./farcaster_recovery.mjs');
        const {createBrowserNativeRegistry}=await import('./hypersnap_native_account.mjs');
        const custody=await deriveFarcasterCustody(phrase);
        try{
            const verified=await createBrowserNativeRegistry().lookup(custody.address);
            if(verified.state!=='confirmed' || verified.fid!==fid)
                throw new Error('HyperSnap custody is not verified for this FID');
            return custody;
        }catch(error){custody.dispose();throw error;}
    }
    // Show feedback in the same browser frame as the click, before ES module
    // parsing or cryptographic initialization can delay the recovery form.
    // This overlay owns no secret; it is removed before the 24-word UI opens.
    async function openRecoveryDialog(signal, onAuthorize, options) {
        const previous=document.getElementById('genycaster-recovery-opening');
        if(previous)previous.remove();
        const layer=document.createElement('div');
        layer.id='genycaster-recovery-opening';
        layer.setAttribute('role','status');
        layer.setAttribute('aria-live','polite');
        const isPersian=(window.__genycasterPreferredLanguage
            ||window.navigator?.language||'en').toLowerCase().startsWith('fa');
        const light=document.documentElement?.dataset?.theme==='light'
            ||(document.documentElement?.dataset?.theme!=='dark'
               &&window.matchMedia?.('(prefers-color-scheme: light)')?.matches);
        layer.style.cssText='position:fixed;inset:0;z-index:2147483647;'
            +'display:flex;align-items:center;justify-content:center;'
            +'background:'+(light?'#fff':'#101116')+';color:'
            +(light?'#17181c':'#f4f6ff')+';font:600 16px system-ui,sans-serif';
        const indicator=document.createElement('span');
        indicator.textContent=isPersian?'در حال آماده‌سازی بازیابی امن…'
            :'Preparing secure recovery…';
        const spinner=document.createElement('span');
        spinner.setAttribute('aria-hidden','true');
        spinner.style.cssText='display:inline-block;vertical-align:middle;'
            +'width:19px;height:19px;margin-inline-end:12px;border-radius:50%;'
            +'border:2px solid currentColor;border-top-color:transparent;'
            +'animation:genycaster-recovery-opening-spin .8s linear infinite';
        const style=document.createElement('style');
        style.textContent='@keyframes genycaster-recovery-opening-spin'
            +'{to{transform:rotate(360deg)}}'
            +'@media(prefers-reduced-motion:reduce)'
            +'{#genycaster-recovery-opening span{animation:none!important}}';
        layer.append(style,spinner,indicator);
        document.body.appendChild(layer);
        try {
            // Two frames allow the browser to display the indicator before a
            // cold dynamic import evaluates the BIP39 wordlist and signer code.
            if(typeof window.requestAnimationFrame==='function')
                await new Promise(resolve=>window.requestAnimationFrame(
                    ()=>window.requestAnimationFrame(resolve)));
            else await new Promise(resolve=>setTimeout(resolve,0));
            if(signal?.aborted)throw new Error('Recovery import cancelled');
            const {promptForFarcasterRecovery}=
                await import('./recovery-phrase-dialog.mjs');
            if(signal?.aborted)throw new Error('Recovery import cancelled');
            layer.remove();
            return await promptForFarcasterRecovery(window,signal,onAuthorize,options);
        }finally{layer.remove();}
    }
    async function authorizeNativeRecovery(current,fid,thisEpoch) {
        const controller=new AbortController();recoveryController=controller;
        const {promptForFarcasterRecovery}=
            await import('./recovery-phrase-dialog.mjs');
        if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted)return;
        return promptForFarcasterRecovery(window,controller.signal,
            phrase=>current.provision(fid,
                requestedFid=>nativeCustodyForFid(requestedFid,phrase),
                ()=>epoch===thisEpoch && owner===fid && !controller.signal.aborted),
            {presentation:'full',protocol:'hypersnap-v2'});
    }
    async function authorizeRecoveryInSetup(current, fid, thisEpoch) {
        const controller = new AbortController();
        recoveryController = controller;
        const { promptForFarcasterRecovery } =
            await import('./recovery-phrase-dialog.mjs');
        if (controller.signal.aborted ||
            epoch !== thisEpoch || owner !== fid) return;
        // Re-provisioning from Account/Profile must cover the entire app,
        // not reuse the onboarding split-pane layout over the live profile.
        await promptForFarcasterRecovery(window,controller.signal,
            phrase=>current.provisionFromRecovery(fid,phrase),
            {presentation:'full'});
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
            owner="";ownerProtocol="";inFlight=false;
            client?.signedOut();nativeClient?.signedOut();
            // Serialize with an in-flight remembered identity write to
            // prevent a late async write resurrecting an explicitly logged-out user.
            void rememberedWrite.catch(()=>{}).then(async()=>{
                try{await (await deviceSession()).clear();}
                catch(error){console.warn('[Genycaster] Local sign-out cleanup failed',error);}
            });
            return;
        }
        if (action === "resume-account") {
            const {requestId}=JSON.parse(payloadString);
            if(!/^[0-9a-f-]{36}$/i.test(requestId||""))return;
            const thisEpoch=++epoch;
            // The boot screen must wait for a *terminal* restore result.
            // A missing local identity is expected, not an auth failure.
            // Never show Setup based on the earlier provisional Guest state.
            if(signedOutLocally()){
                report({kind:'setup-account',phase:'cancelled',fid:'',
                    requestId,localDevice:true});
                return;
            }
            void (async()=>{
                try{
                    await rememberedWrite.catch(()=>{});
                    const device=await deviceSession();
                    const account=await device.restore();
                    const protocol=account?await device.protocolFor(account):'';
                    if(epoch!==thisEpoch)return;
                    if(account && protocol==='hypersnap-v2') {
                        // A remembered device key alone is not a HyperSnap
                        // custody proof. Authorize a resumed native session
                        // only after the stored protocol signer key is verified
                        // again by the correct live HyperSnap registry.
                        const signer=await createNativeClient();
                        const registration=await signer.restore(account);
                        if(!registration)
                            throw new Error('HyperSnap signer authorization needs recovery');
                    }
                    if(epoch!==thisEpoch)return;
                    report({kind:'setup-account',phase:account&&!owner?'ready':'cancelled',
                        fid:account&&!owner?account:'',requestId,localDevice:true,
                        protocol});
                }catch(error){
                    if(epoch===thisEpoch)
                        report({kind:'setup-account',phase:'failed',fid:'',
                            requestId,localDevice:true,
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
            nativeClient?.signedOut();ownerProtocol="";
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
            nativeClient?.signedOut();
            ownerProtocol="";
            inFlight = false;
        }
        if (inFlight) {
            // Never cancel an in-flight signed write. A Setup recovery is
            // different from an ordinary signer action: Qt waits for a
            // one-shot setup-account event, not a "write" error. Otherwise
            // the tap appears to do absolutely nothing.
            if (action === "recover-account" || action === "recover-hypersnap"
                || action === "create-hypersnap" || action === "create-farcaster") {
                let requestId = "", protocol = "";
                try {
                    const payload=JSON.parse(payloadString);
                    requestId=payload.requestId||"";
                    protocol=payload.protocol||"";
                } catch { /* handled as an invalid request below */ }
                report({kind: "setup-account", phase: "failed",
                    requestId, protocol, fid: "",
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
                if (action === "recover-hypersnap"
                    || action === "create-hypersnap"
                    || action === "create-farcaster") {
                    const {requestId,protocol}=JSON.parse(payloadString);
                    if(typeof requestId!=='string' || !/^[0-9a-f-]{36}$/i.test(requestId)
                        || protocol!==(action==="create-farcaster"
                            ?"farcaster-snapchain":"hypersnap-v2"))
                        throw new Error('Invalid protocol registration request');
                    const controller=new AbortController();
                    recoveryController=controller;
                    const isCurrent=()=>epoch===thisEpoch && owner===fid
                        && !controller.signal.aborted;
                    // A native registry module and its crypto dependencies
                    // are needed only AFTER the 24-word form has opened and
                    // the user has submitted a locally verified phrase.
                    let confirmed=null;
                    if(action==="recover-hypersnap"){
                        confirmed=await openRecoveryDialog(controller.signal,
                            async phrase=>{
                                if(!isCurrent())throw new Error('Recovery cancelled');
                                const {createBrowserNativeRegistry}=
                                    await import('./hypersnap_native_account.mjs');
                                const recovered=await createBrowserNativeRegistry().recover(
                                    phrase,{signal:controller.signal});
                                if(!isCurrent())throw new Error('Recovery cancelled');
                                // Native custody registry verification alone is
                                // the proof for *account recovery*. KEY_ADD is a
                                // separate, optional permission to publish.
                                // Never block importing a verified FID merely
                                // because signer authorization is missing,
                                // unreachable, or rejected by the node.
                                return recovered;
                            },{presentation:'full',protocol:'hypersnap-v2'});
                    }else {
                        const registryModule=action==="create-farcaster"
                            ?null:await import('./hypersnap_native_account.mjs');
                        const {promptCreateBrowserWallet}=
                            await import('./browser_account_create_dialog.mjs');
                        // Never solicit a new recovery phrase if the paid
                        // first-party Farcaster allocator is not deployed.
                        const enrollment=action==="create-farcaster"
                            ?(await import('./farcaster_new_account.mjs'))
                                .createBrowserFarcasterRegistration()
                            :null;
                        if(enrollment)await enrollment.preflight();
                        const nativeRegistry=registryModule?.createBrowserNativeRegistry();
                        confirmed=await promptCreateBrowserWallet({
                            browser:window,signal:controller.signal,
                            protocol,
                            onRegister:async(phrase,options)=>{
                                if(!isCurrent())throw new Error('Account creation cancelled');
                                if(!enrollment) {
                                    const established=await nativeRegistry.register(phrase,options);
                                    // The FID is now irreversible. If signer
                                    // activation fails, keep the FID and the
                                    // backup usable through Existing account.
                                    try {
                                        const signer=await createNativeClient();
                                        await signer.provision(established.fid,
                                            requestedFid=>nativeCustodyForFid(requestedFid,phrase),
                                            isCurrent);
                                    } catch {
                                        console.warn('HyperSnap signer authorization is pending');
                                    }
                                    return established;
                                }
                                const established=await enrollment.register(phrase,options);
                                // Enrollment is irreversible and already
                                // verified. KEY_ADD authorization is a separate
                                // step: its failure cannot erase that FID or
                                // cause a second paid registration.
                                try{
                                    const signer=await createClient(base);
                                    await signer.provisionFromRecovery(established.fid,phrase);
                                }catch(error){
                                    console.warn('Farcaster signer activation requires separate authorization');
                                }
                                return established;
                            },
                        });
                    }
                    if(!isCurrent())return;
                    const verifiedFid=String(confirmed?.fid||'');
                    const nativeAccount=protocol==='hypersnap-v2';
                    if(!/^[1-9][0-9]{0,19}$/.test(verifiedFid)
                        || BigInt(verifiedFid)<(nativeAccount?4294967296n:1n)
                        || BigInt(verifiedFid)>(nativeAccount
                            ?18446744073709551615n:BigInt(Number.MAX_SAFE_INTEGER)))
                        throw new Error('Protocol did not confirm a valid account FID');
                    // An independently verified native FID is never routed to
                    // Optimism KEY_ADD or the Farcaster signer cache.
                    owner=verifiedFid;
                    ownerProtocol=protocol;
                    // Local device continuity retains the network namespace:
                    // otherwise a restarted HyperSnap account is incorrectly
                    // reopened as a Farcaster/device-local identity.
                    rememberedWrite=rememberedWrite.catch(()=>{}).then(async()=>{
                        const device=await deviceSession();
                        await device.remember(verifiedFid,protocol);
                        if(epoch===thisEpoch&&owner===verifiedFid)
                            try{window.localStorage?.removeItem(signOutKey);}catch{}
                    });
                    await rememberedWrite.catch(()=>{
                        console.warn('[Genycaster] Trusted device continuity is unavailable');
                    });
                    // owner transitions from the empty pre-auth FID to the
                    // verified FID above. isCurrent() intentionally checks the
                    // *previous* owner during registration and must not be
                    // used after the verified identity transition.
                    if(epoch!==thisEpoch || owner!==verifiedFid
                        || controller.signal.aborted)return;
                    report({kind:'setup-account',phase:'ready',
                        protocol,fid:verifiedFid,requestId});
                } else if (action === "recover-account") {
                    // This event is accepted by Qt only for the current one-shot
                    // recovery request. No phrase or custody address is sent to Qt.
                    const {requestId} = JSON.parse(payloadString);
                    if(typeof requestId !== 'string' ||
                       !/^[0-9a-f-]{36}$/i.test(requestId))
                        throw new Error('Invalid recovery request');
                    const controller=new AbortController();
                    recoveryController=controller;
                    if(epoch!==thisEpoch || owner!==fid) return;
                    const {openProofGatewayOrigin}=JSON.parse(payloadString);
                    let verifiedOpenProof=null;
                    const recoveredFid=await openRecoveryDialog(
                        controller.signal,async phrase=>{
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
                    if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted) return;
                    // recover-account begins with an empty owner because no FID
                    // has been verified. Adopt ONLY the recovered, on-chain
                    // verified Farcaster FID after its signer vault is ready.
                    // Without this transition, owner remains empty and the
                    // Pro/Streak signed account-session retry is rejected.
                    owner = recoveredFid;
                    ownerProtocol='farcaster-snapchain';
                    // Native Qt may mark this as canonical OpenProof identity
                    // only when the verified subject and issuer accompany the
                    // exact one-shot recovery request. Missing proof means a
                    // functional, explicitly local Farcaster session.
                    report({kind:'setup-account',phase:'ready',
                        protocol:'farcaster-snapchain',fid:recoveredFid,requestId,
                        identityIssuer:verifiedOpenProof?.issuer || '',
                        identitySubject:verifiedOpenProof?.subject || '',
                        accountTicket:verifiedOpenProof?.accountTicket || '',
                        accountExpiresSeconds:verifiedOpenProof?.expiresSeconds || 0});
                    // The Qt recovery observer initiates the signed account
                    // retry AFTER the authenticated session is committed.
                    // Initiating it here can race Qt's queued identity event
                    // and lose the only verified Pro/Streak response.
                } else {
                    const options=payloadString?JSON.parse(payloadString):{};
                    const native=options.protocol==='hypersnap-v2';
                    const current=native?await createNativeClient():await createClient(base);
                    if(epoch!==thisEpoch || owner!==fid)return;
                    ownerProtocol=native?'hypersnap-v2':'farcaster-snapchain';
                    if (action === "restore" || action === "restore-for-setup") {
                    if(action==="restore" && payloadString){
                        const options=JSON.parse(payloadString);
                        if(options.rememberLocal===true){
                            rememberedWrite=rememberedWrite.catch(()=>{}).then(async()=>{
                                if(epoch!==thisEpoch || owner!==fid)return;
                                await (await deviceSession()).remember(fid,
                                    native?'hypersnap-v2':'farcaster-snapchain');
                                if(epoch===thisEpoch && owner===fid)
                                    try{window.localStorage?.removeItem(signOutKey);}catch{}
                            });
                            await rememberedWrite.catch(error=>
                                console.warn('[Genycaster] Device session could not be saved',error));
                        }
                    }
                    const existing = await current.restore(fid);
                    // A readable account and a registered browser vault key
                    // can restore the private Pro/Streak session even before
                    // the protocol signer reports its separate write-ready
                    // phase. The server still validates the one-use signature
                    // and exact owner FID; no local balance implies a tier.
                    if (existing && !native && epoch===thisEpoch && owner===fid)
                        retryAccountSession(fid);
                    if (action === "restore-for-setup" && !existing) {
                        // Never collect a phrase on app reload or a restored session.
                        // This command is scoped to a verified, explicit setup login.
                        if (epoch !== thisEpoch || owner !== fid) return;
                        if(native)
                            await authorizeNativeRecovery(current,fid,thisEpoch);
                        else
                            await authorizeRecoveryInSetup(current,fid,thisEpoch);
                    }
                } else if (action === "provision") {
                    // Explicit retry in Setup or Account Settings.
                    if(native)
                        await authorizeNativeRecovery(current,fid,thisEpoch);
                    else
                        await authorizeRecoveryInSetup(current,fid,thisEpoch);
                } else if (action === "revoke") {
                    await current.revoke(fid);
                } else {
                    report({kind: "write", phase: "submitting", fid,
                        protocol:native?'hypersnap-v2':'farcaster-snapchain'});
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
                    case "block":
                        hash = await current.block(fid,payload.targetFid,payload.active); break;
                    case "profile":
                        hash = await current.updateProfile(fid,payload); break;
                    default:
                        throw new Error("Unsupported browser signer action");
                    }
                    if (epoch === thisEpoch && owner === fid)
                        report({kind: "write", phase: "confirmed", fid, hash,
                            protocol:native?'hypersnap-v2':'farcaster-snapchain',
                            operation: action,
                            targetFid: action==='follow'||action==='block'
                                ? String(payload.targetFid||'') : '',
                            active: action==='follow'||action==='block'
                                ? payload.active===true : false,
                            targetCastId: action==='remove'
                                ? String(payload.target?.hash||'') : ''});
                    }
                }
            } catch(error) {
                if (epoch !== thisEpoch || owner !== fid) return;
                const cancelled = error?.code === 'user-cancelled' ||
                    error?.message === 'Recovery import cancelled';
                if (action === "recover-account" || action === "recover-hypersnap"
                    || action === "create-hypersnap" || action === "create-farcaster") {
                    const {requestId,protocol}=JSON.parse(payloadString);
                    report({kind:'setup-account',phase:cancelled?'cancelled':'failed',
                        requestId,protocol,fid:'',
                        error:cancelled?'':String(error?.message||error).slice(0,350)});
                    return;
                }
                report({kind: action === "restore" || action === "restore-for-setup"
                       || action === "provision" || action === "revoke" ? "signer" : "write",
                    protocol:ownerProtocol,
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
        // Must remain synchronous inside the actual Qt button tap: browsers
        // may reject file picker prompts after an await/dynamic import.
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
    window.__genycasterBrowserSigning = Object.freeze({start,retryAccountSession,events});
}());

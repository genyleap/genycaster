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
    // Public registration diagnostics only. Each request has its own
    // correlation token and cancellation scope, independent of account restore.
    const registryPreflights=new Map();
    window.__genycasterRegistryPreflight=Object.freeze({
        start(token,origins,timeoutMs){
            if(!/^[0-9a-f-]{36}$/i.test(token)||registryPreflights.has(token)
                ||!Array.isArray(origins)||origins.length>64)return false;
            const controller=new AbortController();
            const entry={controller,result:null};
            registryPreflights.set(token,entry);
            void import('./hypersnap_native_account.mjs').then(({preflightBrowserNativeRegistries})=>
                preflightBrowserNativeRegistries({origins,browser:window,timeoutMs,signal:controller.signal})
            ).then(rows=>{if(registryPreflights.get(token)===entry)entry.result=JSON.stringify(rows);})
             .catch(()=>{if(registryPreflights.get(token)===entry)entry.result='[]';});
            return true;
        },
        take(token){
            const entry=registryPreflights.get(token);
            if(!entry||entry.result===null)return '';
            registryPreflights.delete(token);
            return entry.result;
        },
        cancel(token){
            const entry=registryPreflights.get(token);
            registryPreflights.delete(token);
            entry?.controller.abort();
        }
    });
    const events = [];
    // One QML-owned setup ceremony. No secret is included in the polled status.
    let setupWallet=null, setupWalletLoad=null, setupLoadEpoch=0, stagedRecovery=null;
    const setupWalletReady=()=>setupWalletLoad??=(import('./browser_setup_wallet.mjs').then(({createSetupWallet})=>setupWallet=createSetupWallet({browser:window,preflight:async protocol=>{
        if(protocol==='hypersnap-v2'){
            const {createBrowserNativeRegistry}=await import('./hypersnap_native_account.mjs');
            if(!(await createBrowserNativeRegistry().networkHealth()).ready)throw Error('HyperSnap is syncing');
        }else await (await import('./farcaster_new_account.mjs')).createBrowserFarcasterRegistration().preflight();
    }})).catch(error=>{setupWalletLoad=null;throw error;}));
    let setupLoading=false,setupLoadError='';
    window.__genycasterSetupWallet=Object.freeze({
        state:()=>setupWallet?.snapshot()??{busy:setupLoading,error:setupLoadError},
        begin(protocol,phrase=''){
            if(setupLoading)return false;
            const current=++setupLoadEpoch;setupLoading=true;setupLoadError='';
            void setupWalletReady().then(wallet=>{if(current===setupLoadEpoch)return wallet.begin(protocol,phrase);}).catch(()=>{if(current===setupLoadEpoch)setupLoadError='wallet_unavailable';}).finally(()=>{phrase='';if(current===setupLoadEpoch)setupLoading=false;});return true;
        },
        challenge:()=>setupWallet?.challenge()??false,
        verify:(a,b)=>setupWallet?.verify(a,b)??false,
        reveal:()=>setupWallet?.reveal(),copy:()=>setupWallet?.copy()??false,
        paint(ptr,size){
            if(!Number.isSafeInteger(ptr)||ptr<=0||size!==512||ptr+size>HEAPU8.length)return false;
            return setupWallet?.displayWords(phrase=>{if(lengthBytesUTF8(phrase)+1<=size)stringToUTF8(phrase,ptr,size);})??false;
        },
        acceptVoucher:value=>setupWallet?.acceptVoucher(value)??false,
        reserveSponsored(){if(!setupWallet)return false;void setupWallet.reserveSponsored();return true;},
        leave:()=>setupWallet?.leave()??false,
        prepare:(p,v,u)=>setupWallet?.prepare(p,v,u)??false,
        stageRecovery(p,words){if(window.isSecureContext!==true||!['hypersnap-v2','farcaster-snapchain'].includes(p)||typeof words!=='string'||words.length>512||stagedRecovery)return false;
            const entry={p,words};stagedRecovery=entry;window.setTimeout(()=>{if(stagedRecovery===entry){entry.words='';stagedRecovery=null;}},5000);return true;},
        discard(){setupLoadEpoch++;setupLoading=false;if(stagedRecovery){stagedRecovery.words="";stagedRecovery=null;}return setupWallet?.discard()??true;},
        cancel(){setupLoadEpoch++;setupLoading=false;if(stagedRecovery){stagedRecovery.words="";stagedRecovery=null;}return setupWallet?.cancel()??true;}
    });
    async function consumeRecovery(signal,callback,options={}){
        if(stagedRecovery){let entry=stagedRecovery;stagedRecovery=null;try{if(signal?.aborted||entry.p!==(options.protocol||'farcaster-snapchain'))throw Error('Recovery cancelled');return await callback(entry.words);}finally{entry.words='';}}
        if(options.requireStaged)throw Error('Recovery input expired. Re-enter the phrase in Setup.');
        return openRecoveryDialog(signal,callback,options);
    }
    let client = null, initializedBase = "", epoch = 0, owner = "";
    let nativeClient = null;
    let ownerProtocol = "";
    let inFlight = false;
    let recoveryController = null;
    let rememberedWrite = Promise.resolve();
    let cloudRetirement=Promise.resolve();
    let revocationFlush=null;
    function flushCloudRevocations(){
        if(revocationFlush)return revocationFlush;
        revocationFlush=cloudRetirement.then(async()=>{
            const module=await import('./browser_account_session.mjs');
            await module.finishBrowserCloudRetirements({environment:window});
            await module.flushBrowserCloudRevocations({environment:window,
                signal:AbortSignal.timeout(15_000)});
        }).catch(()=>{}).finally(()=>{revocationFlush=null;});
        return revocationFlush;
    }
    window.addEventListener('online',flushCloudRevocations);
    window.addEventListener('pageshow',flushCloudRevocations);
    window.addEventListener('focus',flushCloudRevocations);
    window.setInterval(flushCloudRevocations,60_000);
    void flushCloudRevocations();
    const signOutKey = 'genycaster.device-session.signed-out';
    const signedOutLocally=()=>{
        try{return window.localStorage?.getItem(signOutKey)==='1';}
        catch{return false;}
    };
    const deviceSession = async () =>
        (await import('./browser_device_session.mjs')).createDeviceSession(window);
    let accountSessionPromise = null;
    let accountSessionOwner = "";
    let accountSessionNetwork = "";
    let accountSessionAbort = null;
    function cancelAccountSession() {
        accountSessionAbort?.abort();
        accountSessionAbort=null;
        accountSessionPromise=null;
        accountSessionOwner="";
        accountSessionNetwork="";
    }
    // The Pro/Streak retry button must use the browser's registered signer,
    // not Qt's native-only vault signer. This is single-flight per identity.
    function retryAccountSession(fid, bearer = '') {
        if (typeof fid !== "string" || !/^[1-9][0-9]{0,19}$/.test(fid)
            || BigInt(fid)>18446744073709551615n
            || owner !== fid || window.location.origin !== "https://genycaster.xyz")
            return false;
        const network=ownerProtocol==='hypersnap-v2'?'hypersnap':'farcaster';
        if (accountSessionPromise && accountSessionOwner === fid && accountSessionNetwork===network && !bearer) return true;
        if(accountSessionPromise)cancelAccountSession();
        const controller=new AbortController();
        const retirementBeforeRequest=cloudRetirement;
        accountSessionAbort=controller;
        accountSessionOwner = fid;
        accountSessionNetwork=network;
        const operation = import('./browser_account_session.mjs')
            .then(async module => {
                await retirementBeforeRequest.catch(()=>{});
                if(controller.signal.aborted)throw Error('Account session cancelled');
                await module.finishBrowserCloudRetirements({environment:window});
                const args={fid,network,bearer,origin:window.location.origin,signal:controller.signal};
                const device=await module.ensureBrowserCloudDeviceSession(args);
                if(device)return device;
                if(network==='hypersnap')throw Error('Cloud device enrollment required');
                if(bearer)return module.bootstrapBrowserOpenProofSession(args);
                // Compatibility only; strict account API disables this endpoint.
                return module.ensureBrowserAccountSession(args);
            })
            .then(result => {
                if (accountSessionAbort===controller && owner===fid &&
                    (ownerProtocol==='hypersnap-v2')===(network==='hypersnap'))
                    report({kind:'account-session',phase:'ready',
                        fid,protocol:network==='hypersnap'?'hypersnap-v2':'farcaster-snapchain',
                        csrfToken:result.csrf_token,deviceBound:result.auth_source==='device'});
            })
            .catch(error => {
                if (accountSessionAbort===controller && owner===fid &&
                    (ownerProtocol==='hypersnap-v2')===(network==='hypersnap'))
                    report({kind:'account-session',phase:'failed',fid,
                        protocol:network==='hypersnap'?'hypersnap-v2':'farcaster-snapchain',
                        error:'account_session_unavailable'});
            })
            .finally(() => {
                if (accountSessionPromise === operation) {
                    accountSessionPromise = null;
                    accountSessionOwner = "";
                    accountSessionNetwork = "";
                    accountSessionAbort = null;
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
    async function enrollNativeCloudDevice(fid,phrase,signal) {
        const {deriveFarcasterCustody}=await import('./farcaster_recovery.mjs');
        const custody=await deriveFarcasterCustody(phrase);
        try {
            await cloudRetirement.catch(()=>{});
            const {ensureBrowserCloudDeviceSession,finishBrowserCloudRetirements}=await import('./browser_account_session.mjs');
            await finishBrowserCloudRetirements({environment:window});
            await ensureBrowserCloudDeviceSession({fid,network:'hypersnap',custody,signal});
        } finally {custody.dispose();}
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
    function authorizeRecovery(fid,native,base,words) {
        const protocol=native?'hypersnap-v2':'farcaster-snapchain';
        const count=typeof words==='string'?words.trim().split(/\s+/).length:0;
        if(window.isSecureContext!==true||owner!==fid||ownerProtocol!==protocol||inFlight
            ||typeof words!=='string'||words.length>512
            ||(count!==24&&(native||count!==12)))return false;
        start('provision',fid,base,JSON.stringify({protocol}),words);
        return true;
    }

    function start(action, fid, base, payloadString = "", recoveryWords = "") {
        if(recoveryWords){
            // The explicit one-shot input must not be used to change the
            // active identity, including a colliding FID across protocols.
            const protocol=JSON.parse(payloadString).protocol;
            if(action!=='provision'||window.isSecureContext!==true||inFlight
                ||owner!==fid||ownerProtocol!==protocol)return;
        }
        // Runtime identities are authoritative in Qt, never JS localStorage.
        // If the identity changes while a wallet is prompting, pending
        // requests are invalidated and cannot submit protocol messages.
        if (action === "forget-account") {
            window.__genycasterSetupWallet.cancel();
            const previousOwner=owner,previousProtocol=ownerProtocol;
            const pendingCloud=accountSessionPromise;
            cancelAccountSession();
            cancelMedia();
            if(previousOwner){
                const network=previousProtocol==='hypersnap-v2'?'hypersnap':'farcaster';
                // Persist the retirement intent synchronously before the
                // old owner is cleared, even if the browser closes now.
                try{window.localStorage.setItem('genycaster.cloud-retirement.'+network+'.'+previousOwner,'1');}catch{}
                cloudRetirement=cloudRetirement.catch(()=>{}).then(()=>pendingCloud?.catch(()=>{}))
                    .then(async()=>{
                        const module=await import('./browser_account_session.mjs');
                        await module.retireBrowserCloudDevice({fid:previousOwner,network,environment:window});
                    });
                void cloudRetirement.then(flushCloudRevocations).catch(()=>{});
            }
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
                    // Reopen the locally remembered viewer. Qt requests a
                    // separate live signer check after committing the session.
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
            window.__genycasterSetupWallet.cancel();
            cancelAccountSession();
            cancelMedia();
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
            cancelAccountSession();
            cancelMedia();
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
                if(action==='create-hypersnap'||action==='create-farcaster')setupWallet?.failPrepared();
                if(stagedRecovery){stagedRecovery.words='';stagedRecovery=null;}
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
                    let registeredOpenProof=null;
                    if(action==="recover-hypersnap"){
                        confirmed=await consumeRecovery(controller.signal,
                            async phrase=>{
                                if(!isCurrent())throw new Error('Recovery cancelled');
                                const {createBrowserNativeRegistry}=
                                    await import('./hypersnap_native_account.mjs');
                                const recovered=await createBrowserNativeRegistry().recover(
                                    phrase,{signal:controller.signal});
                                if(!isCurrent())throw new Error('Recovery cancelled');
                                // The explicit recovery confirmation also
                                // authorizes this device's signer using the
                                // phrase already in the current secure dialog.
                                // KEY_ADD failure never erases verified identity
                                // or repeats the account registration.
                                try {
                                    const signer=await createNativeClient();
                                    if(!isCurrent())throw new Error('Recovery cancelled');
                                    await signer.provision(recovered.fid,
                                        requestedFid=>nativeCustodyForFid(requestedFid,phrase),
                                        isCurrent);
                                }catch(error){
                                    if(!isCurrent())throw error;
                                    console.warn('HyperSnap signer activation is pending');
                                }
                                try {await enrollNativeCloudDevice(recovered.fid,phrase,controller.signal);}
                                catch {if(!isCurrent())throw Error('Recovery cancelled');}
                                return recovered;
                            },{presentation:'full',protocol:'hypersnap-v2',requireStaged:JSON.parse(payloadString).requireStagedRecovery===true});
                    }else {
                        const registryModule=action==="create-farcaster"
                            ?null:await import('./hypersnap_native_account.mjs');
                        const wallet=await setupWalletReady();
                        // Never solicit a new recovery phrase if the paid
                        // first-party Farcaster allocator is not deployed.
                        const enrollment=action==="create-farcaster";
                        // Preflight completed before the shared backup ceremony.
                        const nativeRegistry=registryModule?.createBrowserNativeRegistry();
                        confirmed=await wallet.submit(protocol,controller.signal,async(phrase,options)=>{
                                if(!isCurrent())throw new Error('Account creation cancelled');
                                if(!enrollment) {
                                    let established;
                                    if(options.restored||options.resumeOnly){
                                        const previous=await nativeRegistry.lookup(options.address,controller.signal);
                                        if(previous.state==='confirmed')established=previous;
                                        else if(options.resumeOnly||previous.state!=='not-found'){
                                            const pending=Error('Previous registration must be reconciled before another submission.');
                                            pending.outcomeUnknown=options.resumeOnly;throw pending;
                                        }
                                    }
                                    established??=await nativeRegistry.register(phrase,options);
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
                                    try {await enrollNativeCloudDevice(established.fid,phrase,controller.signal);}
                                    catch {if(!isCurrent())throw Error('Registration cancelled');}
                                    return established;
                                }
                                const established=await options.sponsoredRegistration.submitReserved(phrase,options);
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
                                // The verified registration already owns the FID. Enroll the
                                // private-cloud key while custody material is still in the dialog.
                                // A temporary OpenProof outage cannot undo paid registration.
                                try {
                                    const {deriveFarcasterCustody}=await import('./farcaster_recovery.mjs');
                                    const custody=await deriveFarcasterCustody(phrase);
                                    try {
                                        if(!isCurrent()||custody.address.toLowerCase()!==established.address.toLowerCase())
                                            throw Error('Registration identity changed');
                                        const {verifyRecoveredFarcasterIdentity}=await import('./openproof_recovery.mjs');
                                        const {deployment}=await import('./deployment_config.mjs');
                                        registeredOpenProof=await verifyRecoveredFarcasterIdentity({
                                            fid:established.fid,custody,gatewayOrigin:deployment.browserGateway,
                                            signal:controller.signal});
                                        await cloudRetirement.catch(()=>{});
                                        const {ensureBrowserCloudDeviceSession,finishBrowserCloudRetirements}=await import('./browser_account_session.mjs');
                                        await finishBrowserCloudRetirements({environment:window});
                                        await ensureBrowserCloudDeviceSession({fid:established.fid,
                                            bearer:registeredOpenProof.accountTicket,signal:controller.signal});
                                    }finally{custody.dispose();}
                                }catch {if(!isCurrent())throw Error('Registration cancelled');}
                                return established;
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
                        protocol,fid:verifiedFid,requestId,
                        identityIssuer:registeredOpenProof?.issuer||'',
                        identitySubject:registeredOpenProof?.subject||'',
                        accountTicket:registeredOpenProof?.accountTicket||'',
                        accountExpiresSeconds:registeredOpenProof?.expiresSeconds||0});
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
                    const recoveredFid=await consumeRecovery(
                        controller.signal,async phrase=>{
                            if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted)
                                throw new Error("Recovery was cancelled");
                            // Importing an existing account is read-only.
                            // Do not initialize IndexedDB signer storage or
                            // create a Snapchain signer before verifying FID.
                            const {verifyFarcasterRecovery}=
                                await import('./farcaster_custody_recovery.mjs');
                            if(epoch!==thisEpoch || owner!==fid || controller.signal.aborted)
                                throw new Error("Recovery was cancelled");
                            return verifyFarcasterRecovery(
                                phrase,{signal:controller.signal,
                                onVerifiedCustody:async (verifiedFid,custody)=>{
                                    // Setup's explicit "Import and authorize"
                                    // grants the device signer in the SAME
                                    // recovery transaction, while the phrase
                                    // remains in the ephemeral recovery ceremony.
                                    // The signer checks the exact custody/FID
                                    // mapping again before submitting KEY_ADD.
                                    try {
                                        const signer=await createClient(base);
                                        if(epoch!==thisEpoch || owner!==fid
                                            || controller.signal.aborted)
                                            throw new Error('Recovery cancelled');
                                        await signer.provisionFromRecovery(verifiedFid,phrase);
                                    }catch(error){
                                        if(epoch!==thisEpoch || owner!==fid
                                            || controller.signal.aborted)throw error;
                                        // Recovering a verified account does
                                        // not depend on Snapchain write health.
                                        console.warn('Farcaster signer activation is pending');
                                    }
                                    if(!openProofGatewayOrigin) return;
                                    const {verifyRecoveredFarcasterIdentity}=
                                        await import('./openproof_recovery.mjs');
                                    verifiedOpenProof=await verifyRecoveredFarcasterIdentity({
                                        fid:verifiedFid,custody,gatewayOrigin:openProofGatewayOrigin,
                                        signal:controller.signal,
                                    });
                                    try {
                                        await cloudRetirement.catch(()=>{});
                                        const {ensureBrowserCloudDeviceSession,finishBrowserCloudRetirements}=await import('./browser_account_session.mjs');
                                        await finishBrowserCloudRetirements({environment:window});
                                        await ensureBrowserCloudDeviceSession({fid:verifiedFid,
                                            bearer:verifiedOpenProof.accountTicket,signal:controller.signal});
                                    }catch {if(controller.signal.aborted)throw Error('Recovery cancelled');}
                                }});
                        },{protocol:'farcaster-snapchain',requireStaged:JSON.parse(payloadString).requireStagedRecovery===true});
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
                    const nextProtocol=native?'hypersnap-v2':'farcaster-snapchain';
                    if(ownerProtocol&&ownerProtocol!==nextProtocol){cancelAccountSession();cancelMedia();}
                    ownerProtocol=nextProtocol;
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
                    if (action==='restore' && epoch===thisEpoch && owner===fid)
                        retryAccountSession(fid);
                    // A missing signer is a read-only state. Only an
                    // explicit QML consent with recovery input may add a key.
                } else if (action === "provision") {
                    const existing=await current.restore(fid);
                    if(existing || epoch!==thisEpoch || owner!==fid
                        || ownerProtocol!==nextProtocol)return;
                    if(!recoveryWords)return;
                    const controller=new AbortController();
                    recoveryController=controller;
                    const stillCurrent=()=>epoch===thisEpoch && owner===fid
                        && ownerProtocol===nextProtocol && !controller.signal.aborted;
                    if(!stillCurrent())return;
                    if(native)await current.provision(fid,
                        requestedFid=>nativeCustodyForFid(requestedFid,recoveryWords),
                        stillCurrent);
                    else await current.provisionFromRecovery(fid,recoveryWords,stillCurrent);
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
                            targetFid: action==='reaction' ? String(payload.target?.fid||'')
                                : action==='follow'||action==='block'
                                ? String(payload.targetFid||'') : '',
                            active: action==='follow'||action==='block'||action==='reaction'
                                ? payload.active===true : false,
                            reaction: action==='reaction' ? String(payload.reaction||'') : '',
                            targetCastId: action==='remove'||action==='reaction'
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
                    if(action==='create-hypersnap'||action==='create-farcaster')setupWallet?.failPrepared();
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
                recoveryWords='';
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
    async function mediaVault(fid,protocol) {
        if(owner!==fid||ownerProtocol!==protocol||!['hypersnap-v2','farcaster-snapchain'].includes(protocol))
            throw new Error('Media account changed or is not authenticated.');
        const {createBrowserSignerVault}=await import('./secure_signer_vault.mjs');
        if(owner!==fid||ownerProtocol!==protocol)throw new Error('Media account changed.');
        return createBrowserSignerVault(window,{scope:protocol==='hypersnap-v2'?'hypersnap-v2':''});
    }
    function pickMedia(fid,afterIndex,maxImageBytes=2*1024*1024) {
        if(typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid)||owner!==fid)return;
        const protocol=ownerProtocol;
        const picker=document.createElement('input');
        picker.type='file';picker.accept='image/jpeg,image/png,image/webp,image/gif,image/avif';
        picker.multiple=false;picker.style.display='none';
        picker.addEventListener('change',()=>{
            const file=picker.files?.[0];
            picker.remove();
            if(!file||owner!==fid||ownerProtocol!==protocol)return;
            // Always report selection first so the Qt controller can show its
            // localized 2/5 MiB error. Never start a network request for a
            // rejected selection.
            mediaReport({kind:'selected',fid,afterIndex,
                fileName:String(file.name).slice(0,160),
                contentType:String(file.type).slice(0,60),sizeBytes:file.size});
            // Local compression runs before any network request. Bound the
            // *source* at 25 MiB, then validate the optimized output against
            // the account's 2/5 MiB upload tier in browser_media_upload.mjs.
            if(file.size>25*1024*1024)return;
            cancelMedia();
            const current=mediaEpoch;
            const controller=new AbortController();
            mediaAbort=controller;
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
                        file,fid,signal:controller.signal,maxImageBytes,
                        vault:await mediaVault(fid,protocol),
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
    // First-party Stats cards are captured in Qt's virtual filesystem.
    // The browser receives only the generated PNG (never a user-selected
    // File, custody secret or signer key). Reuse the identical signed gateway
    // upload and lifecycle path as the manual image picker.
    function uploadGeneratedMedia(fid,afterIndex,bytes,maxImageBytes=2*1024*1024){
        if(typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid)||owner!==fid)return;
        const protocol=ownerProtocol;
        if(!(bytes instanceof Uint8Array)||bytes.length<64
           ||bytes.length>(maxImageBytes===5*1024*1024?5*1024*1024:2*1024*1024)
           ||[137,80,78,71,13,10,26,10].some((byte,index)=>bytes[index]!==byte)){
            mediaReport({kind:'failed',fid,error:'Invalid generated Stats PNG.'});
            return;
        }
        cancelMedia();
        const current=mediaEpoch;
        const controller=new AbortController();
        mediaAbort=controller;
        const file=new File([bytes],'genycaster-stats-card.png',{type:'image/png'});
        (async()=>{
            try{
                const {uploadBrowserImage}=await import('./browser_media_upload.mjs');
                const result=await uploadBrowserImage({
                    file,fid,signal:controller.signal,maxImageBytes,
                    vault:await mediaVault(fid,protocol),
                    optimizationMode:'original',
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
    }
    const pendingRemovals=new Set();
    async function removeManagedMedia(fid,managementToken){
        if(typeof fid!=='string'||!/^[1-9][0-9]{0,19}$/.test(fid)
            ||typeof managementToken!=='string'||managementToken.length>4096
            ||pendingRemovals.has(managementToken))return;
        pendingRemovals.add(managementToken);
        try{
            const {deleteBrowserAsset}=await import('./browser_media_upload.mjs');
            const protocol=ownerProtocol;
            await deleteBrowserAsset({fid,managementToken,vault:await mediaVault(fid,protocol)});
            mediaReport({kind:'cleanup-completed',fid,managementToken});
        }catch(error){
            mediaReport({kind:'cleanup-failed',fid,managementToken,
                error:String(error?.message||error).slice(0,150)});
        }finally{
            pendingRemovals.delete(managementToken);
        }
    }
    function sharePreparedStatsImage(bytes,caption) {
        // Must call navigator.share() synchronously from the Qt button
        // gesture. The file remains on the device; it is never uploaded to
        // a third-party social service by Genycaster.
        if(!(bytes instanceof Uint8Array)||bytes.length<64
           ||bytes.length>5*1024*1024
           ||[137,80,78,71,13,10,26,10].some((byte,i)=>bytes[i]!==byte))
            return false;
        const file=new File([bytes],'genycaster-stats.png',{type:'image/png'});
        const safeCaption=String(caption||'').slice(0,1800);
        if(typeof navigator.share==='function'
           && typeof navigator.canShare==='function'
           && navigator.canShare({files:[file]})) {
            // Instagram appears here only when installed as a share target;
            // it has no public arbitrary-file upload intent for websites.
            navigator.share({files:[file],title:'Genycaster Stats',
                             text:safeCaption}).catch(()=>{});
            return true;
        }
        const link=document.createElement('a');
        const objectUrl=URL.createObjectURL(file);
        link.href=objectUrl;
        link.download=file.name;
        link.click();
        setTimeout(()=>URL.revokeObjectURL(objectUrl),2000);
        return true;
    }
    window.__genycasterBrowserMedia=Object.freeze({
        pick:pickMedia,uploadGenerated:uploadGeneratedMedia,
        sharePreparedStatsImage,
        cancel:cancelMedia,remove:removeManagedMedia,
        events:mediaEvents
    });
    window.__genycasterBrowserSigning = Object.freeze({start,authorizeRecovery,retryAccountSession,events});
}());

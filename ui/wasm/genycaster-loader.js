(function () {
"use strict";
const $ = id => document.getElementById(id);
const boot=$("genycaster-boot"), heading=$("boot-heading"), status=$("boot-status");
const stage=$("boot-stage"), fill=$("boot-fill"), percent=$("boot-percent"), retry=$("boot-retry");
const track=$("boot-track");
let finished=false, failed=false;
const bootStarted=performance.now();
let wasmResourceUrl;
let wasmResourceObserver=null;
let runtimeDependenciesSettled=false;
let bootPhase="download";
function announce(title,detail,phase) {
    heading.textContent=title;
    status.textContent=detail;
    stage.textContent=phase;
}
function preparing() {
    fill.classList.remove("measured");
    fill.style.width="";
    percent.textContent="";
}
// The browser owns the binary stream so its compiled-WASM cache can work.
// We intentionally do not invent download percentages while streaming
// progress cannot be observed without wrapping the response.
const key="genycaster.boot.loaded.v1";
const versionKey="genycaster.boot.version.v1";
const currentVersion=typeof window.__genycasterAssets?.version==="string"
    ? window.__genycasterAssets.version : "";
let seen=false, previousVersion="";
try{
    seen=localStorage.getItem(key)==="1";
    previousVersion=localStorage.getItem(versionKey)||"";
}catch{}
const updating=seen && !!currentVersion && !!previousVersion && previousVersion!==currentVersion;
const installing=!seen || updating;
bootPhase=installing?"download":"initialize";
if(boot?.dataset){
    boot.dataset.mode=installing?"install":"warm";
    boot.dataset.phase=bootPhase;
}
function showPhase(phase,title,detail,label){
    if(finished||failed)return;
    const order={download:0,install:1,initialize:2,interface:3,ready:4};
    if(order[phase] < order[bootPhase])return; // delayed async event
    bootPhase=phase;
    if(boot?.dataset)boot.dataset.phase=phase;
    track?.setAttribute?.("aria-valuetext",label);
    announce(title,detail,label);
}
if(installing)
    showPhase("download",updating?"Downloading Update":"Downloading App Resources",
        "Checking application files and preparing the local cache…",
        updating?"Updating resources":"Downloading resources");
else
    showPhase("initialize","Initializing Social Workspace",
        "Checking application files and preparing the Qt runtime…",
        "Preparing runtime");
const originalFetch=window.fetch.bind(window);
const hashedWasm=/^farcaster_client\.[0-9a-f]{20}\.wasm$/;
const requestedAsset=window.__genycasterAssets?.wasm;
const verifiedAsset=typeof requestedAsset==="string" && hashedWasm.test(requestedAsset)
    ? new URL(requestedAsset,location.href) : null;
function wasmTiming(url) {
    try{
        const entries=performance.getEntriesByName(url,"resource");
        return entries.length ? entries[entries.length-1] : null;
    }catch{return null;}
}
// Preserve the native Response and use browser Resource Timing to observe the
// *completed* WASM transfer. Headers alone do not mean the binary downloaded.
function engineReceived(url) {
    if(!installing || finished || bootPhase!=="download")return;
    const timing=wasmTiming(url);
    if(!timing || timing.responseEnd<=0)return;
    showPhase("install",updating?"Applying Runtime Update":"Installing Social Runtime",
        "Application engine received. Preparing WebAssembly and local files…",
        "Installing runtime");
    if(runtimeDependenciesSettled)
        showPhase("interface","Opening Social Interface",
            "Runtime dependencies are ready. Finishing Qt startup…",
            "Opening interface");
}
function observeEngine(url){
    if(!installing || typeof PerformanceObserver!=="function")return;
    try{
        wasmResourceObserver?.disconnect();
        wasmResourceObserver=new PerformanceObserver(list=>{
            if(list.getEntries().some(entry=>entry.name===url)) engineReceived(url);
        });
        wasmResourceObserver.observe({type:"resource",buffered:true});
        engineReceived(url);
    }catch(error){
        console.debug("[Genycaster] Resource Timing observer unavailable",error);
    }
}
// Don't wrap a large WASM response in a ReadableStream: replacing the native
// Response can disable the browser's optimized instantiateStreaming/module
// compilation cache and duplicates the work on every refresh.
window.fetch=async function(input,options) {
    let originalUrl;
    try { originalUrl=new URL(typeof input==="string" ? input : input.url,location.href); }
    catch { return originalFetch(input,options); }
    if(originalUrl.origin!==location.origin ||
       !/^farcaster_client(?:\.[0-9a-f]{20})?\.wasm$/.test(
           originalUrl.pathname.split("/").pop()))
        return originalFetch(input,options);
    const url=verifiedAsset && originalUrl.pathname.endsWith("/farcaster_client.wasm")
        ? verifiedAsset : originalUrl;
    const changed=url.href!==originalUrl.href;
    const nativeRequest=changed
        ? typeof input==="string" ? url.href : new Request(url.href,input)
        : input;
    const nativeOptions={
        ...options,
        // An immutable content-hashed URL may safely reuse an HTTP entry
        // even under emrun/localhost, where revalidation headers are minimal.
        ...(hashedWasm.test(url.pathname.split("/").pop())
            ? {cache:"force-cache",credentials:"omit"} : {})
    };
    if(installing)showPhase("download",
        updating?"Downloading Runtime Update":"Downloading Social Runtime",
        "Checking the browser cache and retrieving any missing engine files…",
        "Loading engine");
    else showPhase("initialize","Initializing Social Workspace",
        "Retrieving the WebAssembly engine from cache or network…",
        "Preparing runtime");
    const response=await originalFetch(nativeRequest,nativeOptions);
    if(!response.ok)return response;
    wasmResourceUrl=response.url||url.href;
    // The native Response remains untouched for streaming and code caching.
    // Installing is shown only after Resource Timing confirms completion.
    observeEngine(wasmResourceUrl);
    if(!installing)showPhase("initialize","Initializing Social Workspace",
        "WebAssembly engine available. Preparing the Qt interface…",
        "Initializing runtime");
    preparing();
    return response;
};
const screen=$("qt-screen");
function fit() {
    // Mobile browser chrome can change the *visible* viewport dynamically.
    // This keeps the fixed Qt footer above the actual bottom of the screen.
    const viewport=window.visualViewport;
    const height=viewport?viewport.height:window.innerHeight;
    screen.style.height=Math.max(180,Math.round(height))+"px";
}
window.addEventListener("resize",fit);
window.visualViewport?.addEventListener("resize",fit);
fit();
retry.addEventListener("click",()=>location.reload());
async function enableOfflineCache() {
    if(!window.isSecureContext || !navigator.serviceWorker)return;
    try {
        const version=window.__genycasterAssets?.version;
        const suffix=typeof version==="string" && /^[0-9a-f]{20}$/.test(version)
            ? "?v="+version : "";
        await navigator.serviceWorker.register("./genycaster-sw.js"+suffix);
        await navigator.serviceWorker.ready;
    } catch(error) {
        console.warn("[Genycaster] Offline asset cache unavailable",error);
    }
}
const cacheStartup=enableOfflineCache();
function fail(error) {
    if(finished||failed)return;
    failed=true;
    wasmResourceObserver?.disconnect();
    preparing();
    if(boot?.dataset)boot.dataset.phase="failed";
    track?.setAttribute?.("aria-valuetext","Startup failed");
    const detail=String(error?.message||error||"Unknown startup failure").slice(0,350);
    announce("Couldn't start Genycaster",
        "The application couldn't initialize. "+detail+" — check the browser console and retry.",
        "Start failed");
    retry.classList.add("visible");
    console.error("[Genycaster] startup failed",error);
}
async function start() {
    try {
        if(typeof qtLoad!=="function")
            throw new Error("qtloader.js is missing or did not load");
        // Qt 6.9+ generates an app-specific Emscripten factory:
        // <target>_entry. Qt 6.12 requires it in qt.entryFunction.
        // Loading farcaster_client.js alone does NOT select that factory.
        if(typeof window.farcaster_client_entry!=="function")
            throw new Error("farcaster_client.js did not expose farcaster_client_entry");
        if(!seen && window.isSecureContext && navigator.serviceWorker
            && !navigator.serviceWorker.controller) {
            // On first installation briefly allow the worker to activate
            // before the large WASM request. Otherwise it would miss that
            // initial download and need a second request next refresh.
            showPhase("download",
                updating?"Preparing Runtime Update":"Preparing First Download",
                "Setting up the browser's local cache before fetching the engine…",
                "Preparing cache");
            await Promise.race([
                cacheStartup,
                new Promise(resolve=>window.setTimeout(resolve,1800)),
            ]);
        }
        await qtLoad({
            // Emscripten/Qt 6.12 allows an origin-safe content-addressed
            // binary path while retaining the stable generated JS factory.
            locateFile:(file,prefix)=>{
                if(file==="farcaster_client.wasm" &&
                   typeof window.__genycasterAssets?.wasm==="string" &&
                   /^farcaster_client\.[0-9a-f]{20}\.wasm$/.test(window.__genycasterAssets.wasm))
                    return new URL(window.__genycasterAssets.wasm,location.href).href;
                return (prefix||"")+file;
            },
            qt:{
                entryFunction:window.farcaster_client_entry,
                containerElements:[screen],
                onLoaded:()=>{
                    if(failed)return;
                    wasmResourceObserver?.disconnect();
                    // Ready reflects Qt's actual initialization callback, not
                    // a fabricated timer or estimated download percentage.
                    if(boot?.dataset)boot.dataset.phase="ready";
                    track?.setAttribute?.("aria-valuenow","100");
                    track?.setAttribute?.("aria-valuetext","Ready");
                    bootPhase="ready";
                    finished=true;
                    const timing=wasmResourceUrl?wasmTiming(wasmResourceUrl):null;
                    // A worker-handled request is not proof of a cache hit.
                    // Keep native responses intact rather than injecting headers.
                    console.info("[Genycaster] Startup",JSON.stringify({
                        readyMs:Math.round(performance.now()-bootStarted),
                        file:wasmResourceUrl?new URL(wasmResourceUrl).pathname.split("/").pop():null,
                        resourceMs:timing?Math.round(timing.duration):null,
                        transferBytes:timing?.transferSize ?? null,
                        decodedBytes:timing?.decodedBodySize ?? null,
                        delivery:timing?.workerStart>0?"service-worker"
                            :timing?.transferSize===0 && timing?.decodedBodySize>0
                                ?"browser-cache":"network-or-unknown"
                    }));
                    try{
                        localStorage.setItem(key,"1");
                        if(currentVersion)localStorage.setItem(versionKey,currentVersion);
                    }catch{}
                    announce("Social Workspace Ready","Opening your social interface…","Ready");
                    fill.classList.add("measured");fill.style.width="100%";
                    percent.textContent="";
                    boot.classList.add("finished");
                    window.setTimeout(()=>{boot.hidden=true;},400);
                },
                onExit:exit=>{
                    if(!finished)fail(new Error(
                        "Qt exited during startup"+(exit?.text?" ("+String(exit.text).slice(0,150)+")":"")
                    ));
                },
            },
            monitorRunDependencies:remaining=>{
                if(finished)return;
                if(remaining===0){
                    runtimeDependenciesSettled=true;
                    if(!installing || bootPhase==="install" || bootPhase==="initialize")
                        showPhase("interface","Opening Social Interface",
                            "Qt dependencies are ready. Bringing up your workspace…",
                            "Opening interface");
                }else if(!installing){
                    showPhase("initialize","Initializing Social Workspace",
                        "Preparing WebAssembly dependencies and the Qt interface…",
                        "Initializing runtime");
                }else if(bootPhase==="install"){
                    showPhase("initialize","Initializing Social Workspace",
                        "WebAssembly is installed. Initializing Qt dependencies…",
                        "Initializing runtime");
                }
                // On first install do not show "completed download" until the
                // actual WASM resource finishes, even if Qt reports deps early.
            },
        });
    }catch(error){fail(error);}
}
start();
}());

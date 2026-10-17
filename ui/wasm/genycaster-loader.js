(function () {
"use strict";
const $ = id => document.getElementById(id);
const boot=$("genycaster-boot"), heading=$("boot-heading"), status=$("boot-status");
const stage=$("boot-stage"), fill=$("boot-fill"), percent=$("boot-percent"), retry=$("boot-retry");
let finished=false;
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
let seen=false;
try{seen=localStorage.getItem(key)==="1";}catch{}
announce(seen?"Opening Genycaster":"Setting up Genycaster",
    seen?"Checking your browser's cached application files…":
         "Preparing the application on this device for the first time…",
    seen?"Checking resources":"First launch");
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
    announce(seen?"Opening cached or updated engine":"Preparing application engine",
        "Reading the WebAssembly engine; checking browser cache…","Loading engine");
    const response=await originalFetch(nativeRequest,nativeOptions);
    if(!response.ok)return response;
    const cachedByWorker=response.headers.get("x-genycaster-cache")==="hit";
    const timing=wasmTiming(response.url||url.href);
    const cachedByHttp=!!timing && timing.transferSize===0
        && timing.decodedBodySize>0;
    const localCache=cachedByWorker||cachedByHttp;
    announce("Starting Genycaster",
        localCache?"Engine restored from local browser cache. Preparing Qt…"
                  :"WebAssembly resource available. Preparing Qt…",
        localCache?"Cached engine":"Initializing runtime");
    preparing();
    // Useful for deployed debugging; never logs account/secret information.
    console.info("[Genycaster] WASM resource",{
        file:url.pathname.split("/").pop(),
        cache:cachedByWorker?"service-worker":cachedByHttp?"browser":"not-confirmed",
        transferBytes:timing?.transferSize ?? null,
        decodedBytes:timing?.decodedBodySize ?? null
    });
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
    if(finished)return;
    preparing();
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
            announce("Preparing offline resources",
                "Installing this browser's cache for faster future visits…",
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
                    finished=true;
                    try{localStorage.setItem(key,"1");}catch{}
                    announce("Genycaster is ready","Entering your workspace…","Ready");
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
                if(remaining>0 && !finished && !fill.classList.contains("measured"))
                    announce("Preparing your workspace",
                        "Restoring local files and initializing the Qt runtime…",
                        "Initializing");
            },
        });
    }catch(error){fail(error);}
}
start();
}());

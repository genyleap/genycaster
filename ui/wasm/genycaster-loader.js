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
function measurable(loaded,total) {
    if(!(Number.isFinite(total)&&total>0))return;
    const value=Math.min(100,Math.round(100*loaded/total));
    fill.classList.add("measured");
    fill.style.width=value+"%";
    percent.textContent=value+"%";
    stage.textContent="Downloading · "+value+"%";
}
const key="genycaster.boot.loaded.v1";
let seen=false;
try{seen=localStorage.getItem(key)==="1";}catch{}
announce(seen?"Opening Genycaster":"Setting up Genycaster",
    seen?"Checking your browser's cached application files…":
         "Preparing the application on this device for the first time…",
    seen?"Checking resources":"First launch");
const originalFetch=window.fetch.bind(window);
// No pre-download, artificial countdown, or forced cache bypass.
// Observe only the real .wasm response being fetched by Emscripten.
window.fetch=async function(input,options) {
    const response=await originalFetch(input,options);
    let url;
    try{url=new URL(typeof input==="string"?input:input.url,location.href);}catch{return response;}
    if(!url.pathname.endsWith(".wasm")||!response.ok||!response.body)return response;
    const rawLength=Number(response.headers.get("content-length"));
    const encoded=response.headers.get("content-encoding");
    const total=encoded?0:rawLength;
    const cached=response.headers.get("x-genycaster-cache")==="hit";
    announce(cached?"Loading cached application":seen?"Loading application resources":"Downloading application resources",
        cached?"Restoring unchanged engine from this browser's local cache…":
            "Receiving the Qt WebAssembly engine…",cached?"Cached engine":"Downloading");
    const hasMeasuredTotal=Number.isFinite(total)&&total>0;
    if(!hasMeasuredTotal || cached)preparing();
    const reader=response.body.getReader();
    let loaded=0;
    const stream=new ReadableStream({
        async pull(controller) {
            try {
                const part=await reader.read();
                if(part.done) {
                    controller.close();
                    announce("Starting Genycaster",
                        "Application files received. Compiling WebAssembly and preparing Qt…",
                        "Initializing runtime");
                    preparing();
                    return;
                }
                loaded+=part.value.byteLength;
                if(hasMeasuredTotal && !cached)measurable(loaded,total);
                controller.enqueue(part.value);
            }catch(error){controller.error(error);}
        },
        cancel(reason){return reader.cancel(reason);}
    });
    const headers=new Headers(response.headers);
    headers.delete("content-length");
    // Fetch already yields decoded bytes, so don't claim they are encoded again.
    headers.delete("content-encoding");
    return new Response(stream,{status:response.status,statusText:response.statusText,headers});
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

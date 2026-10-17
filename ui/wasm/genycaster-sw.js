/* Public application assets only. Never cache user API responses or credentials. */
"use strict";
const CACHE="genycaster-static-v3";
const WASM_HASH=/^farcaster_client\.[0-9a-f]{20}\.wasm$/;
// Explicit allowlist: a same-origin .json/.html URL might contain personal
// account data. Never cache arbitrary paths based solely on file extension.
const PUBLIC_FILES=new Set([
    "farcaster_client.html","farcaster_client.js","qtloader.js",
    "farcaster_client.wasm","farcaster_client.worker.js","farcaster_client.data",
    "genycaster-loader.js","genycaster-assets.json","genycaster-assets.js",
    "genycaster-sw.js","genycaster.webmanifest","genycaster-icon.svg",
    "secure_signer_vault.mjs","farcaster_protocol.mjs",
    "browser_protocol_signer.mjs","browser_device_session.mjs",
    "farcaster_recovery.mjs","recovery-phrase-dialog.mjs",
    "vendor/noble-secp256k1.mjs","vendor/bip39-english.mjs",
]);
const scopePath=new URL("./",self.location.href).pathname;
const allowed=(url)=>url.origin===self.location.origin &&
    url.pathname.startsWith(scopePath) &&
    (WASM_HASH.test(url.pathname.slice(scopePath.length)) ||
     PUBLIC_FILES.has(url.pathname.slice(scopePath.length)));
const withoutQuery=(url)=>new URL(url.origin+url.pathname);
function asCached(response) {
    const headers=new Headers(response.headers);
    headers.set("x-genycaster-cache","hit");
    return new Response(response.body,{status:response.status,
        statusText:response.statusText,headers});
}
function cacheResponse(response){
    return response.ok && response.type!=="opaque" &&
        (!response.headers.get("cache-control")?.includes("no-store"));
}
self.addEventListener("install",event=>{
    event.waitUntil((async()=>{
        const cache=await caches.open(CACHE);
        // Do not force another .wasm download just for installation.
        // The initial application response is stored by the page loader;
        // subsequent releases use the content-hashed URL.
        const manifest=await fetch(new URL("./genycaster-assets.json",self.location.href),
            {cache:"no-store"});
        if(!manifest.ok)throw new Error("Missing public asset list");
        const assets=await manifest.json();
        if(!Array.isArray(assets.files) || assets.files.length>32)
            throw new Error("Invalid public asset manifest");
        await Promise.all(assets.files.filter(f=>!f.endsWith(".wasm")).map(async file=>{
            if(typeof file!=="string"||file.startsWith("/")||file.includes(".."))return;
            try{
                const url=new URL(file,self.location.href);
                const response=await fetch(url,{cache:"no-cache"});
                if(cacheResponse(response))await cache.put(url,response);
            }catch{ /* offline fallback may be incomplete until warm */ }
        }));
    })());
    self.skipWaiting();
});
self.addEventListener("activate",event=>{
    event.waitUntil((async()=>{
        await Promise.all((await caches.keys()).filter(key=>
            key.startsWith("genycaster-static-") && key!==CACHE)
            .map(key=>caches.delete(key)));
        await self.clients.claim();
    })());
});
self.addEventListener("fetch",event=>{
    const request=event.request;
    if(request.method!=="GET" || request.credentials==="include")return;
    const url=new URL(request.url);
    if(!allowed(url))return;
    const key=withoutQuery(url);
    event.respondWith((async()=>{
        const cache=await caches.open(CACHE);
        const saved=await cache.match(key.href);
        // Immutable content-addressed WASM: no re-download on repeat visits.
        if(WASM_HASH.test(key.pathname.slice(scopePath.length))&&saved)
            return asCached(saved);
        try{
            const response=await fetch(request);
            if(cacheResponse(response))await cache.put(key.href,response.clone());
            return response;
        }catch(error){
            // Offline fallback for previously cached public application files.
            if(saved)return asCached(saved);
            throw error;
        }
    })());
});

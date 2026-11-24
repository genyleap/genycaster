/* Public application assets only. Never cache user API responses or credentials. */
"use strict";
const CACHE="genycaster-static-v16";
const WASM_HASH=/^genycaster\.[0-9a-f]{20}\.wasm$/;
// Explicit allowlist: a same-origin .json/.html URL might contain personal
// account data. Never cache arbitrary paths based solely on file extension.
const PUBLIC_FILES=new Set([
    "genycaster.html","genycaster.js","qtloader.js",
    "genycaster.worker.js","genycaster.data",
    "genycaster-loader.js","genycaster-assets.json","genycaster-assets.js",
    "genycaster-sw.js","genycaster.webmanifest","genycaster-icon.svg",
    "genycaster-brand.svg","genycaster-wordmark.svg","genycaster-wordmark-dark.svg",
    "favicon.ico","favicon-16x16.png","favicon-32x32.png","favicon-48x48.png",
    "genycaster-192.png","genycaster-512.png",
    "genycaster-boot-icon.svg","intro-base.svg","intro-farcaster.svg","intro-hypersnap.svg","intro-openproof.svg","genycaster-update-icon.svg","apple-touch-icon.png",
    "apple-touch-icon-precomposed.png",
    "secure_signer_vault.mjs","farcaster_protocol.mjs",
    "browser_protocol_signer.mjs","browser_device_session.mjs","browser_account_session.mjs",
    "browser_media_upload.mjs","client_image_optimizer.mjs",
    // Deployment authority is never stored in offline caches.
    "farcaster_recovery.mjs","recovery-phrase-dialog.mjs",
    "vendor/noble-secp256k1.mjs","vendor/bip39-english.mjs",
    // Cached only as first-party static code; requests still need live
    // WalletConnect relay approval and per-origin wallet consent.
    "miniapp-host-bridge.mjs","genycaster-walletconnect.mjs",
]);
const scopePath=new URL("./",self.location.href).pathname;
const allowed=(url)=>url.origin===self.location.origin &&
    url.pathname.startsWith(scopePath) &&
    (WASM_HASH.test(url.pathname.slice(scopePath.length)) ||
     PUBLIC_FILES.has(url.pathname.slice(scopePath.length)));
const withoutQuery=(url)=>new URL(url.origin+url.pathname);
function cacheResponse(response){
    return response.ok && response.type!=="opaque" &&
        (!response.headers.get("cache-control")?.includes("no-store"));
}
self.addEventListener("install",event=>{
    event.waitUntil((async()=>{
        // Caching is an optional optimization, never a startup prerequisite.
        // A blocked request, offline browser, corrupt manifest or denied
        // CacheStorage must not reject the install event.
        try {
            const requestUrl=new URL("./genycaster-assets.json",self.location.href);
            const manifest=await fetch(requestUrl,{cache:"no-store"});
            if(!manifest.ok)throw new Error("Public asset manifest unavailable");
            const assets=await manifest.json();
            // Validate shape and individual entries; never infer public
            // caching eligibility merely from an arbitrary JSON manifest.
            if(!Array.isArray(assets.files) || assets.files.length>128)
                throw new Error("Invalid public asset manifest");
            const urls=[];
            const seen=new Set();
            for(const file of assets.files) {
                if(typeof file!=="string" || file.length>160 ||
                    file.startsWith("/") || file.includes("..") ||
                    file.includes("?") || file.includes("#"))continue;
                const url=new URL(file,requestUrl);
                if(!allowed(url) || WASM_HASH.test(file) || seen.has(url.href))continue;
                seen.add(url.href);
                urls.push(url);
            }
            const cache=await caches.open(CACHE);
            // A cold Qt launch simultaneously downloads a ~49 MiB WASM
            // engine. Do not compete with it using 50+ parallel background
            // image/module requests; use a small bounded install queue.
            let next=0;
            const precache=async()=>{
                while(next<urls.length) {
                    const url=urls[next++];
                    try{
                        const response=await fetch(url,{cache:"no-cache"});
                        if(cacheResponse(response))await cache.put(url,response);
                    }catch{ /* optional offline cache miss */ }
                }
            };
            await Promise.all(Array.from(
                {length:Math.min(2,urls.length)},()=>precache()));
        } catch(error) {
            console.warn("[GenyCaster] Offline pre-cache skipped",error);
        }
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
    // Never intercept large WebAssembly binaries. Safari/WebKit can abort the
    // streaming Response when the worker tees an ~80 MiB download into
    // CacheStorage, yielding FetchEvent.respondWith / TypeError: Load failed.
    // Let the native network/HTTP disk cache serve these immutable URLs.
    const relative=url.pathname.startsWith(scopePath)
        ? url.pathname.slice(scopePath.length) : "";
    if(relative==="genycaster.wasm" || WASM_HASH.test(relative))return;
    if(!allowed(url))return;
    const key=withoutQuery(url);
    let cacheWrite=Promise.resolve();
    const delivery=(async()=>{
        let cache;
        let saved;
        try {
            cache=await caches.open(CACHE);
            saved=await cache.match(key.href);
        } catch(error) {
            // Caching is optional: blocked storage must not prevent startup.
            cache=null;
            console.warn("[Genycaster] Static cache unavailable",error);
        }
        // WebAssembly is never handled here; all other public assets can
        // still fall back to an offline cache when available.
        try{
            // Mutable JS modules (including the signing vault) must be
            // revalidated against the current deployment after an app update;
            // stale browser HTTP cache must not retain old authorization code.
            const response=await fetch(request,{cache:"no-cache"});
            if(cache && cacheResponse(response)) {
                // Do not hold streaming compilation behind the full download
                // and disk write. waitUntil keeps the background write alive.
                cacheWrite=Promise.resolve().then(()=>cache.put(key.href,response.clone()))
                    .catch(error=>console.warn("[Genycaster] Static cache write failed",error));
            }
            return response;
        }catch(error){
            // Offline fallback for previously cached public application files.
            if(saved)return saved;
            throw error;
        }
    })();
    event.respondWith(delivery);
    event.waitUntil(delivery.then(()=>cacheWrite).catch(()=>{}));
});

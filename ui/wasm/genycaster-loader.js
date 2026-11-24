(function () {
"use strict";
const $ = id => document.getElementById(id);
const boot=$("genycaster-boot"), heading=$("boot-heading"), status=$("boot-status");
const stage=$("boot-stage"), fill=$("boot-fill"), percent=$("boot-percent"), retry=$("boot-retry");
const track=$("boot-track");
// Browser chrome is separate from the Qt canvas. Match the actual Qt theme
// after initialization; HTML defaults dark until the Qt bridge is ready.
// The installed web app's status bar follows the theme when the host allows it.
// Matching the Theme.qml accent presets (without guessing from browser CSS).
// Qt sends its effective color theme on each appearance change. Older WASM
// binaries may send dark alone; they retain a coarse OS-accent fallback.
const chromePalette={
    geny:["#315CF5","#829AFF"],ocean:["#1289A8","#63C6DE"],
    violet:["#7047EB","#AB91FF"],ember:["#C95B35","#F39A77"],
    rose:["#D83E8A","#FF79B7"],mint:["#0F8F78","#61D7BD"],
    mono:["#17181B","#F4F5F8"]
};
function browserPlatformAccent(){
    const platform=String(navigator.userAgentData?.platform||navigator.platform||"");
    const userAgent=String(navigator.userAgent||"");
    if(/Android/i.test(platform+" "+userAgent))return "#3DDC84";
    if(/Mac|iPad|iPhone/i.test(platform+" "+userAgent))return "#007AFF";
    if(/Win/i.test(platform+" "+userAgent))return "#0078D4";
    if(/Linux/i.test(platform+" "+userAgent))return "#3584E4";
    return "#315CF5";
}
function applyBrowserChromeTheme(dark, themeId="platform", customAccent="", platformAccent="", remember=true) {
    const color=dark?"#0b0c0f":"#f1f1f3";
    const root=document.documentElement;
    root.setAttribute("data-chrome-theme",dark?"dark":"light");
    const meta=document.querySelector('meta[name="theme-color"]');
    if(meta)meta.setAttribute("content",color);
    const index=dark?1:0;
    const validHex=value=>typeof value==="string"&&/^#[a-fA-F0-9]{6}$/.test(value);
    const accent=themeId==="custom"&&validHex(customAccent)?customAccent:
        themeId==="platform"?(validHex(platformAccent)?platformAccent:browserPlatformAccent()):
        Object.prototype.hasOwnProperty.call(chromePalette,themeId)?
            chromePalette[themeId][index]:browserPlatformAccent();
    root.style.setProperty("--nd-accent",accent);
    // Pick whichever contrast meets WCAG more comfortably. Bright Night
    // presets such as #829AFF need dark lettering; a raw sRGB average would
    // incorrectly leave white text on their lightly saturated backgrounds.
    const rgb=parseInt(accent.slice(1),16);
    const linear=v=>{
        const channel=v/255;
        return channel<=0.04045?channel/12.92:Math.pow((channel+0.055)/1.055,2.4);
    };
    const luminance=0.2126*linear((rgb>>16)&255)
                   +0.7152*linear((rgb>>8)&255)
                   +0.0722*linear(rgb&255);
    const darkContrast=(luminance+0.05)/0.05;
    const lightContrast=1.05/(luminance+0.05);
    root.style.setProperty("--nd-on-accent",darkContrast>=lightContrast?"#17181B":"#FFFFFF");
    if(remember){
        try { localStorage.setItem("genycaster.browser.chrome.v1",dark?"dark":"light"); }
        catch (_) { /* optional appearance cache, never block Qt startup */ }
    }
}
window.__genycasterSetBrowserChromeTheme=applyBrowserChromeTheme;
// Keep the early, OS- or user-selected appearance until Qt reports its real
// theme. Forcing dark here can paint a dark status bar over light pages.
applyBrowserChromeTheme(
    document.documentElement.getAttribute?.("data-chrome-theme") !== "light", undefined, undefined, undefined, false);
let finished=false, failed=false;
let qtRuntimeLoaded=false, qmlRootReady=false, accountRestoreSettled=false;
// A provisional browser Guest state must not expose Setup before the
// remembered-account WebCrypto challenge has returned its terminal result.
window.__genycasterBrowserAuthBootstrap=()=>{
    accountRestoreSettled=true;
    revealReady();
};
const studioIntro=$("genycaster-studio-intro");
let studioComplete=true;
let splashMinimumReached=true;
// Three readable phases on every refresh, independently of WASM cache speed.
// A brief studio credit, a longer partner reveal, then visible Genycaster branding.
const studioSceneMs=2700;
const studioPartnersMs=5500;
const splashSceneMs=2600;
const bootStarted=performance.now();
let wasmResourceUrl;
let wasmResourceObserver=null;
let runtimeDependenciesSettled=false;
let progressReader=null;
let bootPhase="download";
function announce(title,detail,phase) {
    heading.textContent=title;
    status.textContent=detail;
    stage.textContent=phase;
}
function downloadProgressText(received,total,pct){
    if(total<1048576)return pct+"%";
    return pct+"% · "+(received/1048576).toFixed(1)+" / "+(total/1048576).toFixed(1)+" MiB";
}
function preparing() {
    fill.classList.remove("measured");
    fill.style.width="";
    percent.textContent="";
    track?.removeAttribute?.("aria-valuenow");
}
// Only a first-run/update and a reliable decoded Content-Length permit a
// byte-accurate percentage. We consume a clone concurrently but pass the
// *original* native Response to Emscripten, preserving streaming compilation.
// Warm refreshes never clone the large binary.
const key="genycaster.boot.loaded.v1";
const versionKey="genycaster.boot.version.v1";
const currentVersion=typeof window.__genycasterAssets?.version==="string"
    ? window.__genycasterAssets.version : "";
const releaseInfo=window.__genycasterAssets||{};
// Exposed for diagnosis without accessing any identity, draft, or wallet data.
window.__genycasterActiveBuild=String(releaseInfo.buildId||"");
const applicationVersion=typeof releaseInfo.appVersion==="string"
    && /^\d+\.\d+\.\d+(?:[-+][\w.-]+)?$/.test(releaseInfo.appVersion)
    ? releaseInfo.appVersion : "";
const buildId=typeof releaseInfo.buildId==="string"
    && /^[0-9a-f]{8}$/.test(releaseInfo.buildId)
    && currentVersion.startsWith(releaseInfo.buildId)
    ? releaseInfo.buildId : "";
// Product, numeric version and release channel arrive independently from CMake.
const numericVersion=typeof releaseInfo.versionNumber==="string"
    && /^\d+\.\d+\.\d+$/.test(releaseInfo.versionNumber)
    ? releaseInfo.versionNumber:"";
const releaseChannel=["stable","alpha","beta","rc"].includes(releaseInfo.releaseChannel)
    ? releaseInfo.releaseChannel:"";
const releaseChannelNumber=Number.isInteger(releaseInfo.releaseChannelNumber)
    ? releaseInfo.releaseChannelNumber:0;
const buildLabel=(applicationVersion?"v"+applicationVersion:"")
    +(buildId?(applicationVersion?" · ":"")+"Build "+buildId:"");
const creditBuild=$("boot-build-credit");
if(creditBuild)creditBuild.textContent=buildLabel;
let seen=false, previousVersion="";
try{
    seen=localStorage.getItem(key)==="1";
    previousVersion=localStorage.getItem(versionKey)||"";
}catch{}
const updating=seen && !!currentVersion && !!previousVersion && previousVersion!==currentVersion;
const installing=!seen || updating;
bootPhase=installing?"download":"initialize";
// Studio credits are not progress simulation: run on EVERY document load,
// including ordinary refreshes with cached WASM. Actual progress remains
// independently measured; Qt readiness cannot cut the credits short.
if (studioIntro && boot) {
    const earlyStart=Number(window.__genycasterIntroStartedAt);
    const startedAt=Number.isFinite(earlyStart) && earlyStart>0
        ? earlyStart : performance.now();
    studioComplete=false;
    splashMinimumReached=false;
    studioIntro.hidden=false;
    boot.dataset.intro='on';
    const elapsed=Math.max(0,performance.now()-startedAt);
    studioIntro.dataset.scene=elapsed>=studioSceneMs ? 'partners' : 'studio';
    window.setTimeout(()=>{
        if (!failed && !finished && !studioComplete)
            studioIntro.dataset.scene='partners';
    },Math.max(0,studioSceneMs-elapsed));
    window.setTimeout(()=>{
        if (studioComplete) return;
        studioComplete=true;
        studioIntro.hidden=true;
        boot.dataset.intro='off';
        // Keep the actual Genycaster splash visible for its own full beat.
        // This is presentation gating only: downloaded-byte progress and
        // Qt's real readiness remain unchanged.
        window.setTimeout(()=>{
            splashMinimumReached=true;
            if(qtRuntimeLoaded && qmlRootReady) revealReady();
        },splashSceneMs);
    },Math.max(0,studioSceneMs+studioPartnersMs-elapsed));
}
if(boot?.dataset){
    boot.dataset.mode=installing?"install":"warm";
    boot.dataset.phase=bootPhase;
}
function showPhase(phase,title,detail,label){
    if(finished||failed)return;
    const order={download:0,install:1,initialize:2,interface:3,ready:4};
    if(order[phase] < order[bootPhase])return; // delayed async event
    const leavingDownload=bootPhase==="download" && phase!=="download";
    bootPhase=phase;
    if(boot?.dataset)boot.dataset.phase=phase;
    if(leavingDownload)preparing();
    // Preserve the real loader state machine for first install, updates and
    // warm starts, but expose only one human-readable status to the user.
    const display={
        download:[updating?"Updating Software…":"Installing Software…",
                  "Downloading application resources"],
        install:["Verifying Software…","Preparing WebAssembly resources"],
        initialize:["Initializing Runtime…","Initializing Qt and WebAssembly"],
        interface:["Opening Genycaster…","Almost ready"],
        ready:["Genycaster is ready","Ready"]
    }[phase];
    const [headline,progressLabel]=display;
    track?.setAttribute?.("aria-valuetext",progressLabel);
    announce(headline,detail,progressLabel);
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
// WKWebView and Safari have stricter cache/service-worker semantics for large
// streaming WASM bodies. Keep browser HTTP caching, but disable app worker
// interception on WebKit until this path can be tested on those engines.
const isWebKitRuntime=typeof navigator!=="undefined"
    && /AppleWebKit/i.test(navigator.userAgent||"")
    && !/Chrome\/|Chromium\/|Edg\/|OPR\//i.test(navigator.userAgent||"");
const hashedWasm=/^genycaster\.[0-9a-f]{20}\.wasm$/;
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
function measureFirstDownload(response,url){
    if(!installing || !response.body || typeof response.clone!=="function")return;
    // Browser CacheStorage, proxies and compression may suppress or change
    // Content-Length. The immutable, hashed WASM's *decoded* byte size comes
    // from our own release packager, so we can measure the native stream even
    // behind Cloudflare or a Service Worker without guessing.
    const manifestBytes=Number(releaseInfo.wasmBytes);
    // Never tee a large WASM stream: the original stream must remain available
    // for progressive compilation without buffering a second 80+ MiB copy.
    if(Number.isFinite(manifestBytes) && manifestBytes>16*1024*1024)return;
    const trusted=verifiedAsset && url===verifiedAsset.href
        && Number.isSafeInteger(manifestBytes) && manifestBytes>0;
    const headersLength=Number(response.headers.get("content-length"));
    const hasPlainLength=!response.headers.get("content-encoding")
        && Number.isSafeInteger(headersLength) && headersLength>0;
    const total=trusted?manifestBytes:(hasPlainLength?headersLength:0);
    if(!total)return;
    try{
        progressReader=response.clone().body.getReader();
    }catch{return;}
    const reader=progressReader;
    let received=0,lastPaint=0;
    void (async()=>{
        try{
            while(!finished && !failed){
                const part=await reader.read();
                if(part.done){
                    if(received===total && bootPhase==="download"){
                        percent.textContent="100%";
                        fill.classList.add("measured");
                        fill.style.width="100%";
                        // The full stream completed; this is a real event
                        // even when Resource Timing is unavailable.
                        showPhase("install",updating?"Applying Runtime Update":
                            "Installing Social Runtime",
                            "The WebAssembly engine is available. Preparing local files…",
                            "Installing runtime");
                        if(runtimeDependenciesSettled)
                            showPhase("interface","Opening Social Interface",
                                "Runtime dependencies are ready. Finishing Qt startup…",
                                "Opening interface");
                    }
                    break;
                }
                received+=part.value.byteLength;
                if(bootPhase!=="download")break;
                const now=performance.now();
                if(received===total || now-lastPaint>=90){
                    lastPaint=now;
                    const pct=Math.min(99,Math.floor(received*100/total));
                    fill.classList.add("measured");
                    fill.style.width=pct+"%";
                    percent.textContent=downloadProgressText(received,total,pct);
                    track?.setAttribute?.("aria-valuenow",String(pct));
                    track?.setAttribute?.("aria-valuetext",
                        "Downloading engine "+pct+" percent");
                }
            }
        }catch(error){
            // The original WASM Response remains viable even if progress
            // tracking is unsupported by a browser implementation.
            console.debug("[Genycaster] Optional byte progress stopped",
                String(error?.message||error));
        }finally{
            if(progressReader===reader)progressReader=null;
            try{reader.releaseLock();}catch{}
        }
    })();
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
// Download the immutable gzip sidecar on every browser with streaming gzip
// decompression (Safari, Chromium, Firefox and Edge). The existing WebKit-only
// gate made Chromium/Firefox download the much larger WASM resource instead.
// Transform the ONE compressed stream without teeing or buffering a second
// full-sized copy; Qt receives a streaming application/wasm Response.
// Browsers without this API fall back to the native HTTP/wasm fetch path.
async function compressedBrowserEngine(url,options) {
    const name=url.pathname.split("/").pop();
    const gzipName=releaseInfo.wasmGzip;
    const gzipBytes=Number(releaseInfo.wasmGzipBytes);
    if(typeof DecompressionStream!=="function"
       || typeof TransformStream!=="function"
       || !hashedWasm.test(name) || gzipName!==name+".gz"
       || !Number.isSafeInteger(gzipBytes) || gzipBytes<=0)return null;
    const gzipUrl=new URL(gzipName,location.href);
    // A 49 MiB gzip sidecar is immutable because its filename embeds the
    // actual WASM SHA-256 prefix. A change to UI/JS must not create a fresh
    // 49 MiB cache key; use the binary fingerprint, not the release manifest's
    // mutable version. Browser disk cache can then serve repeat visits with
    // no conditional network request.
    const releaseToken=name.match(/^genycaster\.([0-9a-f]{20})\.wasm$/)?.[1] || "";
    if(releaseToken)gzipUrl.searchParams.set("v",releaseToken);
    let source;
    try{
        source=await originalFetch(gzipUrl.href,{...options,cache:"force-cache"});
    }catch(error){
        console.warn("[Genycaster] Compressed WASM unavailable; using native fetch",
            String(error?.message||error));
        return null;
    }
    if(!source.ok || !source.body || /text\/html/i.test(source.headers.get("content-type")||""))
        return null;
    let received=0, lastPaint=0;
    const wireBytes=source.headers.get("content-encoding")?0
        : Number(source.headers.get("content-length"));
    // The release packager knows the exact compressed byte count. CDNs
    // commonly omit Content-Length for streamed or encoded responses.
    const measure=installing && (!wireBytes || wireBytes===gzipBytes);
    const progressTotal=gzipBytes;
    const compressed=source.body.pipeThrough(new TransformStream({
        transform(chunk,controller){
            received+=chunk.byteLength;
            if(measure && !finished && !failed && bootPhase==="download"){
                const now=performance.now();
                if(now-lastPaint>120 || received>=wireBytes){
                    lastPaint=now;
                    const pct=Math.min(99,Math.floor(received*100/progressTotal));
                    fill.classList.add("measured");
                    fill.style.width=pct+"%";
                    percent.textContent=downloadProgressText(received,progressTotal,pct);
                    track?.setAttribute?.("aria-valuenow",String(pct));
                }
            }
            controller.enqueue(chunk);
        },
        flush(){
            if(!failed && !finished && installing){
                showPhase("install","Verifying compressed engine",
                    "Application resources downloaded; preparing WebAssembly...",
                    "Preparing runtime");
            }
        }
    }));
    return {response:new Response(
        compressed.pipeThrough(new DecompressionStream("gzip")),
        {headers:{"Content-Type":"application/wasm"}}),
        resourceUrl:gzipUrl.href};
}
window.fetch=async function(input,options) {
    let originalUrl;
    try { originalUrl=new URL(typeof input==="string" ? input : input.url,location.href); }
    catch { return originalFetch(input,options); }
    if(originalUrl.origin!==location.origin ||
       !/^genycaster(?:\.[0-9a-f]{20})?\.wasm$/.test(
           originalUrl.pathname.split("/").pop()))
        return originalFetch(input,options);
    const url=verifiedAsset && originalUrl.pathname.endsWith("/genycaster.wasm")
        ? verifiedAsset : originalUrl;
    const changed=url.href!==originalUrl.href;
    const nativeRequest=changed
        ? typeof input==="string" ? url.href : new Request(url.href,input)
        : input;
    // Prefer the immutable .wasm.gz sidecar with streamed decoding across
    // supported browsers; native HTTP/wasm remains the compatibility fallback.
    // Neither path clones the large decoded engine or stores private data.
    // If native fetch fails, retry once with a full identity Range on WebKit
    // to bypass intermediaries that transform WASM response bodies.
    const nativeOptions={...options};
    const useIdentityRange=isWebKitRuntime
        && hashedWasm.test(url.pathname.split("/").pop());
    let attemptedIdentityRange=false;
    if(installing)showPhase("download",
        updating?"Downloading Runtime Update":"Downloading Social Runtime",
        "Checking the browser cache and retrieving any missing engine files…",
        "Loading engine");
    else showPhase("initialize","Initializing Social Workspace",
        "Retrieving the WebAssembly engine from cache or network…",
        "Preparing runtime");
    let response;
    let compressedPath=false;
    try {
        const zipped=await compressedBrowserEngine(url,nativeOptions);
        if(zipped){
            response=zipped.response;
            compressedPath=true;
            wasmResourceUrl=zipped.resourceUrl;
        }else{
            response=await originalFetch(nativeRequest,nativeOptions);
        }
    } catch(error) {
        // A corrupted/blocked old HTTP or SW-cached WASM should not make
        // the whole Qt runtime fail. One network revalidation bypass is
        // permitted, never a retry loop and never a private-data reset.
        if(!hashedWasm.test(url.pathname.split("/").pop()))throw error;
        console.warn("[Genycaster] Retrying WASM with fresh HTTP fetch",
            String(error?.message||error));
        const retryOptions={...nativeOptions,cache:"reload"};
        if(useIdentityRange){
            const headers=new Headers(options?.headers
                || (typeof input==="string" ? undefined : input.headers));
            headers.set("Range","bytes=0-");
            retryOptions.headers=headers;
            attemptedIdentityRange=true;
        }
        response=await originalFetch(nativeRequest,retryOptions);
    }
    if(!response.ok)return response;
    if(attemptedIdentityRange && response.status===206){
        const range=response.headers.get("content-range")||"";
        const match=/^bytes 0-([0-9]+)\/([0-9]+)$/.exec(range);
        const expected=Number(releaseInfo.wasmBytes);
        if(!match || Number(match[1])+1!==Number(match[2])
           || (Number.isSafeInteger(expected) && expected>0
               && Number(match[2])!==expected))
            throw new Error("Incomplete WebAssembly range response");
    }
    wasmResourceUrl=wasmResourceUrl||response.url||url.href;
    // The native Response remains untouched for streaming and code caching.
    // Installing is shown only after Resource Timing confirms completion.
    observeEngine(wasmResourceUrl);
    if(!installing)showPhase("initialize","Initializing Social Workspace",
        "WebAssembly engine available. Preparing the Qt interface…",
        "Initializing runtime");
    preparing();
    // Observe a large native WASM download on the SAME stream consumed by Qt.
    // No clone/tee or extra request: progressive WebAssembly compilation stays
    // possible and the browser owns the network cache as before.
    const totalBytes=Number(releaseInfo.wasmBytes);
    const directEngine=verifiedAsset && url.href===verifiedAsset.href;
    if(installing && !compressedPath && directEngine && response.body
        && typeof TransformStream==="function"
        && Number.isSafeInteger(totalBytes) && totalBytes>16*1024*1024){
        let received=0,lastPaint=0;
        const measured=response.body.pipeThrough(new TransformStream({
            transform(chunk,controller){
                received+=chunk.byteLength;
                if(!finished && !failed && bootPhase==="download"){
                    const now=performance.now();
                    if(now-lastPaint>=90 || received>=totalBytes){
                        lastPaint=now;
                        const pct=Math.min(99,Math.floor(received*100/totalBytes));
                        fill.classList.add("measured");
                        fill.style.width=pct+"%";
                        percent.textContent=downloadProgressText(received,totalBytes,pct);
                        track?.setAttribute?.("aria-valuenow",String(pct));
                    }
                }
                controller.enqueue(chunk);
            },
            flush(){
                if(!finished && !failed && bootPhase==="download"
                    && received===totalBytes){
                    showPhase("install","Verifying WebAssembly resources",
                        "Application resources downloaded; preparing runtime…",
                        "Preparing runtime");
                }
            }
        }));
        // Fetch has already decoded Content-Encoding; a rewrapped response
        // must not claim that its decoded stream is still gzip encoded or
        // that the original wire Content-Length applies to it.
        const headers=new Headers(response.headers);
        headers.delete("content-encoding");
        headers.delete("content-length");
        return new Response(measured,{
            status:response.status,statusText:response.statusText,
            headers
        });
    }
    measureFirstDownload(response,wasmResourceUrl);
    return response;
};
const screen=$("qt-screen");
function fit() {
    // A browser can pan the visual viewport to the focused text input without
    // scrolling the layout viewport. Keep the Qt canvas within that *visible*
    // rectangle, not at document y=0 behind the keyboard.
    const viewport=window.visualViewport;
    const height=viewport?viewport.height:window.innerHeight;
    const top=viewport?viewport.pageTop:window.scrollY;
    const left=viewport?viewport.pageLeft:window.scrollX;
    screen.style.top=Math.round(top||0)+"px";
    screen.style.left=Math.round(left||0)+"px";
    screen.style.height=Math.max(180,Math.round(height))+"px";
}
window.addEventListener("resize",fit);
window.addEventListener("scroll",fit,{passive:true});
window.visualViewport?.addEventListener("resize",fit);
window.visualViewport?.addEventListener("scroll",fit);
fit();
retry.addEventListener("click",()=>location.reload());
async function enableOfflineCache() {
    if(!window.isSecureContext || typeof navigator==="undefined" || !navigator.serviceWorker)return false;
    if(isWebKitRuntime){
        try {
            const registration=await navigator.serviceWorker.getRegistration(location.href);
            if(registration)await registration.unregister();
            // A former controller stays attached to an already-open tab
            // after unregister(). Navigate exactly once to obtain an
            // uncontrolled page before Qt fetches the large WASM engine.
            if(navigator.serviceWorker.controller){
                const marker="genycaster.webkit.network-boot.v1";
                const stamp=currentVersion||"unknown";
                if(window.sessionStorage.getItem(marker)!==stamp){
                    window.sessionStorage.setItem(marker,stamp);
                    const next=new URL(location.href);
                    next.searchParams.set("genycaster-network-boot",stamp);
                    location.replace(next.href);
                    return true;
                }
            }
        }catch(error){
            console.warn("[Genycaster] WebKit native-network preparation unavailable",error);
        }
        return false;
    }
    try {
        const version=window.__genycasterAssets?.version;
        const suffix=typeof version==="string" && /^[0-9a-f]{20}$/.test(version)
            ? "?v="+version : "";
        await navigator.serviceWorker.register("./genycaster-sw.js"+suffix);
        await navigator.serviceWorker.ready;
    } catch(error) {
        console.warn("[Genycaster] Offline asset cache unavailable",error);
    }
    return false;
}
const cacheStartup=enableOfflineCache();
// An already-running PWA keeps its old Qt/WASM instance even when a new build
// is served. Only notify on a confirmed, public manifest mismatch; never
// reload a composing user without their explicit tap.
const updateBanner=$("genycaster-update-banner");
const updateBackdrop=$("genycaster-update-backdrop");
const updateNow=$("genycaster-update-now");
const updateLater=$("genycaster-update-later");
const updateTextByLanguage = {"ar":["يتوفر تحديث لجنيكاستر","إصدار جديد جاهز للمراجعة.","الإصدار الحالي","الإصدار الجديد","ملاحظات الإصدار","لم تُنشر ملاحظات لهذا الإصدار. راجع صفحة الإصدار الرسمية للتفاصيل.","ليس الآن","تحديث الصفحة"],"de":["Genycaster-Update verfügbar","Eine neuere Version ist verfügbar.","Aktuelle Version","Neue Version","Versionshinweise","Für dieses Release wurden keine Änderungsnotizen veröffentlicht. Details stehen auf der offiziellen Release-Seite.","Nicht jetzt","Zum Aktualisieren neu laden"],"en":["Genycaster update available","A newer release is ready to review.","Current version","New version","Release notes","This release has no published change notes. View the official release for details.","Not now","Refresh to update"],"es":["Actualización de Genycaster disponible","Hay una nueva versión lista para revisar.","Versión actual","Versión nueva","Notas de la versión","No hay notas publicadas para esta versión. Consulta el lanzamiento oficial para más detalles.","Ahora no","Actualizar página"],"fa":["به‌روزرسانی جنی‌کستر آماده است","نسخهٔ تازه برای بررسی آماده است.","نسخهٔ فعلی","نسخهٔ جدید","یادداشت‌های انتشار","برای این نسخه یادداشت تغییراتی منتشر نشده است. برای جزئیات به صفحهٔ رسمی انتشار مراجعه کنید.","فعلاً نه","بازبارگذاری و به‌روزرسانی"],"fr":["Mise à jour de Genycaster disponible","Une nouvelle version est disponible.","Version actuelle","Nouvelle version","Notes de version","Aucune note de modification publiée pour cette version. Consultez la page officielle pour plus de détails.","Pas maintenant","Actualiser pour mettre à jour"],"hi":["Genycaster अपडेट उपलब्ध है","नया संस्करण देखने के लिए तैयार है।","वर्तमान संस्करण","नया संस्करण","रिलीज़ नोट्स","इस रिलीज़ के बदलाव प्रकाशित नहीं किए गए हैं। विवरण के लिए आधिकारिक रिलीज़ पृष्ठ देखें।","अभी नहीं","अपडेट के लिए रीफ़्रेश करें"],"ja":["Genycaster の更新があります","新しいバージョンを確認できます。","現在のバージョン","新しいバージョン","リリースノート","このリリースの更新履歴は公開されていません。詳細は公式リリースページをご覧ください。","後で","再読み込みして更新"],"ko":["Genycaster 업데이트 이용 가능","새 버전을 확인할 수 있습니다.","현재 버전","새 버전","릴리스 정보","이 릴리스에는 게시된 변경 사항이 없습니다. 공식 릴리스 페이지에서 자세한 내용을 확인하세요.","나중에","새로고침하여 업데이트"],"pt":["Atualização do Genycaster disponível","Uma nova versão está pronta para avaliação.","Versão atual","Nova versão","Notas da versão","Este lançamento não tem notas de mudanças publicadas. Consulte a página oficial para detalhes.","Agora não","Atualizar página"],"ru":["Доступно обновление Genycaster","Новая версия готова к просмотру.","Текущая версия","Новая версия","Примечания к выпуску","Для этой версии нет опубликованных примечаний. Подробности — на официальной странице релиза.","Не сейчас","Обновить страницу"],"zh":["Genycaster 有可用更新","新版本已准备好供查看。","当前版本","新版本","版本说明","此版本尚未发布更新说明。详情请查看官方发布页面。","稍后","刷新并更新"]};
let updateLanguage="en";
// Display-only native digit shaping: release identifiers, hashes and network
// metadata stay ASCII, while every visible update/version number follows UI language.
function localizedUpdateDigits(value){
    const alphabet=updateLanguage==="fa"?"۰۱۲۳۴۵۶۷۸۹"
        :updateLanguage==="ar"?"٠١٢٣٤٥٦٧٨٩":"";
    const valueText=String(value ?? "");
    return alphabet?valueText.replace(/[0-9]/g,digit=>alphabet[Number(digit)]):valueText;
}
let latestReleaseDetails=null;
let availableRelease="", updateCheckRunning=false, lastCheck=0, dismissedUntil=0;
function activeUpdateText(){
    return updateTextByLanguage[updateLanguage] || updateTextByLanguage.en;
}
function setUpdateLanguage(languageTag){
    const tag=String(languageTag||"en").toLowerCase().split(/[-_]/)[0];
    updateLanguage=Object.prototype.hasOwnProperty.call(updateTextByLanguage,tag)?tag:"en";
    const t=activeUpdateText();
    const ids=["genycaster-update-title","genycaster-update-hint",
        "genycaster-update-current-label","genycaster-update-next-label",
        "genycaster-update-details-title"];
    for(let i=0;i<ids.length;i++){
        const node=$(ids[i]);
        if(node)node.textContent=t[i];
    }
    if(updateLater)updateLater.textContent=t[6];
    if(updateNow)updateNow.textContent=t[7];
    if(updateBanner)updateBanner.dir=(updateLanguage==="fa"||updateLanguage==="ar")?"rtl":"ltr";
    if(creditBuild)creditBuild.textContent=localizedUpdateDigits(buildLabel);
    if(latestReleaseDetails)fillReleaseDetails(latestReleaseDetails);
}
window.__genycasterSetUpdateLanguage=setUpdateLanguage;
setUpdateLanguage(window.__genycasterPreferredLanguage||window.navigator?.language||"en");
function fillReleaseDetails(fresh){
    latestReleaseDetails=fresh;
    const current=$("genycaster-update-current");
    const latest=$("genycaster-update-next");
    const oldChannel=$("genycaster-update-current-channel");
    const newChannel=$("genycaster-update-next-channel");
    const newAppVersion=typeof fresh.appVersion==="string"
        && /^\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)[1-9]\d*)?$/.test(fresh.appVersion)
        ? fresh.appVersion:"";
    const nextFingerprint=typeof fresh.version==="string"
        && /^[0-9a-f]{20}$/.test(fresh.version) ? fresh.version:"";
    const nextBuild=typeof fresh.buildId==="string"
        && /^[0-9a-f]{8}$/.test(fresh.buildId)
        && nextFingerprint.startsWith(fresh.buildId)
        ? fresh.buildId : nextFingerprint.slice(0,8);
    const currentBuild=buildId||currentVersion.slice(0,8);
    const sameAppVersion=!!applicationVersion && newAppVersion===applicationVersion;
    // When the semver is unchanged, the build fingerprint is the upgrade.
    // Never show "1.0.0 → 1.0.0" as though that were a new product version.
    const primaryOld=sameAppVersion && currentBuild
        ? "Build "+currentBuild : "v"+(applicationVersion||numericVersion||"—");
    const primaryNew=sameAppVersion && nextBuild
        ? "Build "+nextBuild : "v"+(newAppVersion||fresh.versionNumber||"—");
    if(current)current.textContent=localizedUpdateDigits(primaryOld);
    if(latest)latest.textContent=localizedUpdateDigits(primaryNew);
    if(oldChannel)oldChannel.textContent=localizedUpdateDigits(
        sameAppVersion ? "v"+applicationVersion : (currentBuild?"Build "+currentBuild:""));
    if(newChannel)newChannel.textContent=localizedUpdateDigits(
        sameAppVersion ? "v"+newAppVersion : (nextBuild?"Build "+nextBuild:""));
    const releaseNotes=fresh.releaseNotes;
    const translations=releaseNotes && typeof releaseNotes==="object"
        ? releaseNotes.translations:null;
    const locale=translations && Object.keys(translations).find(tag=>
        tag.split("-")[0].toLowerCase()===updateLanguage);
    const localized=locale?translations[locale]:translations?.["en-US"];
    const changeList=Array.isArray(releaseNotes?.changes)?releaseNotes.changes:[];
    const bullets=localized?.items ? changeList
        .map(change=>localized.items[change.id])
        .filter(item=>typeof item==="string" && item.length>0)
        .slice(0,12):[];
    const summary=typeof localized?.summary==="string"
        ? localized.summary.slice(0,500) : "";
    const translatedTitle=$("genycaster-update-details-title");
    if(translatedTitle && localized?.title)translatedTitle.textContent=localized.title;
    const bytes=Number(fresh.wasmBytes);
    const gzipBytes=Number(fresh.wasmGzipBytes);
    const supportsGzip=typeof DecompressionStream==="function"
        && typeof TransformStream==="function";
    const downloadBytes=supportsGzip && Number.isSafeInteger(gzipBytes) && gzipBytes>0
        ? gzipBytes : bytes;
    // Show expected network transfer, not the expanded in-memory engine size.
    const size=Number.isSafeInteger(downloadBytes)&&downloadBytes>0
        ? " (Download: ~"+(downloadBytes/1048576).toFixed(1)+" MiB)" : "";
    const details=$("genycaster-update-details");
    if(details){
        // Structured, readable release content; no remote HTML is evaluated.
        // The scrolling region and fixed footer remain separate.
        details.replaceChildren();
        const paragraph=document.createElement("p");
        paragraph.textContent=localizedUpdateDigits(summary||activeUpdateText()[5]+size);
        details.append(paragraph);
        if(bullets.length){
            const list=document.createElement("ul");
            for(const item of bullets){
                const entry=document.createElement("li");
                entry.textContent=localizedUpdateDigits(item.slice(0,600));
                list.append(entry);
            }
            details.append(list);
        }
    }
}
function hideReleaseDrawer(){
    if(updateBanner)updateBanner.hidden=true;
    if(updateBackdrop)updateBackdrop.hidden=true;
}
function showReleaseDrawer(fresh){
    fillReleaseDetails(fresh);
    if(updateBackdrop)updateBackdrop.hidden=false;
    if(updateBanner)updateBanner.hidden=false;
    updateNow?.focus?.();
}
updateBackdrop?.addEventListener("click",()=>{
    hideReleaseDrawer();
    dismissedUntil=Date.now()+10*60*1000;
});
document.addEventListener?.("keydown",event=>{
    if(event.key==="Escape" && updateBanner && !updateBanner.hidden){
        hideReleaseDrawer();
        dismissedUntil=Date.now()+10*60*1000;
    }
});
async function checkForNewRelease(){
    if(!finished || failed || updateCheckRunning ||
       !/^[0-9a-f]{20}$/.test(currentVersion) ||
       document.visibilityState==="hidden")return;
    const now=Date.now();
    if(now-lastCheck<15000)return;
    lastCheck=now;
    updateCheckRunning=true;
    try{
        const url=new URL("./genycaster-assets.json",location.href);
        url.searchParams.set("check",String(now));
        const response=await originalFetch(url.href,{cache:"no-store",credentials:"omit"});
        if(!response.ok)return;
        const fresh=await response.json();
        const version=String(fresh?.version||"");
        if(!/^[0-9a-f]{20}$/.test(version) || version===currentVersion)return;
        availableRelease=version;
        if(updateBanner && now>=dismissedUntil)showReleaseDrawer(fresh);
    }catch{
        // Offline installs continue normally, using their cached engine.
    }finally{updateCheckRunning=false;}
}
updateLater?.addEventListener("click",()=>{
    hideReleaseDrawer();
    dismissedUntil=Date.now()+10*60*1000;
});
updateNow?.addEventListener("click",()=>{
    if(!/^[0-9a-f]{20}$/.test(availableRelease))return;
    // This is a navigation, not storage cleanup. Preserve local keys,
    // accounts, recovery data and all immutable cached WASM revisions.
    const next=new URL(location.href);
    next.searchParams.set("genycaster-release",availableRelease);
    location.replace(next.href);
});
function watchReleaseUpdates(){
    if(!/^[0-9a-f]{20}$/.test(currentVersion))return;
    window.addEventListener("pageshow",checkForNewRelease);
    window.addEventListener("focus",checkForNewRelease);
    document.addEventListener?.("visibilitychange",()=>{
        if(document.visibilityState!=="hidden")void checkForNewRelease();
    });
    window.setInterval?.(checkForNewRelease,5*60*1000);
    window.setTimeout?.(checkForNewRelease,2500);
}
function recoverFromStaleWasmServiceWorker(error) {
    const message=String(error?.message||error||"");
    if(!/both async and sync fetching of the wasm failed|FetchEvent\.respondWith|Load failed/i.test(message))return;
    if(typeof navigator==="undefined" || !navigator.serviceWorker?.controller)return;
    if(!window.sessionStorage)return;
    const marker="genycaster.wasm.sw-recovery.v1."+(currentVersion||"unknown");
    try {
        if(window.sessionStorage.getItem(marker)==="attempted")return;
        window.sessionStorage.setItem(marker,"attempted");
    }catch{return;}
    // The previous worker may still control this document. Unregister only
    // this app's scope, keep IndexedDB/account data intact, then navigate once
    // so the next WASM fetch can bypass that stale controller.
    void (async()=>{
        const registration=await navigator.serviceWorker.getRegistration(location.href);
        if(!registration)return;
        await registration.unregister();
        const next=new URL(location.href);
        next.searchParams.set("genycaster-sw-recovery",currentVersion||"1");
        location.replace(next.href);
    })().catch(e=>console.warn("[Genycaster] WASM recovery could not reload",e));
}
function fail(error) {
    if(finished||failed)return;
    if(startupWatchdog!==undefined)window.clearInterval?.(startupWatchdog);
    failed=true;
    if(studioIntro)studioIntro.hidden=true;
    if(boot?.dataset)boot.dataset.intro='off';
    wasmResourceObserver?.disconnect();
    try{progressReader?.cancel();}catch{}
    preparing();
    if(boot?.dataset)boot.dataset.phase="failed";
    track?.setAttribute?.("aria-valuetext","Startup failed");
    const detail=String(error?.message||error||"Unknown startup failure").slice(0,350);
    announce("Couldn't start Genycaster",
        "The application couldn't initialize. "+detail+" — check the browser console and retry.",
        "Start failed");
    retry.classList.add("visible");
    console.error("[Genycaster] startup failed",error);
    recoverFromStaleWasmServiceWorker(error);
}
// The Qt loader signals only that WebAssembly bootstrapped, NOT that QML
// constructed Main. Requiring an explicit C++ acknowledgement prevents the
// silent black screen seen when a type in the AppShell dependency tree fails.
function revealReady() {
    if(failed||finished||!qtRuntimeLoaded||!qmlRootReady||!accountRestoreSettled
       ||!studioComplete||!splashMinimumReached)return;
    if(startupWatchdog!==undefined)window.clearInterval?.(startupWatchdog);
    wasmResourceObserver?.disconnect();
    try{progressReader?.cancel();}catch{}
    // Ready reflects Qt's actual initialization callback, not
    // a fabricated timer or estimated download percentage.
    if(boot?.dataset)boot.dataset.phase="ready";
    track?.setAttribute?.("aria-valuenow","100");
    track?.setAttribute?.("aria-valuetext","Ready");
    bootPhase="ready";
    finished=true;
    if(studioIntro)studioIntro.hidden=true;
    if(boot?.dataset)boot.dataset.intro='off';
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
    announce("Genycaster is ready","Opening your social interface…","Ready");
    fill.classList.add("measured");fill.style.width="100%";
    percent.textContent="";
    boot.classList.add("finished");
    window.setTimeout(()=>{boot.hidden=true;},280);
    watchReleaseUpdates();
}
// Called synchronously from main.cpp after the QML root is constructed,
// or with failure if QQmlApplicationEngine could not load Main.
window.__genycasterQmlStartup=(success,detail)=>{
if(failed||finished)return;
if(!success) {
fail(new Error("Qt interface failed to load: "+String(detail||"unknown QML error").slice(0,200)));
return;
}
qmlRootReady=true;
// QML cannot acknowledge readiness before its WASM engine has downloaded.
// Resource Timing is advisory and can be missing in browsers or after
// service-worker/Cloudflare delivery: do not stay stuck at "Downloading".
if(bootPhase==="download")
    showPhase("interface","Opening Genycaster…","Qt interface is ready.",
        "Opening interface");
revealReady();
};
// Fail visibly instead of leaving an endless animated "Downloading" state.
 // Count only foreground time: background browser tabs may suspend downloads.
let visibleBootMs=0, lastBootCheck=performance.now();
const startupWatchdog=window.setInterval?.(()=>{
    if(finished||failed)return;
    const now=performance.now();
    if(document.visibilityState==="hidden"){lastBootCheck=now;return;}
    visibleBootMs+=Math.min(Math.max(now-lastBootCheck,0),20000);
    lastBootCheck=now;
    // Cold WASM is ~80 MiB uncompressed (and ~33 MiB compressed). On slow
    // connections 4 minutes can be a normal transfer, and on WebKit the
    // Resource Timing completion event can be absent entirely. Limit the
    // foreground bootstrap to a realistic, finite horizon instead of
    // treating a missing progress notification as a failed download.
    const limit=bootPhase==="download"?900000:600000;
    if(visibleBootMs>=limit){
        const reason=bootPhase==="download"
            ? "WebAssembly did not finish loading within 15 minutes. The connection may be slow or interrupted. Please retry."
            : "The Qt interface did not become ready within 10 minutes. Please retry.";
        fail(new Error(reason));
    }
},10000);
async function start() {
    try {
        if(typeof qtLoad!=="function")
            throw new Error("qtloader.js is missing or did not load");
        // Qt 6.9+ generates an app-specific Emscripten factory:
        // <target>_entry. Qt 6.12 requires it in qt.entryFunction.
        // Loading genycaster.js alone does NOT select that factory.
        if(typeof window.genycaster_entry!=="function")
            throw new Error("genycaster.js did not expose genycaster_entry");
        if(isWebKitRuntime && await cacheStartup)return;
        if(!isWebKitRuntime && !seen && window.isSecureContext && navigator.serviceWorker
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
                if(file==="genycaster.wasm" &&
                   typeof window.__genycasterAssets?.wasm==="string" &&
                   /^genycaster\.[0-9a-f]{20}\.wasm$/.test(window.__genycasterAssets.wasm))
                    return new URL(window.__genycasterAssets.wasm,location.href).href;
                return (prefix||"")+file;
            },
            qt:{
                entryFunction:window.genycaster_entry,
                containerElements:[screen],
                onLoaded:()=>{
                    qtRuntimeLoaded=true;
                    // The runtime can only invoke this callback after the
                    // full binary has been received and instantiated.
                    // Treat this as stronger evidence than Resource Timing.
                    if(bootPhase==="download")
                        showPhase("interface","Opening Genycaster…",
                            "WebAssembly engine is ready. Loading Qt interface…",
                            "Opening interface");
                    revealReady();
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

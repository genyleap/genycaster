(function () {
"use strict";
const $ = id => document.getElementById(id);
const boot=$("genycaster-boot"), heading=$("boot-heading"), status=$("boot-status");
const stage=$("boot-stage"), fill=$("boot-fill"), percent=$("boot-percent"), retry=$("boot-retry");
const track=$("boot-track");
// Browser chrome is separate from the Qt canvas. Match the actual Qt theme
// after initialization; HTML defaults dark until the Qt bridge is ready.
// The installed PWA status bar follows the same theme when iOS permits it.
function applyBrowserChromeTheme(dark, remember=true) {
    const color=dark?"#0b0c0f":"#f1f1f3";
    document.documentElement.setAttribute("data-chrome-theme",dark?"dark":"light");
    const meta=document.querySelector('meta[name="theme-color"]');
    if(meta)meta.setAttribute("content",color);
    if(remember){
        try { localStorage.setItem("genycaster.browser.chrome.v1",dark?"dark":"light"); }
        catch (_) { /* optional appearance cache, never block Qt startup */ }
    }
    // The PWA status-bar style is chosen by WebKit at installation time;
    // rewriting a meta tag after boot cannot reliably re-style an installed
    // iOS app. Do not force the legacy black-translucent mode.
}
window.__genycasterSetBrowserChromeTheme=applyBrowserChromeTheme;
// Keep the early, OS- or user-selected appearance until Qt reports its real
// theme. Forcing dark here painted a black status bar over light iOS pages.
applyBrowserChromeTheme(
    document.documentElement.getAttribute?.("data-chrome-theme") !== "light", false);
let finished=false, failed=false;
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
const releaseLabel=typeof releaseInfo.releaseLabel==="string"
    && /^Beta \d+(?:\.\d+){1,2}$/.test(releaseInfo.releaseLabel)
    ? releaseInfo.releaseLabel : "";
const buildLabel=(releaseLabel||(applicationVersion?"v"+applicationVersion:""))
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
        download:[updating?"Updating Genycaster…":"Preparing Genycaster…",
                  "Downloading app files"],
        install:["Starting Genycaster…","Preparing application"],
        initialize:["Starting Genycaster…","Preparing application"],
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
function measureFirstDownload(response,url){
    if(!installing || !response.body || typeof response.clone!=="function")return;
    // Browser CacheStorage, proxies and compression may suppress or change
    // Content-Length. The immutable, hashed WASM's *decoded* byte size comes
    // from our own release packager, so we can measure the native stream even
    // behind Cloudflare or a Service Worker without guessing.
    const manifestBytes=Number(releaseInfo.wasmBytes);
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
                    percent.textContent=pct+"%";
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
    measureFirstDownload(response,wasmResourceUrl);
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
// An already-running PWA keeps its old Qt/WASM instance even when a new build
// is served. Only notify on a confirmed, public manifest mismatch; never
// reload a composing user without their explicit tap.
const updateBanner=$("genycaster-update-banner");
const updateBackdrop=$("genycaster-update-backdrop");
const updateNow=$("genycaster-update-now");
const updateLater=$("genycaster-update-later");
const updateTextByLanguage = {"ar":["يتوفر تحديث لجنيكاستر","إصدار جديد جاهز للمراجعة.","الإصدار الحالي","الإصدار الجديد","ملاحظات الإصدار","لم تُنشر ملاحظات لهذا الإصدار. راجع صفحة الإصدار الرسمية للتفاصيل.","ليس الآن","تحديث الصفحة"],"de":["Genycaster-Update verfügbar","Eine neuere Version ist verfügbar.","Aktuelle Version","Neue Version","Versionshinweise","Für dieses Release wurden keine Änderungsnotizen veröffentlicht. Details stehen auf der offiziellen Release-Seite.","Nicht jetzt","Zum Aktualisieren neu laden"],"en":["Genycaster update available","A newer release is ready to review.","Current version","New version","Release notes","This release has no published change notes. View the official release for details.","Not now","Refresh to update"],"es":["Actualización de Genycaster disponible","Hay una nueva versión lista para revisar.","Versión actual","Versión nueva","Notas de la versión","No hay notas publicadas para esta versión. Consulta el lanzamiento oficial para más detalles.","Ahora no","Actualizar página"],"fa":["به‌روزرسانی جنی‌کستر آماده است","نسخهٔ تازه برای بررسی آماده است.","نسخهٔ فعلی","نسخهٔ جدید","یادداشت‌های انتشار","برای این نسخه یادداشت تغییراتی منتشر نشده است. برای جزئیات به صفحهٔ رسمی انتشار مراجعه کنید.","فعلاً نه","بازبارگذاری و به‌روزرسانی"],"fr":["Mise à jour de Genycaster disponible","Une nouvelle version est disponible.","Version actuelle","Nouvelle version","Notes de version","Aucune note de modification publiée pour cette version. Consultez la page officielle pour plus de détails.","Pas maintenant","Actualiser pour mettre à jour"],"hi":["Genycaster अपडेट उपलब्ध है","नया संस्करण देखने के लिए तैयार है।","वर्तमान संस्करण","नया संस्करण","रिलीज़ नोट्स","इस रिलीज़ के बदलाव प्रकाशित नहीं किए गए हैं। विवरण के लिए आधिकारिक रिलीज़ पृष्ठ देखें।","अभी नहीं","अपडेट के लिए रीफ़्रेश करें"],"ja":["Genycaster の更新があります","新しいバージョンを確認できます。","現在のバージョン","新しいバージョン","リリースノート","このリリースの更新履歴は公開されていません。詳細は公式リリースページをご覧ください。","後で","再読み込みして更新"],"ko":["Genycaster 업데이트 이용 가능","새 버전을 확인할 수 있습니다.","현재 버전","새 버전","릴리스 정보","이 릴리스에는 게시된 변경 사항이 없습니다. 공식 릴리스 페이지에서 자세한 내용을 확인하세요.","나중에","새로고침하여 업데이트"],"pt":["Atualização do Genycaster disponível","Uma nova versão está pronta para avaliação.","Versão atual","Nova versão","Notas da versão","Este lançamento não tem notas de mudanças publicadas. Consulte a página oficial para detalhes.","Agora não","Atualizar página"],"ru":["Доступно обновление Genycaster","Новая версия готова к просмотру.","Текущая версия","Новая версия","Примечания к выпуску","Для этой версии нет опубликованных примечаний. Подробности — на официальной странице релиза.","Не сейчас","Обновить страницу"],"zh":["Genycaster 有可用更新","新版本已准备好供查看。","当前版本","新版本","版本说明","此版本尚未发布更新说明。详情请查看官方发布页面。","稍后","刷新并更新"]};
let updateLanguage="en";
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
    if(latestReleaseDetails)fillReleaseDetails(latestReleaseDetails);
}
window.__genycasterSetUpdateLanguage=setUpdateLanguage;
setUpdateLanguage(window.__genycasterPreferredLanguage||window.navigator?.language||"en");
function fillReleaseDetails(fresh){
    latestReleaseDetails=fresh;
    const current=$("genycaster-update-current");
    const latest=$("genycaster-update-next");
    if(current)current.textContent=currentVersion.slice(0,8);
    if(latest)latest.textContent=String(fresh.version||"").slice(0,8);
    const oldChannel=$("genycaster-update-current-channel");
    const newChannel=$("genycaster-update-next-channel");
    const validatedLabel=typeof fresh.releaseLabel==="string"
      && /^Beta \d+(?:\.\d+){1,2}$/.test(fresh.releaseLabel)
        ? fresh.releaseLabel : (releaseLabel||"Genycaster");
    if(oldChannel)oldChannel.textContent=releaseLabel||(applicationVersion?"v"+applicationVersion:"");
    if(newChannel)newChannel.textContent=validatedLabel;
    const notes=typeof fresh.releaseNotes==="string"
        ? fresh.releaseNotes.trim().slice(0,640) : "";
    const bytes=Number(fresh.wasmBytes);
    const size=Number.isSafeInteger(bytes)&&bytes>0
        ? " (WASM: "+(bytes/1048576).toFixed(1)+" MiB)" : "";
    const details=$("genycaster-update-details");
    if(details)details.textContent=notes||(activeUpdateText()[5]+size);
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
function fail(error) {
    if(finished||failed)return;
    failed=true;
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
}
// The Qt loader signals only that WebAssembly bootstrapped, NOT that QML
// constructed Main. Requiring an explicit C++ acknowledgement prevents the
// silent black screen seen when a type in the AppShell dependency tree fails.
let qtRuntimeLoaded=false, qmlRootReady=false;
function revealReady() {

    if(failed||finished||!qtRuntimeLoaded||!qmlRootReady)return;
    wasmResourceObserver?.disconnect();
    try{progressReader?.cancel();}catch{}
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
revealReady();
};
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
                    qtRuntimeLoaded=true;
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

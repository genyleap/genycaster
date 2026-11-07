import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";

const source=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
const fn=source.slice(source.indexOf("function recoverFromStaleWasmServiceWorker("),
    source.indexOf("function fail(error)"));
const sw=readFileSync(new URL("../../ui/wasm/genycaster-sw.js",import.meta.url),"utf8");

test("large WASM bypasses service worker without removing private browser data",()=>{
    const cacheVersion = sw.match(/const CACHE="genycaster-static-v(\d+)"/);
    assert.ok(cacheVersion && Number(cacheVersion[1]) >= 13,
        "app service worker must stay on the version that excludes WASM payloads");
    assert.match(sw,/if\(relative==="genycaster\.wasm" \|\| WASM_HASH\.test\(relative\)\)return;/);
    assert.doesNotMatch(sw,/cache\.put\([^\n]*\.wasm/);
    assert.match(source,/manifestBytes>16\*1024\*1024\)return;/);
});

test("failed old-worker WASM startup unregisters only app worker and reloads at most once",async()=>{
    const session=new Map();
    const changes=[];
    const location={href:"https://caster.genyleap.com/?p=1",replace:url=>changes.push(url)};
    let unregisterCount=0;
    const registration={unregister:async()=>{unregisterCount++;return true;}};
    const scope={
        currentVersion:"0123456789abcdef0123",
        URL,location,console,
        navigator:{serviceWorker:{controller:{},
            getRegistration:async()=>registration}},
        window:{sessionStorage:{getItem:k=>session.get(k)||null,
            setItem:(k,v)=>session.set(k,v)}}
    };
    vm.runInNewContext(fn+"\nthis.attempt=recoverFromStaleWasmServiceWorker;",scope);
    scope.attempt(new Error("Qt exited during startup (both async and sync fetching of the wasm failed)"));
    for(let n=0;n<8;n++)await Promise.resolve();
    assert.equal(unregisterCount,1);
    assert.equal(changes.length,1);
    assert.match(changes[0],/genycaster-sw-recovery=0123456789abcdef0123/);
    assert.match(changes[0],/p=1/);
    scope.attempt(new Error("both async and sync fetching of the wasm failed"));
    for(let n=0;n<8;n++)await Promise.resolve();
    assert.equal(unregisterCount,1,"no reload loop");
    assert.equal(changes.length,1);
});

test("unrelated errors and non-worker browsers do not unregister anything",async()=>{
    const location={href:"https://caster.genyleap.com/",replace:()=>{throw Error("should not reload")}};
    let calls=0;
    const scope={currentVersion:"test",URL,location,console,
        navigator:{serviceWorker:{controller:null,
            getRegistration:async()=>{calls++;return null;}}},
        window:{sessionStorage:{getItem:()=>null,setItem:()=>{}}}};
    vm.runInNewContext(fn+"\nthis.attempt=recoverFromStaleWasmServiceWorker;",scope);
    scope.attempt(new Error("authentication timed out"));
    scope.attempt(new Error("Load failed"));
    for(let n=0;n<8;n++)await Promise.resolve();
    assert.equal(calls,0);
});

test("WASM wrapper preserves native request credentials and retries once without stale cache",async()=>{
    const begin=source.indexOf("window.fetch=async function(input,options) {");
    const end=source.indexOf("const screen=$(\"qt-screen\");",begin);
    assert.ok(begin>=0 && end>begin);
    const calls=[];
    const wasmURL="https://caster.genyleap.com/genycaster.0123456789abcdef0123.wasm";
    const native=new Response(new Uint8Array([0,97,115,109]),{headers:{"content-type":"application/wasm"}});
    const ctx={window:{},URL,location:{href:"https://caster.genyleap.com/",origin:"https://caster.genyleap.com"},
        originalFetch:async(input,opts)=>{
            calls.push({url:String(input),options:opts});
            if(calls.length===1)throw new TypeError("Load failed");
            return native;
        },
        verifiedAsset:new URL(wasmURL),hashedWasm:/^genycaster\.[0-9a-f]{20}\.wasm$/,
        isWebKitRuntime:false,compressedWebKitEngine:async()=>null,releaseInfo:{wasmBytes:8},Headers,
        installing:false,updating:false,finished:false,bootPhase:"initialize",wasmResourceUrl:null,
        showPhase(){},observeEngine(){},preparing(){},measureFirstDownload(){},console};
    vm.runInNewContext(source.slice(begin,end),ctx);
    const result=await ctx.window.fetch(wasmURL,{credentials:"same-origin"});
    assert.equal(result,native);
    assert.equal(calls.length,2);
    assert.equal(calls[0].options.credentials,"same-origin");
    assert.equal(calls[0].options.cache,undefined);
    assert.equal(calls[1].options.credentials,"same-origin");
    assert.equal(calls[1].options.cache,"reload");
});

test("WebKit startup unregisters only app worker and navigates once before Qt bootstrap",async()=>{
    const begin=source.indexOf("async function enableOfflineCache() {");
    const end=source.indexOf("// An already-running PWA",begin);
    assert.ok(begin>=0&&end>begin);
    let unregisters=0,registrations=0;
    const navigations=[];
    const storage=new Map();
    const context={isWebKitRuntime:true,currentVersion:"12345678901234567890",URL,console,
        location:{href:"https://caster.genyleap.com/",replace:u=>navigations.push(u)},
        window:{isSecureContext:true,sessionStorage:{getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v)}},
        navigator:{serviceWorker:{controller:{},getRegistration:async()=>({unregister:async()=>{unregisters++;return true;}}),
            register:async()=>{registrations++;}}}};
    vm.runInNewContext(source.slice(begin,end).replace("const cacheStartup=enableOfflineCache();","")+"\nthis.boot=()=>enableOfflineCache();",context);
    assert.equal(await context.boot(),true);
    assert.equal(unregisters,1);
    assert.equal(registrations,0);
    assert.equal(navigations.length,1);
    assert.match(navigations[0],/genycaster-network-boot=12345678901234567890/);
    assert.equal(await context.boot(),false);
    assert.equal(navigations.length,1);
});

test("WebKit preserves native streaming Response on its first ordinary WASM request",async()=>{
    const begin=source.indexOf("window.fetch=async function(input,options) {");
    const end=source.indexOf('const screen=$("qt-screen");',begin);
    const wasmURL="https://caster.genyleap.com/genycaster.0123456789abcdef0123.wasm";
    const binary=Uint8Array.from([0,97,115,109,1,0,0,0]);
    const native=new Response(binary,{status:206,headers:{
        "Content-Type":"application/wasm",
        "Content-Length":"8",
        "Content-Range":"bytes 0-7/8"
    }});
    const calls=[];
    const ctx={window:{},URL,Headers,Response,isWebKitRuntime:true,
        compressedWebKitEngine:async()=>null,wasmResourceUrl:null,
        location:{href:"https://caster.genyleap.com/",origin:"https://caster.genyleap.com"},
        originalFetch:async(input,options)=>{calls.push({input,options});return native;},
        verifiedAsset:new URL(wasmURL),hashedWasm:/^genycaster\.[0-9a-f]{20}\.wasm$/,
        releaseInfo:{wasmBytes:8},installing:false,updating:false,finished:false,
        bootPhase:"initialize",showPhase(){},observeEngine(){},preparing(){},
        measureFirstDownload(){},console};
    vm.runInNewContext(source.slice(begin,end),ctx);
    const r=await ctx.window.fetch(wasmURL,{credentials:"same-origin"});
    assert.equal(r,native,"do not rewrap streaming response");
    assert.equal(calls.length,1);
    assert.equal(calls[0].options.headers,undefined,"normal fetch permits precompressed gzip");
    assert.equal(calls[0].options.credentials,"same-origin");
    const mod=await WebAssembly.compileStreaming(Promise.resolve(r));
    assert.ok(mod instanceof WebAssembly.Module,"206 full-body response streams as wasm");
});

test("WebKit rejects incomplete fallback ranges and uses Range only after ordinary fetch fails",async()=>{
    const begin=source.indexOf("window.fetch=async function(input,options) {");
    const end=source.indexOf('const screen=$("qt-screen");',begin);
    const wasmURL="https://caster.genyleap.com/genycaster.0123456789abcdef0123.wasm";
    const makeCtx=(impl)=>({window:{},URL,Headers,isWebKitRuntime:true,
        compressedWebKitEngine:async()=>null,wasmResourceUrl:null,
        location:{href:"https://caster.genyleap.com/",origin:"https://caster.genyleap.com"},
        originalFetch:impl,verifiedAsset:new URL(wasmURL),
        hashedWasm:/^genycaster\.[0-9a-f]{20}\.wasm$/,
        releaseInfo:{wasmBytes:8},installing:false,updating:false,finished:false,
        bootPhase:"initialize",showPhase(){},observeEngine(){},preparing(){},
        measureFirstDownload(){},console});
    const truncated=new Response(new Uint8Array(4),{status:206,headers:{
        "Content-Type":"application/wasm","Content-Range":"bytes 0-3/8"}});
    let n=0;
    const ctx=makeCtx(async()=>{if(!n++)throw new TypeError("Load failed");return truncated;});
    vm.runInNewContext(source.slice(begin,end),ctx);
    await assert.rejects(ctx.window.fetch(wasmURL),/Incomplete WebAssembly range response/);
    const normal=new Response(new Uint8Array([0,97,115,109,1,0,0,0]),
        {headers:{"Content-Type":"application/wasm"}});
    const calls=[];
    const fallback=makeCtx(async(_input,options)=>{
        calls.push(options);
        if(calls.length===1)throw new TypeError("Load failed");
        return normal;
    });
    vm.runInNewContext(source.slice(begin,end),fallback);
    assert.equal(await fallback.window.fetch(wasmURL),normal);
    assert.equal(calls[0].headers,undefined,"initial request permits gzip");
    assert.equal(calls[1].headers.get("range"),"bytes=0-","fallback requests identity WASM");
    assert.equal(calls[1].cache,"reload");
});

test("WebKit gzip sidecar streams and decodes valid WebAssembly without cloning full engine",async()=>{
    const begin=source.indexOf("async function compressedWebKitEngine(");
    const end=source.indexOf("window.fetch=async function(input,options) {",begin);
    assert.ok(begin>=0 && end>begin);
    const {gzipSync}=await import("node:zlib");
    const binary=Uint8Array.from([0,97,115,109,1,0,0,0]);
    const compressed=gzipSync(binary);
    const wasm="genycaster.0123456789abcdef0123.wasm";
    const url="https://caster.genyleap.com/"+wasm;
    const assetRequests=[];
    const ctx={
        URL,Response,TransformStream,DecompressionStream,
        isWebKitRuntime:true,hashedWasm:new RegExp("^genycaster[.][0-9a-f]{20}[.]wasm$"),
        releaseInfo:{wasmGzip:wasm+".gz",wasmGzipBytes:compressed.length,
            version:"1234567890abcdef1234"},
        location:{href:"https://caster.genyleap.com/"},
        originalFetch:async(request,options)=>{
            assetRequests.push({request,options});
            return new Response(compressed,{headers:{
                "Content-Length":String(compressed.length),
                "Content-Type":"application/octet-stream"
            }});
        },
        installing:true,finished:false,failed:false,bootPhase:"download",
        fill:{classList:{add(){}},style:{}},percent:{textContent:""},
        track:{setAttribute(){}},performance:{now:()=>200},showPhase(){},console
    };
    const progress = source.slice(source.indexOf("function downloadProgressText("),
        source.indexOf("function preparing()"));
    assert.match(progress,/function downloadProgressText/);
    vm.runInNewContext(progress + source.slice(begin,end)
        +"\nthis.loadCompressed=compressedWebKitEngine;",ctx);
    const result=await ctx.loadCompressed(new URL(url),{credentials:"same-origin"});
    assert.ok(result);
    assert.equal(result.response.headers.get("content-type"),"application/wasm");
    const module=await WebAssembly.compileStreaming(Promise.resolve(result.response));
    assert.ok(module instanceof WebAssembly.Module);
    assert.equal(assetRequests.length,1);
    assert.equal(assetRequests[0].request,
        url+".gz?v=1234567890abcdef1234");
    assert.equal(assetRequests[0].options.cache,"no-cache",
        "do not reuse cached 200 HTML from a previously missing sidecar");
});

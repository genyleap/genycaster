import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";

const source=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
const fn=source.slice(source.indexOf("function recoverFromStaleWasmServiceWorker("),
    source.indexOf("function fail(error)"));
const sw=readFileSync(new URL("../../ui/wasm/genycaster-sw.js",import.meta.url),"utf8");

test("large WASM bypasses service worker without removing private browser data",()=>{
    assert.match(sw,/const CACHE="genycaster-static-v11"/);
    assert.match(sw,/if\(relative==="farcaster_client\.wasm" \|\| WASM_HASH\.test\(relative\)\)return;/);
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
    const wasmURL="https://caster.genyleap.com/farcaster_client.0123456789abcdef0123.wasm";
    const native=new Response(new Uint8Array([0,97,115,109]),{headers:{"content-type":"application/wasm"}});
    const ctx={window:{},URL,location:{href:"https://caster.genyleap.com/",origin:"https://caster.genyleap.com"},
        originalFetch:async(input,opts)=>{
            calls.push({url:String(input),options:opts});
            if(calls.length===1)throw new TypeError("Load failed");
            return native;
        },
        verifiedAsset:new URL(wasmURL),hashedWasm:/^farcaster_client\.[0-9a-f]{20}\.wasm$/,
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

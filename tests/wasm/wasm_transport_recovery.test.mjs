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

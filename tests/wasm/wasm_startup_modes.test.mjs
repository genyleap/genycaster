import test from "node:test";
import assert from "node:assert/strict";
import vm from "node:vm";
import {readFileSync} from "node:fs";

const script=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
function host({loaded=false,oldVersion="",version="aaaaaaaaaaaaaaaaaaaa",measured=false,withoutLength=false}={}) {
    const nodes=new Map();
    for(const id of ["genycaster-boot","boot-heading","boot-status","boot-stage",
        "boot-fill","boot-percent","boot-retry","boot-build-credit","qt-screen"]) {
        const classes=new Set();
        nodes.set(id,{
            hidden:false,textContent:"",style:{},dataset:{},
            classList:{add:x=>classes.add(x),remove:x=>classes.delete(x),contains:x=>classes.has(x)},
            addEventListener(){}
        });
    }
    const storage=new Map();
    if(loaded)storage.set("genycaster.boot.loaded.v1","1");
    if(oldVersion)storage.set("genycaster.boot.version.v1",oldVersion);
    const resources=new Map();
    const chrome={scheme:"",color:"",statusStyle:""};
    let config=null, observer=null, downloadStream=null;
    const binary="farcaster_client.bbbbbbbbbbbbbbbbbbbb.wasm";
    const scope={
        window:{
            __genycasterAssets:{version,wasm:binary,appVersion:"1.0.0",buildId:version.slice(0,8),
                releaseLabel:"Beta 1.0",wasmBytes:8},
            farcaster_client_entry(){},
            fetch:async()=>measured
                ? new Response(new ReadableStream({
                    start(controller){downloadStream=controller;}
                }),{headers:{"Content-Type":"application/wasm",
                    ...(withoutLength?{}:{"Content-Length":"8"})}})
                : new Response(new Uint8Array([0,97,115,109]),{
                    headers:{"Content-Type":"application/wasm"}
                }),
            addEventListener(){},innerHeight:700,
            setTimeout:fn=>{fn();},
        },
        localStorage:{
            getItem:key=>storage.get(key)||null,
            setItem:(key,val)=>storage.set(key,val),
        },
        document:{getElementById:id=>nodes.get(id),
            documentElement:{setAttribute:(key,value)=>{if(key==="data-chrome-theme")chrome.scheme=value;}},
            querySelector:selector=>({
                setAttribute:(key,value)=>{
                    if(key!=="content")return;
                    if(selector.includes("theme-color"))chrome.color=value;
                    if(selector.includes("status-bar-style"))chrome.statusStyle=value;
                }
            })},
        location:{href:"https://example.test/farcaster_client.html",origin:"https://example.test"},
        performance:{now:()=>100,getEntriesByName:url=>resources.get(url)||[]},
        PerformanceObserver:class {
            constructor(cb){this.cb=cb;}
            observe(){observer=this.cb;}
            disconnect(){}
        },
        qtLoad:async options=>{config=options;},
        console,URL,Response,Headers,ReadableStream
    };
    vm.runInNewContext(script,scope);
    return {
        nodes,storage,resources,chrome,
        setBrowserTheme:dark=>scope.window.__genycasterSetBrowserChromeTheme(dark),
        qmlReady:()=>scope.window.__genycasterQmlStartup(true, ""),
        qmlFailed:reason=>scope.window.__genycasterQmlStartup(false,reason),
        fetch:()=>scope.window.fetch("https://example.test/farcaster_client.wasm"),
        pushBytes(bytes){downloadStream.enqueue(new Uint8Array(bytes));},
        finishBytes(){downloadStream.close();},
        percent:()=>nodes.get("boot-percent").textContent,
        get config(){return config;},
        resourceComplete(){
            const resource={name:"https://example.test/"+binary,responseEnd:350,duration:350,
                transferSize:123456,decodedBodySize:456789};
            resources.set(resource.name,[resource]);
            observer?.({getEntries:()=>[resource]});
        },
        phase:()=>nodes.get("genycaster-boot").dataset.phase,
        mode:()=>nodes.get("genycaster-boot").dataset.mode,
        label:()=>nodes.get("boot-stage").textContent
    };
}

test("first launch uses real download → install → Qt initialize → ready milestones",async()=>{
    const h=host();
    assert.equal(h.phase(),"download");
    assert.equal(h.mode(),"install");
    assert.match(h.label(),/Download/i);
    assert.equal(h.nodes.get("boot-build-credit").textContent,"Beta 1.0 · Build aaaaaaaa");
    const response=await h.fetch();
    await response.arrayBuffer();
    assert.equal(h.phase(),"download","headers are not download completion");
    h.resourceComplete();
    assert.equal(h.phase(),"install");
    assert.equal(h.label(),"Preparing application");
    h.config.monitorRunDependencies(0);
    assert.equal(h.phase(),"interface");
    assert.equal(h.label(),"Almost ready");
    h.config.qt.onLoaded();
    assert.equal(h.phase(),"interface","Qt runtime alone is not QML readiness");
    h.qmlReady();
    assert.equal(h.phase(),"ready");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),"1");
    assert.equal(h.storage.get("genycaster.boot.version.v1"),"aaaaaaaaaaaaaaaaaaaa");
});

test("normal refresh skips Download/Install and restores the Qt runtime",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    assert.equal(h.mode(),"warm");
    assert.equal(h.phase(),"initialize");
    assert.equal(h.nodes.get("boot-heading").textContent,"Starting Genycaster…");
    assert.equal(h.label(),"Preparing application");
    const response=await h.fetch();
    assert.ok(response.ok);
    assert.equal(h.phase(),"initialize");
    h.config.monitorRunDependencies(0);
    assert.equal(h.phase(),"interface");
    h.config.qt.onLoaded();
    assert.equal(h.phase(),"interface","Qt runtime alone is not QML readiness");
    h.qmlReady();
    assert.equal(h.phase(),"ready");
});

test("a changed deployment follows update stages rather than a warm reload",async()=>{
    const h=host({loaded:true,oldVersion:"00000000000000000000"});
    assert.equal(h.mode(),"install");
    assert.equal(h.phase(),"download");
    await h.fetch();
    h.resourceComplete();
    assert.equal(h.phase(),"install");
    h.config.monitorRunDependencies(0);
    assert.equal(h.phase(),"interface");
    h.config.qt.onLoaded();
    h.qmlReady();
    assert.equal(h.storage.get("genycaster.boot.version.v1"),"aaaaaaaaaaaaaaaaaaaa");
});

test("QML startup acknowledged before Qt onLoaded still completes correctly",()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    h.qmlReady();
    assert.equal(h.phase(),"initialize","QML acknowledgement alone is not sufficient");
    h.config.qt.onLoaded();
    assert.equal(h.phase(),"ready");
});
test("a broken Main.qml reports a recoverable failure instead of hiding the overlay",()=>{
    const h=host();
    h.config.qt.onLoaded();
    assert.notEqual(h.phase(),"ready");
    assert.equal(h.nodes.get("genycaster-boot").classList.contains("finished"),false);
    h.qmlFailed("QQmlApplicationEngine Main unavailable");
    assert.equal(h.phase(),"failed");
    assert.equal(h.nodes.get("genycaster-boot").dataset.phase,"failed");
    assert.equal(h.nodes.get("boot-retry").classList.contains("visible"),true);
    assert.match(h.nodes.get("boot-status").textContent,/QML/i);
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),undefined);
    h.qmlReady();
    assert.equal(h.nodes.get("genycaster-boot").dataset.phase,"failed");
});
test("the application emits the QML startup handshake after root creation",()=>{
    const cpp=readFileSync(new URL("../../ui/mobile/main.cpp",import.meta.url),"utf8");
    assert.match(cpp,/if \(engine\.rootObjects\(\)\.isEmpty\(\)\) \{[\s\S]*?__genycasterQmlStartup\?\.\(false/);
    assert.match(cpp,/installBrowserWheelBridge\(mainWindow\);[\s\S]*?__genycasterQmlStartup\?\.\(true/);
});
test("failed boot never marks first-time install as completed",async()=>{
    const h=host();
    h.config.qt.onExit({text:"wasm compile failure"});
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),undefined);
    assert.equal(h.storage.get("genycaster.boot.version.v1"),undefined);
    assert.equal(h.nodes.get("boot-retry").classList.contains("visible"),true);
    assert.equal(h.nodes.get("boot-heading").textContent,"Couldn't start Genycaster");
    h.config.monitorRunDependencies(0);
    h.config.qt.onLoaded(); // a late callback must not claim success
    assert.equal(h.nodes.get("genycaster-boot").dataset.phase,"failed");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),undefined);
});

test("first download shows measured byte percent without replacing Qt's native response",async()=>{
    const h=host({measured:true});
    const original=await h.fetch();
    assert.equal(h.phase(),"download");
    assert.equal(h.percent(),"");
    h.pushBytes([0,97,115,109]);
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.percent(),"50%");
    assert.equal(h.phase(),"download");
    h.pushBytes([1,0,0,0]);
    h.finishBytes();
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.phase(),"install");
    assert.equal(h.percent(),"");
    assert.equal((await original.arrayBuffer()).byteLength,8);
    h.config.monitorRunDependencies(0);
    h.config.qt.onLoaded();
    assert.equal(h.phase(),"interface","Qt runtime alone is not QML readiness");
    h.qmlReady();
    assert.equal(h.phase(),"ready");
});

test("warm refresh never clones large WASM for progress",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa",measured:true});
    await h.fetch();
    assert.equal(h.percent(),"");
    assert.equal(h.phase(),"initialize");
    h.config.qt.onLoaded();
});


test("Qt appearance updates browser chrome without changing user account state",()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    assert.equal(h.chrome.scheme,"dark");
    assert.equal(h.chrome.color,"#0b0c0f");
    assert.equal(h.chrome.statusStyle,"","status style must not be rewritten at runtime");
    h.setBrowserTheme(false);
    assert.equal(h.chrome.scheme,"light");
    assert.equal(h.chrome.color,"#f1f1f3");
    assert.equal(h.chrome.statusStyle,"","light mode must not force an installed PWA status style");
    h.setBrowserTheme(true);
    assert.equal(h.chrome.scheme,"dark");
    assert.equal(h.chrome.color,"#0b0c0f");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),"1");
});


test("first install reports genuine WASM byte progress without Content-Length",async()=>{
    const h=host({measured:true,withoutLength:true});
    const response=await h.fetch();
    h.pushBytes([0,97,115,109]);
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.percent(),"50%");
    assert.equal(h.phase(),"download");
    h.pushBytes([1,0,0,0]);
    h.finishBytes();
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.phase(),"install");
    assert.equal((await response.arrayBuffer()).byteLength,8,
        "the original native streaming response must remain intact");
});

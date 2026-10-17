import test from "node:test";
import assert from "node:assert/strict";
import vm from "node:vm";
import {readFileSync} from "node:fs";

const script=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
function host({loaded=false,oldVersion="",version="aaaaaaaaaaaaaaaaaaaa"}={}) {
    const nodes=new Map();
    for(const id of ["genycaster-boot","boot-heading","boot-status","boot-stage",
        "boot-fill","boot-percent","boot-retry","qt-screen"]) {
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
    let config=null, observer=null;
    const binary="farcaster_client.bbbbbbbbbbbbbbbbbbbb.wasm";
    const scope={
        window:{
            __genycasterAssets:{version,wasm:binary},
            farcaster_client_entry(){},
            fetch:async()=>new Response(new Uint8Array([0,97,115,109]),{
                headers:{"Content-Type":"application/wasm"}
            }),
            addEventListener(){},innerHeight:700,
            setTimeout:fn=>{fn();},
        },
        localStorage:{
            getItem:key=>storage.get(key)||null,
            setItem:(key,val)=>storage.set(key,val),
        },
        document:{getElementById:id=>nodes.get(id)},
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
        nodes,storage,resources,
        fetch:()=>scope.window.fetch("https://example.test/farcaster_client.wasm"),
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
    const response=await h.fetch();
    await response.arrayBuffer();
    assert.equal(h.phase(),"download","headers are not download completion");
    h.resourceComplete();
    assert.equal(h.phase(),"install");
    assert.match(h.label(),/Install/i);
    h.config.monitorRunDependencies(0);
    assert.equal(h.phase(),"initialize");
    h.config.qt.onLoaded();
    assert.equal(h.phase(),"ready");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),"1");
    assert.equal(h.storage.get("genycaster.boot.version.v1"),"aaaaaaaaaaaaaaaaaaaa");
});

test("normal refresh skips Download/Install and restores the Qt runtime",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    assert.equal(h.mode(),"warm");
    assert.equal(h.phase(),"initialize");
    assert.match(h.label(),/Initializ/i);
    const response=await h.fetch();
    assert.ok(response.ok);
    assert.equal(h.phase(),"initialize");
    h.config.monitorRunDependencies(0);
    assert.equal(h.phase(),"initialize");
    h.config.qt.onLoaded();
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
    assert.equal(h.phase(),"initialize");
    h.config.qt.onLoaded();
    assert.equal(h.storage.get("genycaster.boot.version.v1"),"aaaaaaaaaaaaaaaaaaaa");
});

test("failed boot never marks first-time install as completed",async()=>{
    const h=host();
    h.config.qt.onExit({text:"wasm compile failure"});
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),undefined);
    assert.equal(h.storage.get("genycaster.boot.version.v1"),undefined);
    assert.equal(h.nodes.get("boot-retry").classList.contains("visible"),true);
});
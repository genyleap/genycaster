import test from "node:test";
import assert from "node:assert/strict";
import vm from "node:vm";
import {readFileSync} from "node:fs";

const script=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
function host({loaded=false,oldVersion="",version="aaaaaaaaaaaaaaaaaaaa",measured=false,withoutLength=false,studioTimed=false,authRestorePending=false}={}) {
    function element(tag="div") {
        const children=[];
        let text="";
        return {tag,children,
            get textContent(){return text+children.map(child=>child.textContent).join("");},
            set textContent(value){text=String(value);children.length=0;},
            replaceChildren(...next){text="";children.splice(0,children.length,...next);},
            append(...next){children.push(...next);}
        };
    }
    const nodes=new Map();
    for(const id of ["genycaster-boot","boot-heading","boot-status","boot-stage",
        "boot-fill","boot-percent","boot-retry","boot-build-credit","qt-screen",
        "genycaster-update-banner","genycaster-update-backdrop","genycaster-update-title",
        "genycaster-update-hint","genycaster-update-now","genycaster-update-later",
        "genycaster-update-current-label","genycaster-update-next-label",
        "genycaster-update-details-title",
        "genycaster-update-current","genycaster-update-next",
        "genycaster-update-current-channel","genycaster-update-next-channel",
        "genycaster-update-details","genycaster-studio-intro"]) {
        const classes=new Set();
        nodes.set(id,Object.assign(element(),{
            hidden:id==="genycaster-update-banner"||id==="genycaster-update-backdrop"||id==="genycaster-studio-intro",style:{},dataset:{},
            classList:{add:x=>classes.add(x),remove:x=>classes.delete(x),contains:x=>classes.has(x)},
            handlers:{},addEventListener(type,handler){this.handlers[type]=handler;}
        }));
    }
    const storage=new Map();
    if(loaded)storage.set("genycaster.boot.loaded.v1","1");
    if(oldVersion)storage.set("genycaster.boot.version.v1",oldVersion);
    const resources=new Map();
    const chrome={scheme:"",color:"",statusStyle:""};
    let config=null, observer=null, downloadStream=null;
    let latestVersion=version,latestAppVersion="1.0.1", reloadedTo="";
    let now=100;
    const timers=[];
    const listeners={};
    const binary="genycaster.bbbbbbbbbbbbbbbbbbbb.wasm";
    const scope={
        window:{
            __genycasterAssets:{version,wasm:binary,appVersion:"1.0.0",buildId:version.slice(0,8),
                productName:"Genycaster",versionNumber:"1.0.0",releaseChannel:"rc",releaseChannelNumber:1,wasmBytes:8},
            genycaster_entry(){},
            fetch:async(input)=>String(input).includes("genycaster-assets.json")
                ? new Response(JSON.stringify({version:latestVersion,buildId:latestVersion.slice(0,8),appVersion:latestAppVersion,versionNumber:latestAppVersion,releaseChannel:"rc",releaseChannelNumber:1,wasmBytes:78643200}),
                    {headers:{"Content-Type":"application/json"}})
                : measured
                    ? new Response(new ReadableStream({
                        start(controller){downloadStream=controller;}
                    }),{headers:{"Content-Type":"application/wasm",
                        ...(withoutLength?{}:{"Content-Length":"8"})}})
                    : new Response(new Uint8Array([0,97,115,109]),{
                        headers:{"Content-Type":"application/wasm"}
                    }),
            addEventListener(name,handler){listeners[name]=handler;},innerHeight:700,
            setTimeout:(fn,delay)=>{
                if(delay===2500)return 0;
                if(studioTimed){timers.push({at:now+delay,fn});return timers.length;}
                fn();
                return 0;
            },
        },
        localStorage:{
            getItem:key=>storage.get(key)||null,
            setItem:(key,val)=>storage.set(key,val),
        },
        document:{getElementById:id=>nodes.get(id),createElement:tag=>element(tag),visibilityState:"visible",
            addEventListener(name,handler){listeners[name]=handler;},
            documentElement:{
                setAttribute:(key,value)=>{if(key==="data-chrome-theme")chrome.scheme=value;},
                getAttribute:key=>key==="data-chrome-theme"?chrome.scheme:null,
                style:{setProperty:(key,value)=>{if(key==="--nd-accent")chrome.accent=value;}}
            },
            querySelector:selector=>({
                setAttribute:(key,value)=>{
                    if(key!=="content")return;
                    if(selector.includes("theme-color"))chrome.color=value;
                    if(selector.includes("status-bar-style"))chrome.statusStyle=value;
                }
            })},
        navigator:{platform:"Linux",userAgent:"Genycaster test host"},
        location:{href:"https://example.test/genycaster.html",origin:"https://example.test",
                  replace(url){reloadedTo=url;}},
        performance:{now:()=>now,getEntriesByName:url=>resources.get(url)||[]},
        PerformanceObserver:class {
            constructor(cb){this.cb=cb;}
            observe(){observer=this.cb;}
            disconnect(){}
        },
        qtLoad:async options=>{config=options;},
        console,URL,Response,Headers,ReadableStream
    };
    vm.runInNewContext(script,scope);
    // Simulate the terminal account-restore result: an authenticated UI must
    // never appear while the browser is still resolving the remembered user.
    if (!authRestorePending) scope.window.__genycasterBrowserAuthBootstrap();
    return {
        nodes,storage,resources,chrome,
        advance(ms) {
            const target=now+ms;
            while(timers.some(timer=>timer.at<=target)){
                timers.sort((a,b)=>a.at-b.at);
                const next=timers.shift();
                now=next.at;
                next.fn();
            }
            now=target;
        },
        setLatestVersion(value){latestVersion=value;},
        setLatestAppVersion(value){latestAppVersion=value;},
        fireFocus(){return listeners.focus?.();},
        clickUpdate(){nodes.get("genycaster-update-now").handlers.click?.();},
        clickLater(){nodes.get("genycaster-update-later").handlers.click?.();},
        setAppLanguage:tag=>scope.window.__genycasterSetUpdateLanguage(tag),
        get reloadedTo(){return reloadedTo;},
        setBrowserTheme:dark=>scope.window.__genycasterSetBrowserChromeTheme(dark),
        qmlReady:()=>scope.window.__genycasterQmlStartup(true, ""),
        authSettled:()=>scope.window.__genycasterBrowserAuthBootstrap(),
        qmlFailed:reason=>scope.window.__genycasterQmlStartup(false,reason),
        fetch:()=>scope.window.fetch("https://example.test/genycaster.wasm"),
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

test("three intro phases: 2.7s studio, 5.5s partners, 2.6s Genycaster splash",()=>{
    const h=host({studioTimed:true});
    const intro=h.nodes.get("genycaster-studio-intro");
    assert.equal(intro.hidden,false);
    assert.equal(intro.dataset.scene,"studio");
    h.config.qt.onLoaded();
    h.qmlReady();
    assert.notEqual(h.phase(),"ready","Qt readiness must not cut the studio credit short");
    h.advance(2699);
    assert.equal(intro.dataset.scene,"studio");
    assert.equal(intro.hidden,false);
    h.advance(1);
    assert.equal(intro.dataset.scene,"partners");
    h.advance(5499);
    assert.equal(intro.hidden,false);
    assert.notEqual(h.phase(),"ready");
    h.advance(1);
    assert.equal(intro.hidden,true);
    assert.notEqual(h.phase(),"ready","splash must stay visible after logo reveal");
    h.advance(2599);
    assert.notEqual(h.phase(),"ready");
    h.advance(1);
    assert.equal(h.phase(),"ready","real Qt readiness is revealed after the splash");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),"1");
});
test("every refresh presents the studio credits, even after WASM is cached",()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa",studioTimed:true});
    const intro=h.nodes.get("genycaster-studio-intro");
    assert.equal(intro.hidden,false);
    h.config.qt.onLoaded();
    h.qmlReady();
    assert.notEqual(h.phase(),"ready");
    h.advance(2700);
    assert.equal(intro.dataset.scene,"partners");
    h.advance(5500);
    assert.equal(intro.hidden,true);
    assert.notEqual(h.phase(),"ready");
    h.advance(2600);
    assert.equal(h.phase(),"ready");
});

test("first launch uses real download → install → Qt initialize → ready milestones",async()=>{
    const h=host();
    assert.equal(h.phase(),"download");
    assert.equal(h.mode(),"install");
    assert.match(h.label(),/Download/i);
    assert.equal(h.nodes.get("boot-build-credit").textContent,"v1.0.0 · Build aaaaaaaa");
    const response=await h.fetch();
    await response.arrayBuffer();
    assert.equal(h.phase(),"download","headers are not download completion");
    h.resourceComplete();
    assert.equal(h.phase(),"install");
    assert.equal(h.label(),"Preparing WebAssembly resources");
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
    assert.equal(h.nodes.get("boot-heading").textContent,"Initializing Runtime…");
    assert.equal(h.label(),"Initializing Qt and WebAssembly");
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
test("browser startup does not reveal account UI until restore settles",()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa",authRestorePending:true});
    h.config.qt.onLoaded();
    h.qmlReady();
    assert.notEqual(h.phase(),"ready","Qt/QML readiness cannot bypass account restore");
    h.authSettled();
    assert.equal(h.phase(),"ready","terminal account restore completes an otherwise ready UI");
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
    const cpp=readFileSync(new URL("../../ui/app/main.cpp",import.meta.url),"utf8");
    assert.match(cpp,/if \(engine\.rootObjects\(\)\.isEmpty\(\)\) \{[\s\S]*?__genycasterQmlStartup\?\.\(false/);
    assert.match(cpp,/genycaster::ui::web::installWheelBridge\(mainWindow\);[\s\S]*?__genycasterQmlStartup\?\.\(true/);
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

test("installed PWA notices a new live release without discarding draft or identity",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    h.config.qt.onLoaded();
    h.qmlReady();
    // The ready screen does not refresh itself just because a new build exists.
    h.setLatestVersion("bbbbbbbbbbbbbbbbbbbb");
    h.fireFocus();
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.nodes.get("genycaster-update-banner").hidden,false);
    assert.equal(h.nodes.get("genycaster-update-backdrop").hidden,false);
    assert.equal(h.nodes.get("genycaster-update-current").textContent,"v1.0.0");
    assert.equal(h.nodes.get("genycaster-update-next").textContent,"v1.0.1");
    assert.equal(h.nodes.get("genycaster-update-next-channel").textContent,"Build bbbbbbbb");
    assert.match(h.nodes.get("genycaster-update-details").textContent,/75\.0 MiB/);
    assert.equal(h.reloadedTo,"","update prompt cannot navigate without a user tap");
    assert.equal(h.storage.get("genycaster.boot.loaded.v1"),"1");
    h.clickUpdate();
    assert.match(h.reloadedTo,/genycaster-release=bbbbbbbbbbbbbbbbbbbb/);
});

test("a same-version WASM rollout shows both build identifiers, not identical semvers",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    h.config.qt.onLoaded();
    h.qmlReady();
    h.setLatestVersion("bbbbbbbbbbbbbbbbbbbb");
    h.setLatestAppVersion("1.0.0");
    h.fireFocus();
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.nodes.get("genycaster-update-current").textContent,"Build aaaaaaaa");
    assert.equal(h.nodes.get("genycaster-update-next").textContent,"Build bbbbbbbb");
    assert.equal(h.nodes.get("genycaster-update-current-channel").textContent,"v1.0.0");
    assert.equal(h.nodes.get("genycaster-update-next-channel").textContent,"v1.0.0");
    assert.equal(h.reloadedTo,"");
});

test("no update prompt is shown when remote and running builds match",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    h.config.qt.onLoaded();
    h.qmlReady();
    h.fireFocus();
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(h.nodes.get("genycaster-update-banner").hidden,true);
    assert.equal(h.nodes.get("genycaster-update-backdrop").hidden,true);
    assert.equal(h.reloadedTo,"");
});

test("WASM release notifier is a top-down, non-destructive, accessible drawer",()=>{
    const shell=readFileSync(new URL("../../ui/wasm/genycaster-shell.html",import.meta.url),"utf8");
    assert.match(shell,/#genycaster-update-banner\{position:fixed;z-index:130;top:0/);
    assert.match(shell,/@keyframes genycasterDrawerIn/);
    assert.match(shell,/id="genycaster-update-backdrop"/);
    assert.match(shell,/role="dialog" aria-modal="true"/);
    assert.match(shell,/id="genycaster-update-current"/);
    assert.match(shell,/id="genycaster-update-next"/);
    assert.match(shell,/id="genycaster-update-details"/);
    assert.doesNotMatch(shell,/#genycaster-update-banner\{[^}]*bottom:calc/);
});


test("WASM update drawer has independent fixed chrome and a scroll-only notes viewport",()=>{
    const shell=readFileSync(new URL("../../ui/wasm/genycaster-shell.html",import.meta.url),"utf8");
    assert.match(shell,/#genycaster-update-banner\{[^}]*display:flex;flex-direction:column;overflow:hidden/);
    assert.match(shell,/\.update-fixed-header\{flex:0 0 auto/);
    assert.match(shell,/\.update-scroll-body\{[^}]*overflow-y:auto/);
    assert.match(shell,/\.update-actions\{flex:0 0 auto/);
    const header=shell.indexOf('class="update-fixed-header"');
    const content=shell.indexOf('class="update-scroll-body"');
    const footer=shell.indexOf('class="update-actions"');
    assert.ok(header>0 && header<content && content<footer);
    assert.match(shell,/id="genycaster-update-details"/);
});

test("WASM release drawer tracks in-app language changes independently of browser locale",async()=>{
    const h=host({loaded:true,oldVersion:"aaaaaaaaaaaaaaaaaaaa"});
    h.config.qt.onLoaded();
    h.qmlReady();
    h.setLatestVersion("bbbbbbbbbbbbbbbbbbbb");
    h.fireFocus();
    await new Promise(resolve=>setImmediate(resolve));
    h.setAppLanguage("fa-IR");
    assert.equal(h.nodes.get("genycaster-update-title").textContent,"به‌روزرسانی جنی‌کستر آماده است");
    assert.equal(h.nodes.get("genycaster-update-now").textContent,"بازبارگذاری و به‌روزرسانی");
    assert.equal(h.nodes.get("genycaster-update-current-label").textContent,"نسخهٔ فعلی");
    assert.equal(h.nodes.get("genycaster-update-current").textContent,"v۱.۰.۰");
    assert.equal(h.nodes.get("genycaster-update-next").textContent,"v۱.۰.۱");
    assert.equal(h.nodes.get("genycaster-update-next-channel").textContent,"Build bbbbbbbb");
    assert.equal(h.nodes.get("boot-build-credit").textContent,"v۱.۰.۰ · Build aaaaaaaa");
    assert.match(h.nodes.get("genycaster-update-details").textContent,/برای این نسخه یادداشت تغییراتی/);
    assert.match(h.nodes.get("genycaster-update-details").textContent,/۷۵\.۰ MiB/);
    h.setAppLanguage("de-DE");
    assert.equal(h.nodes.get("genycaster-update-title").textContent,"Genycaster-Update verfügbar");
    assert.equal(h.nodes.get("genycaster-update-now").textContent,"Zum Aktualisieren neu laden");
    h.setAppLanguage("en-US");
    assert.equal(h.nodes.get("genycaster-update-title").textContent,"Genycaster update available");
    assert.equal(h.nodes.get("genycaster-update-current").textContent,"v1.0.0");
    assert.equal(h.nodes.get("genycaster-update-next").textContent,"v1.0.1");
    assert.match(h.nodes.get("genycaster-update-details").textContent,/75\.0 MiB/);
    assert.equal(h.nodes.get("genycaster-update-banner").hidden,false);
    assert.equal(h.reloadedTo,"");
});

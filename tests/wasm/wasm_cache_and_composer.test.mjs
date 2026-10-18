import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,mkdirSync,writeFileSync,readFileSync,rmSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {readFileSync as read} from 'node:fs';
import vm from 'node:vm';

const root=new URL('../../',import.meta.url);
const pack=new URL('../../tools/package_wasm_assets.py',import.meta.url);
const worker=read(new URL('../../ui/wasm/genycaster-sw.js',import.meta.url),'utf8');
const html=read(new URL('../../ui/wasm/genycaster-shell.html',import.meta.url),'utf8');
const main=read(new URL('../../ui/components/AppShell.qml',import.meta.url),'utf8');

test('generated filename depends on WASM bytes, not unrelated JS updates',()=>{
    const dir=mkdtempSync(join(tmpdir(),'genycaster-assets-'));
    const run=(appVersion='1.0.0')=>{
        const result=spawnSync('python3',[pack.pathname,'--dir',dir,'--app-version',appVersion,'--release-label','Beta 1.0'],{encoding:'utf8'});
        assert.equal(result.status,0,result.stderr);
        return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    };
    try{
        writeFileSync(join(dir,'farcaster_client.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
        for(const file of ['farcaster_client.html','qtloader.js','farcaster_client.js','genycaster-loader.js','genycaster-sw.js'])
            writeFileSync(join(dir,file),'test-'+file);
        const a=run();
        assert.match(a.wasm,/^farcaster_client\.[0-9a-f]{20}\.wasm$/);
        assert.equal(a.appVersion,'1.0.0');
        assert.equal(a.releaseLabel,'Beta 1.0');
        assert.equal(a.wasmBytes,8);
        assert.match(a.buildId,/^[a-f0-9]{8}$/);
        assert.equal(a.buildId,a.version.slice(0,8));
        assert.ok(existsSync(join(dir,a.wasm)));
        const versionOnly=run('1.0.1');
        assert.equal(versionOnly.wasm,a.wasm,'semantic updates must not rename identical WASM bytes');
        assert.notEqual(versionOnly.version,a.version,'semantic version changes invalidate release cache identity');
        const a2=run();
        assert.equal(a2.wasm,a.wasm);assert.equal(a2.version,a.version);
        writeFileSync(join(dir,'genycaster-loader.js'),'updated loader');
        const b=run();
        assert.equal(b.wasm,a.wasm,'JS edit must not rename expensive WASM');
        assert.notEqual(b.version,a.version,'worker should update on JS edit');
        writeFileSync(join(dir,'farcaster_client.wasm'),Buffer.from([0,97,115,109,2,0,0,0]));
        const c=run();
        assert.notEqual(c.wasm,b.wasm);
        assert.ok(existsSync(join(dir,b.wasm)),'old clients need old hashed URL during update');
        const config=readFileSync(join(dir,'genycaster-assets.js'),'utf8');
        assert.ok(config.includes(c.wasm));
        assert.match(config,/"appVersion": "1.0.0"/);
        assert.match(config,/"releaseLabel": "Beta 1.0"/);
        assert.match(config,/"wasmBytes": 8/);
        assert.ok(config.includes(c.buildId));
        const packSource=spawnSync('python3',[pack.pathname,'--source',
            new URL('../../ui/wasm/',import.meta.url).pathname,'--dir',dir,'--app-version','1.0.0',
            '--release-label','Beta 1.0'],
            {encoding:'utf8'});
        assert.equal(packSource.status,0,packSource.stderr);
        const originalBrand=read(new URL('../../ui/assets/icons/genyleap/icons/brands/geny.svg',import.meta.url),'utf8');
        assert.equal(readFileSync(join(dir,'genycaster-brand.svg'),'utf8'),originalBrand,
            'boot asset must be the actual application SVG');
        const included=JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8')).files;
        assert.ok(included.includes('genycaster-brand.svg'));
        for(const name of ['apple-touch-icon.png','apple-touch-icon-precomposed.png']){
            const file=readFileSync(join(dir,name));
            assert.deepEqual([...file.subarray(0,8)],[137,80,78,71,13,10,26,10]);
            assert.equal(file.readUInt32BE(16),180);
            assert.equal(file.readUInt32BE(20),180);
            assert.ok(included.includes(name),name+' is distributed with the release');
        }
    }finally{rmSync(dir,{recursive:true,force:true});}
});

test('service worker caches only same-origin public files, never API requests',async()=>{
    const data=new Map();
    const responses=new Map();
    const callbacks={};
    let networkCount=0;
    let deleted=[];
    const manifest={version:'abcd',wasm:'farcaster_client.abc123abc123abc123ab.wasm',
        files:['farcaster_client.html','farcaster_client.js',
               'farcaster_client.abc123abc123abc123ab.wasm']};
    const base='https://app.example.com/';
    for(const name of manifest.files)
        responses.set(base+name,new Response('body for '+name,{headers:{'content-type':'application/wasm'}}));
    responses.set(base+'genycaster-assets.json',new Response(JSON.stringify(manifest)));
    const fakeCache={
        async match(url){return data.get(String(url))?.clone();},
        async put(url,response){data.set(String(url),response.clone());},
    };
    const scope={
        location:{origin:base.slice(0,-1),href:base+'genycaster-sw.js'},
        addEventListener(name,callback){callbacks[name]=callback;},
        skipWaiting(){},
        clients:{claim:async()=>{}},
    };
    const context={
        self:scope,URL,Response,Headers,
        caches:{
            open:async()=>fakeCache,
            keys:async()=>['genycaster-static-v3'],
            delete:async name=>{deleted.push(name);return true;}
        },
        fetch:async request=>{
            const url=String(request?.url||request);
            networkCount++;
            const result=responses.get(url);
            if(!result)throw new Error('offline');
            return result.clone();
        },
    };
    vm.runInNewContext(worker,context);
    let installed;
    callbacks.install({waitUntil:p=>{installed=p;}});
    await installed;
    const wasmURL=base+manifest.wasm;
    const request={url:wasmURL,method:'GET',credentials:'same-origin'};
    let answer;
    let background=[];
    const fetchEvent=request=>({
        request,respondWith:p=>{answer=p;},
        waitUntil:p=>{background.push(p);}
    });
    callbacks.fetch(fetchEvent(request));
    assert.equal(await (await answer).text(),'body for '+manifest.wasm);
    await Promise.all(background); // service-worker waitUntil commits the cloned body
    background=[];
    const count=networkCount;
    callbacks.fetch(fetchEvent(request));
    const cachedWasm=await answer;
    assert.equal(cachedWasm.headers.get('x-genycaster-cache'),null,
        'serve original browser response without rewriting headers');
    assert.equal(await cachedWasm.text(),'body for '+manifest.wasm);
    assert.equal(networkCount,count,'immutable WASM must come from local CacheStorage');
    responses.delete(base+'farcaster_client.html');
    callbacks.fetch(fetchEvent({url:base+'farcaster_client.html',method:'GET',
                        credentials:'same-origin'}));
    assert.match(await (await answer).text(),/body for farcaster_client.html/);
    let handled=false;
    callbacks.fetch({request:{url:base+'api/user',method:'GET',credentials:'same-origin'},
                    respondWith:()=>{handled=true;},waitUntil:p=>{background.push(p);}});
    assert.equal(handled,false,'API user data must not be cached');
    callbacks.fetch({request:{url:base+'private-session.json',
                        method:'GET',credentials:'same-origin'},
                    respondWith:()=>{handled=true;},waitUntil:p=>{background.push(p);}});
    assert.equal(handled,false,'same-origin private JSON must not be cached');
    callbacks.fetch({request:{url:'https://openproof.example.net/account',
                       method:'GET',credentials:'omit'},
                    respondWith:()=>{handled=true;},waitUntil:p=>{background.push(p);}});
    assert.equal(handled,false,'third-party identity requests must not be cached');
});

test('composer waits for confirmed cast hash before closing and keeps failed draft',()=>{
    assert.match(main,/presentationBridge\.writePhase === "confirmed"/);
    assert.match(main,/castId !== root\.submittedCastBaseline/);
    assert.match(main,/composerDrawer\.close\(\)/);
    assert.match(main,/presentationBridge\.writePhase === "failed"/);
    assert.doesNotMatch(main,/function onCastQueued\(\)[\s\S]{0,150}composerDrawer\.close\(\)/);
});

test('context menu is attached globally to all QML text areas and uses themed popup',()=>{
    const qml=read(new URL('../../ui/components/EditorContextMenu.qml',import.meta.url),'utf8');
    const areas=read(new URL('../../ui/components/DirectionAwareTextArea.qml',import.meta.url),'utf8');
    const code=read(new URL('../../ui/components/CodeBlockEditor.qml',import.meta.url),'utf8');
    assert.match(qml,/popupType: Popup\.Item/);
    assert.match(qml,/EditorMenuItem/);
    assert.doesNotMatch(qml,/delegate: MenuItem/);
    assert.match(qml,/background: Superellipse/);
    assert.match(areas,/ContextMenu\.menu: EditorContextMenu/);
    assert.match(code,/ContextMenu\.menu: EditorContextMenu/);
});

test('recovery failures are not mislabeled Genyleap login errors',()=>{
    const cpp=read(new URL('../../ui/mobile/main.cpp',import.meta.url),'utf8');
    assert.match(cpp,/route == QStringLiteral\("recovery"\)/);
    assert.match(cpp,/genycaster\.setup\.recovery_failed_retry/);
    assert.match(cpp,/if \(bridge\.authenticationRoute\(\) == QStringLiteral\("recovery"\)\) return;/);
    const js=read(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
    const route=js.split('if (action === "recover-account") {')[1]
        .split('} else {')[0];
    assert.ok(route.indexOf("promptForFarcasterRecovery(")<route.indexOf("createClient(base)"),
        'recovery form opens before network signer setup');
});

test('menu keeps hover label contrast independent of Qt MenuItem opacity',()=>{
    const qml=read(new URL('../../ui/components/EditorContextMenu.qml',import.meta.url),'utf8');
    const item=read(new URL('../../ui/components/EditorMenuItem.qml',import.meta.url),'utf8');
    assert.match(item,/Item \{/);
    assert.doesNotMatch(item,/^MenuItem \{/m);
    assert.match(item,/text: root\.label/);
    assert.match(item,/color: root\.actionEnabled \? Theme\.ink/);
    assert.match(item,/onClicked: root\.activate\(\)/);
    assert.match(qml,/EditorMenuItem/);
    assert.doesNotMatch(qml,/delegate: MenuItem/);
});

test('Safari touch icons and responsive stage indicators ship from one public root',()=>{
    const cmake=read(new URL('../../ui/CMakeLists.txt',import.meta.url),'utf8');
    assert.match(html,/rel="apple-touch-icon" sizes="180x180" href="\.\/apple-touch-icon\.png"/);
    assert.match(cmake,/apple-touch-icon-precomposed\.png/);
    assert.match(cmake,/apple-touch-icon\.png/);
    assert.doesNotMatch(html,/class="boot-journey/);
    assert.match(html,/Preparing Genycaster…/);
    assert.match(html,/id="boot-track"/);
});

test('bootstrap uses actual original Geny SVG, not a home-made DNA',()=>{
    assert.match(html,/genycaster-brand\.svg/);
    assert.match(html,/Powered by <strong>Genyleap<\/strong>/);
    assert.doesNotMatch(html,/helix-float|boot-card|qtlogo/);
});

test('release HTML cache-busts all boot scripts without changing WASM and is idempotent',()=>{
    const dir=mkdtempSync(join(tmpdir(),'genycaster-stamped-'));
    const scripts=['qtloader.js','farcaster_client.js','genycaster-assets.js','genycaster-loader.js'];
    const markup='<!doctype html><title>Genycaster Beta 1.0</title>'+
        scripts.map(name=>'<script src="'+name+'"></script>').join('');
    try {
        writeFileSync(join(dir,'farcaster_client.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
        for(const file of scripts)writeFileSync(join(dir,file),'test-'+file);
        writeFileSync(join(dir,'farcaster_client.html'),markup);
        const packOnce=()=>{
            const result=spawnSync('python3',[pack.pathname,'--dir',dir,
                '--app-version','1.0.0','--release-label','Beta 1.0'],{encoding:'utf8'});
            assert.equal(result.status,0,result.stderr);
            return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
        };
        const first=packOnce();
        let stamped=readFileSync(join(dir,'farcaster_client.html'),'utf8');
        for(const script of scripts)
            assert.ok(stamped.includes('src="'+script+'?v='+first.version+'"'));
        const digest=createHash('sha256').update(stamped).digest('hex');
        assert.equal(first.integrity['farcaster_client.html'],digest);
        const second=packOnce();
        assert.equal(first.version,second.version);
        assert.equal(readFileSync(join(dir,'farcaster_client.html'),'utf8'),stamped);
        writeFileSync(join(dir,'genycaster-loader.js'),'updated');
        const third=packOnce();
        assert.notEqual(third.version,first.version);
        assert.equal(third.wasm,first.wasm,'unchanged WASM stays cached');
        stamped=readFileSync(join(dir,'farcaster_client.html'),'utf8');
        assert.ok(stamped.includes('src="genycaster-loader.js?v='+third.version+'"'));
        assert.ok(!stamped.includes('src="genycaster-loader.js?v='+first.version+'"'));
    }finally{rmSync(dir,{recursive:true,force:true});}
});

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
    const run=()=>{
        const result=spawnSync('python3',[pack.pathname,'--dir',dir],{encoding:'utf8'});
        assert.equal(result.status,0,result.stderr);
        return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    };
    try{
        writeFileSync(join(dir,'farcaster_client.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
        for(const file of ['farcaster_client.html','qtloader.js','farcaster_client.js','genycaster-loader.js','genycaster-sw.js'])
            writeFileSync(join(dir,file),'test-'+file);
        const a=run();
        assert.match(a.wasm,/^farcaster_client\.[0-9a-f]{20}\.wasm$/);
        assert.ok(existsSync(join(dir,a.wasm)));
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
    callbacks.fetch({request,respondWith:p=>{answer=p;}});
    assert.equal(await (await answer).text(),'body for '+manifest.wasm);
    const count=networkCount;
    callbacks.fetch({request,respondWith:p=>{answer=p;}});
    const cachedWasm=await answer;
    assert.equal(cachedWasm.headers.get('x-genycaster-cache'),'hit');
    assert.equal(await cachedWasm.text(),'body for '+manifest.wasm);
    assert.equal(networkCount,count,'immutable WASM must come from local CacheStorage');
    responses.delete(base+'farcaster_client.html');
    callbacks.fetch({request:{url:base+'farcaster_client.html',method:'GET',
                        credentials:'same-origin'},respondWith:p=>{answer=p;}});
    assert.match(await (await answer).text(),/body for farcaster_client.html/);
    let handled=false;
    callbacks.fetch({request:{url:base+'api/user',method:'GET',credentials:'same-origin'},
                    respondWith:()=>{handled=true;}});
    assert.equal(handled,false,'API user data must not be cached');
    callbacks.fetch({request:{url:base+'private-session.json',
                        method:'GET',credentials:'same-origin'},
                    respondWith:()=>{handled=true;}});
    assert.equal(handled,false,'same-origin private JSON must not be cached');
    callbacks.fetch({request:{url:'https://openproof.example.net/account',
                       method:'GET',credentials:'omit'},
                    respondWith:()=>{handled=true;}});
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
    assert.match(qml,/delegate: MenuItem/);
    assert.match(qml,/background: Superellipse/);
    assert.match(areas,/ContextMenu\.menu: EditorContextMenu/);
    assert.match(code,/ContextMenu\.menu: EditorContextMenu/);
});

test('bootstrap uses static minimal SVG rather than animated DNA art or a card',()=>{
    assert.match(html,/<svg viewBox="0 0 56 56"/);
    assert.doesNotMatch(html,/helix-float|boot-card|qtlogo/);
});

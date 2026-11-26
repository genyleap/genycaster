import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,mkdirSync,writeFileSync,readFileSync,rmSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {readFileSync as read} from 'node:fs';
import vm from 'node:vm';

const root=new URL('../../',import.meta.url);
const pack=new URL('../../ci/package_wasm_assets.py',import.meta.url);
const worker=read(new URL('../../ui/wasm/genycaster-sw.js',import.meta.url),'utf8');
const html=read(new URL('../../ui/wasm/genycaster-shell.html',import.meta.url),'utf8');
const main=read(new URL('../../ui/components/shell/AppShell.qml',import.meta.url),'utf8');

test('generated filename depends on WASM bytes, not unrelated JS updates',()=>{
    const dir=mkdtempSync(join(tmpdir(),'genycaster-assets-'));
    const run=(appVersion='1.0.0')=>{
        const result=spawnSync('python3',[pack.pathname,'--dir',dir,'--app-version',appVersion,'--product-name','Genycaster','--version-number',appVersion,'--release-channel','stable','--release-channel-number','0'],{encoding:'utf8'});
        assert.equal(result.status,0,result.stderr);
        return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
    };
    try{
        writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
        for(const file of ['genycaster.html','qtloader.js','genycaster.js','genycaster-loader.js','genycaster-sw.js'])
            writeFileSync(join(dir,file),'test-'+file);
        const a=run();
        assert.match(a.wasm,/^genycaster\.[0-9a-f]{20}\.wasm$/);
        assert.equal(a.appVersion,'1.0.0');
        assert.equal(a.productName,'Genycaster');
        assert.equal(a.releaseChannel,'stable');
        assert.equal(a.versionNumber,'1.0.0');
        assert.equal(a.wasmBytes,8);
        assert.match(a.buildId,/^[a-f0-9]{8}$/);
        assert.equal(a.buildId,a.version.slice(0,8));
        assert.ok(existsSync(join(dir,a.wasm)));
        // Browsers can use the immutable .wasm.gz path directly. It must be
        // included in the release file list and integrity manifest, not
        // merely generated inside the CMake build directory.
        assert.equal(a.wasmGzip,a.wasm+'.gz');
        assert.ok(a.files.includes(a.wasmGzip),'gzip sidecar is deployable');
        assert.ok(existsSync(join(dir,a.wasmGzip)),'gzip sidecar exists');
        const sidecar=readFileSync(join(dir,a.wasmGzip));
        assert.equal(sidecar.length,a.wasmGzipBytes);
        assert.equal(createHash('sha256').update(sidecar).digest('hex'),
            a.wasmGzipSha256);
        assert.equal(a.integrity[a.wasmGzip],a.wasmGzipSha256,
            'gzip sidecar participates in release verification');
        assert.deepEqual(gunzipSync(sidecar),readFileSync(join(dir,'genycaster.wasm')),
            'gzip sidecar decodes to the original executable');
        const versionOnly=run('1.0.1');
        assert.equal(versionOnly.wasm,a.wasm,'semantic updates must not rename identical WASM bytes');
        assert.notEqual(versionOnly.version,a.version,'semantic version changes invalidate release cache identity');
        const a2=run();
        assert.equal(a2.wasm,a.wasm);assert.equal(a2.version,a.version);
        writeFileSync(join(dir,'genycaster-loader.js'),'updated loader');
        const b=run();
        assert.equal(b.wasm,a.wasm,'JS edit must not rename expensive WASM');
        assert.notEqual(b.version,a.version,'worker should update on JS edit');
        writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,2,0,0,0]));
        const c=run();
        assert.notEqual(c.wasm,b.wasm);
        assert.notEqual(c.version,b.version,
            'WASM-only changes must invalidate release and boot-script cache keys');
        assert.ok(existsSync(join(dir,b.wasm)),'old clients need old hashed URL during update');
        const config=readFileSync(join(dir,'genycaster-assets.js'),'utf8');
        assert.ok(config.includes(c.wasm));
        assert.match(config,/"appVersion": "1.0.0"/);
        assert.match(config,/"releaseChannel": "stable"/);
        assert.match(config,/"wasmBytes": 8/);
        assert.ok(config.includes(c.buildId));
        const packSource=spawnSync('python3',[pack.pathname,'--source',
            new URL('../../ui/wasm/',import.meta.url).pathname,'--dir',dir,'--app-version','1.0.0',
            '--product-name','Genycaster','--version-number','1.0.0','--release-channel','stable','--release-channel-number','0'],
            {encoding:'utf8'});
        assert.equal(packSource.status,0,packSource.stderr);
        const originalBrand=read(new URL('../../ui/wasm/genycaster-icon.svg',import.meta.url),'utf8');
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
    const manifest={version:'abcd',wasm:'genycaster.abc123abc123abc123ab.wasm',
        files:['genycaster.html','genycaster.js',
               'genycaster.abc123abc123abc123ab.wasm']};
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
    let wasmIntercepted=false;
    callbacks.fetch({request,respondWith:()=>{wasmIntercepted=true;},
                     waitUntil:p=>{background.push(p);}});
    assert.equal(wasmIntercepted,false,
        'large WASM must use the native browser fetch instead of respondWith');
    callbacks.fetch({request:{url:base+'genycaster.wasm',
                         method:'GET',credentials:'same-origin'},
                     respondWith:()=>{wasmIntercepted=true;},
                     waitUntil:p=>{background.push(p);}});
    assert.equal(wasmIntercepted,false,'non-hashed WASM alias must bypass worker');
    assert.equal(data.has(wasmURL),false,'never clone 80 MiB WASM into CacheStorage');
    const count=networkCount;
    callbacks.fetch(fetchEvent(request));
    assert.equal(networkCount,count,'worker must not attempt WASM network fetch');
    responses.delete(base+'genycaster.html');
    callbacks.fetch(fetchEvent({url:base+'genycaster.html',method:'GET',
                        credentials:'same-origin'}));
    assert.match(await (await answer).text(),/body for genycaster.html/);
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

test('shared text controls use native platform menus and browser edit adapter',()=>{
 const qml=read(new URL('../../ui/components/composer/EditorContextMenu.qml',import.meta.url),'utf8');
 const areas=read(new URL('../../ui/components/controls/BidiTextArea.qml',import.meta.url),'utf8');
 const code=read(new URL('../../ui/components/composer/CodeBlockEditor.qml',import.meta.url),'utf8');
 assert.doesNotMatch(qml,/popupType:|MenuItem \{/);
 assert.match(qml,/function mountBrowserEditor\(\)/);
 assert.doesNotMatch(areas,/ContextMenu\.menu:/);
 assert.match(code,/DirectionAwareTextArea \{\s+id: codeEditor/);
 assert.match(qml,/presentationBridge\.openBrowserTextEditor/);
});

test('recovery failures are not mislabeled Genyleap login errors',()=>{
    const cpp=read(new URL('../../ui/app/main.cpp',import.meta.url),'utf8') + '\n' +
        read(new URL('../../ui/app/runtime/bindings.cpp',import.meta.url),'utf8');
    assert.match(cpp,/route == QStringLiteral\("recovery"\)/);
    assert.match(cpp,/genycaster\.setup\.recovery_failed_retry/);
    assert.match(cpp,/event\.phase != BrowserOpenProofState::Phase::SignedOut/);
    const js=read(new URL('../../ui/wasm/protocol-signer-pre.js',import.meta.url),'utf8');
    const open=js.indexOf("const recoveredFid=await consumeRecovery(");
    const lookup=js.indexOf("await import('./farcaster_custody_recovery.mjs')",open);
    assert.ok(open>=0 && lookup>open,
        'recovery form opens before read-only identity lookup');
    const recovery=js.slice(open,js.indexOf("owner = recoveredFid;",open));
    const verified=recovery.indexOf('onVerifiedCustody:async');
    assert.ok(verified>0);
    assert.doesNotMatch(recovery.slice(0,verified),/await createClient\(base\)|provisionFromRecovery\(/);
    assert.match(recovery.slice(verified),/provisionFromRecovery\(verifiedFid,phrase\)/);
});

test('browser-native editor uses accessible real DOM controls and never injects markup',()=>{
 const js=read(new URL('../../ui/wasm/native-text-editor-pre.js',import.meta.url),'utf8');
 assert.match(js,/genycaster-native-input/);
 assert.match(js,/input.setAttribute\('aria-label'/);
 assert.doesNotMatch(js,/showModal|createElement\(.dialog./);
 assert.match(js,/input.value=value/);
 assert.doesNotMatch(js,/innerHTML|localStorage|sessionStorage|navigator.clipboard.readText/);
});

test('Safari touch icons and responsive stage indicators ship from one public root',()=>{
    const cmake=read(new URL('../../ui/CMakeLists.txt',import.meta.url),'utf8');
    assert.match(html,/rel="apple-touch-icon" sizes="180x180" href="\.\/apple-touch-icon\.png"/);
    assert.match(cmake,/apple-touch-icon-precomposed\.png/);
    assert.match(cmake,/apple-touch-icon\.png/);
    assert.doesNotMatch(html,/class="boot-journey/);
    assert.match(html,/Preparing Genycaster…/);
    assert.match(html,/Genyleap Software Revolution/);
    assert.match(html,/src="\.\/intro-farcaster\.svg"/);
    assert.match(html,/id="boot-track"/);
});

test('bootstrap uses actual original Geny SVG, not a home-made DNA',()=>{
    assert.match(html,/genycaster-wordmark-dark\.svg/);
    assert.match(html,/Powered by <strong>Genyleap<\/strong>/);
    assert.doesNotMatch(html,/helix-float|boot-card|qtlogo/);
});

test('release HTML cache-busts all boot scripts without changing WASM and is idempotent',()=>{
    const dir=mkdtempSync(join(tmpdir(),'genycaster-stamped-'));
    const scripts=['qtloader.js','genycaster.js','genycaster-assets.js','genycaster-loader.js'];
    const markup='<!doctype html><title>Genycaster Beta 1.0</title>'+
        scripts.map(name=>'<script src="'+name+'"></script>').join('');
    try {
        writeFileSync(join(dir,'genycaster.wasm'),Buffer.from([0,97,115,109,1,0,0,0]));
        for(const file of scripts)writeFileSync(join(dir,file),'test-'+file);
        writeFileSync(join(dir,'genycaster.html'),markup);
        const packOnce=()=>{
            const result=spawnSync('python3',[pack.pathname,'--dir',dir,
                '--app-version','1.0.0','--product-name','Genycaster','--version-number','1.0.0','--release-channel','stable','--release-channel-number','0'],{encoding:'utf8'});
            assert.equal(result.status,0,result.stderr);
            return JSON.parse(readFileSync(join(dir,'genycaster-assets.json'),'utf8'));
        };
        const first=packOnce();
        let stamped=readFileSync(join(dir,'genycaster.html'),'utf8');
        for(const script of scripts)
            assert.ok(stamped.includes('src="'+script+'?v='+first.version+'"'));
        const digest=createHash('sha256').update(stamped).digest('hex');
        assert.equal(first.integrity['genycaster.html'],digest);
        const second=packOnce();
        assert.equal(first.version,second.version);
        assert.equal(readFileSync(join(dir,'genycaster.html'),'utf8'),stamped);
        writeFileSync(join(dir,'genycaster-loader.js'),'updated');
        const third=packOnce();
        assert.notEqual(third.version,first.version);
        assert.equal(third.wasm,first.wasm,'unchanged WASM stays cached');
        stamped=readFileSync(join(dir,'genycaster.html'),'utf8');
        assert.ok(stamped.includes('src="genycaster-loader.js?v='+third.version+'"'));
        assert.ok(!stamped.includes('src="genycaster-loader.js?v='+first.version+'"'));
    }finally{rmSync(dir,{recursive:true,force:true});}
});


test('service worker accepts more than 40 release assets and survives a blocked manifest request',async()=>{
    const registrations={};
    const cached=[];
    const base='https://genycaster.xyz/app/';
    const fakeCache={async match(){return null;},
        async put(url){cached.push(String(url));}};
    const runtime={
        self:{location:{origin:'https://genycaster.xyz',href:base+'genycaster-sw.js'},
            addEventListener:(name,handler)=>{registrations[name]=handler;},
            skipWaiting(){},clients:{claim:async()=>{}}},
        URL,Response,Headers,
        caches:{open:async()=>fakeCache,keys:async()=>[],delete:async()=>true},
        console:{warn:()=>{}},
    };
    // Each fresh SW script works on an arbitrary HTTPS origin and subpath.
    let mode='large';
    runtime.fetch=async request=>{
        const url=String(request?.url||request);
        if(mode==='blocked')throw new Error('ERR_BLOCKED_BY_CLIENT');
        if(url===base+'genycaster-assets.json')
            return new Response(JSON.stringify({files:[
                'genycaster.html','miniapp-host-bridge.mjs',
                ...Array.from({length:41},(_,i)=>'private-'+i+'.json'),
                'genycaster.abc123abc123abc123ab.wasm'
            ]}));
        return new Response('public content',{headers:{'cache-control':'public'}});
    };
    vm.runInNewContext(worker,runtime);
    assert.match(worker,/genycaster-static-v17/);
    let promise;
    registrations.install({waitUntil:task=>{promise=task;}});
    await assert.doesNotReject(promise);
    assert.ok(cached.includes(base+'genycaster.html'));
    assert.ok(cached.includes(base+'miniapp-host-bridge.mjs'));
    assert.equal(cached.some(url=>url.includes('private-')),false);
    assert.equal(cached.some(url=>url.endsWith('.wasm')),false);
    mode='blocked';
    registrations.install({waitUntil:task=>{promise=task;}});
    await assert.doesNotReject(promise,'ad blockers must not abort SW installation');
    const handled=[];
    registrations.fetch({request:{url:base+'api/session',method:'GET',credentials:'same-origin'},
        respondWith:value=>handled.push(value),waitUntil:()=>{}});
    assert.equal(handled.length,0);
});

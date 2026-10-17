import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const root=new URL('../../',import.meta.url);
const read=path=>readFileSync(new URL(path,root),'utf8');
const html=read('ui/wasm/genycaster-shell.html');
const loader=read('ui/wasm/genycaster-loader.js');

test('WASM packages branded shell instead of Qt logo; mobile uses visible viewport',()=>{
    const cmake=read('ui/CMakeLists.txt');
    assert.match(cmake,/genycaster-shell\.html/);
    assert.match(cmake,/farcaster_client\.html/);
    assert.match(cmake,/genycaster-loader\.js/);
    assert.match(html,/<meta name="viewport".*viewport-fit=cover/);
    assert.match(html,/<div id="qt-screen"/);
    assert.match(html,/<script src="qtloader\.js">/);
    assert.match(html,/<script src="farcaster_client\.js">/);
    assert.doesNotMatch(html,/qtlogo/);
    assert.match(loader,/visualViewport/);
});
test('completed Setup is independent of a volatile session and signer revocation',()=>{
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    const auth=bridge.split('void PresentationBridge::applyAuthenticationState(')[1]
        .split('void PresentationBridge::applyAuthenticationError(')[0];
    assert.doesNotMatch(auth,/resetInitialSetupProgress\(/);
    const signer=bridge.split('void PresentationBridge::applySignerState(')[1]
        .split('void PresentationBridge::applyVerifiedBrowserWrite(')[0];
    assert.doesNotMatch(signer,/resetInitialSetupProgress\(/);
    const app=read('ui/mobile/main.cpp');
    assert.match(app,/\/home\/web_user\/\.local\/share\/genycaster-settings\.ini/);
    assert.match(app,/QObject::connect\(&presentationBridge, &PresentationBridge::setupStateChanged/);
    assert.doesNotMatch(app,/QObject::connect\(&bridge, &PresentationBridge::setupStateChanged/);
    assert.match(app,/FS\.syncfs\(false/);
    const pre=read('ui/wasm/persistence-pre.js');
    assert.match(pre,/FS\.syncfs\(true/);
    const observer=app.split('const auto sessionObserver = ')[1]
        .split('// A browser-safe OpenProof device grant')[0];
    assert.doesNotMatch(observer,/restore-for-setup/);
    assert.match(observer,/browserSigner->start\(QStringLiteral\("restore"\)/);
    const setup=read('ui/components/InitialSetup.qml');
    assert.match(setup,/genycaster\.setup\.viewer_description/);
    assert.match(setup,/id: setupFooter/);
});
function fakeElement() {
    const classes=new Set();
    return {
        style:{},textContent:'',hidden:false,
        classList:{
            add(value){classes.add(value);},
            remove(value){classes.delete(value);},
            contains(value){return classes.has(value);}
        },
        addEventListener(){},
    };
}
test('WASM boot shows measured download bytes, then hides only on Qt onLoaded',async()=>{
    const elements=new Map([
        'genycaster-boot','boot-heading','boot-status','boot-stage',
        'boot-fill','boot-percent','boot-retry','qt-screen'
    ].map(name=>[name,fakeElement()]));
    const collected=[];
    let loadedCallback;
    const memory=new Map();
    const sample=new Uint8Array([0,1,2,3,4,5,6,7]);
    const scope={
        document:{getElementById:id=>elements.get(id)},
        localStorage:{getItem:k=>memory.get(k),setItem:(k,v)=>memory.set(k,v)},
        window:{
            fetch:async()=>new Response(sample,{
                headers:{'content-type':'application/wasm','content-length':'8'}
            }),
            addEventListener(){},
            visualViewport:{height:640,addEventListener(){}},
            innerHeight:680
        },
        location:{href:'https://example.test/farcaster_client.html'},
        qtLoad:async config=>{loadedCallback=config.qt.onLoaded;},
        console,URL,Response,Headers,ReadableStream,
    };
    scope.window.setTimeout=callback=>{callback();return 0;};
    vm.runInNewContext(loader,scope);
    assert.equal(elements.get('qt-screen').style.height,'640px');
    assert.equal(elements.get('genycaster-boot').hidden,false);
    const wasm=await scope.window.fetch('https://example.test/farcaster_client.wasm');
    const received=new Uint8Array(await wasm.arrayBuffer());
    assert.deepEqual(received,sample);
    assert.equal(elements.get('boot-percent').textContent,'100%');
    assert.equal(elements.get('boot-fill').style.width,'100%');
    loadedCallback();
    assert.equal(elements.get('genycaster-boot').hidden,true);
    assert.equal(memory.get('genycaster.boot.loaded.v1'),'1');
    assert.equal(collected.length,0);
});

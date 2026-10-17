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
    assert.match(cmake,/browser_device_session\.mjs/);
    assert.match(cmake,/package_wasm_assets\.py/);
    assert.match(cmake,/genycaster-sw\.js/);
    assert.match(html,/<meta name="viewport".*viewport-fit=cover/);
    assert.match(html,/<div id="qt-screen"/);
    assert.match(html,/<script src="qtloader\.js">/);
    assert.match(html,/<script src="farcaster_client\.js">/);
    assert.doesNotMatch(html,/qtlogo/);
    assert.match(html,/class="boot-glyph"/);
    assert.match(html,/url\("\.\/genycaster-brand\.svg"\)/);
    assert.match(html,/Powered by <strong>Genyleap<\/strong>/);
    assert.match(html,/class="boot-content"/);
    assert.doesNotMatch(html,/<div class="boot-card"/);
    assert.match(html,/src="genycaster-assets.js"/);
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
    assert.match(app,/browserSigner->restoreRememberedAccount\(\)/);
    assert.match(app,/browserSigner->forgetAccount\(\)/);
    assert.match(app,/genycaster:device-local/);
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
        style:{},textContent:'',hidden:false,dataset:{},
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
    let passedFactory;
    const factory=()=>({});
    const memory=new Map();
    const sample=new Uint8Array([0,1,2,3,4,5,6,7]);
    const hashedWasm='farcaster_client.0123456789abcdef0123.wasm';
    let requestedWasm='', requestedOptions={};
    const timingEntries=new Map();
    let observeResources=null;
    let monitorDependencies=null;
    const nativeResponse=new Response(sample,{
        headers:{'content-type':'application/wasm','content-length':'8'}
    });
    const scope={
        document:{getElementById:id=>elements.get(id),
            documentElement:{setAttribute(){}},
            querySelector:()=>({setAttribute(){}})},
        localStorage:{getItem:k=>memory.get(k),setItem:(k,v)=>memory.set(k,v)},
        window:{
            farcaster_client_entry:factory,
            __genycasterAssets:{wasm:hashedWasm},
            fetch:async(input,options)=>{
                requestedWasm=String(input);requestedOptions=options;
                return nativeResponse;
            },
            addEventListener(){},
            visualViewport:{height:640,addEventListener(){}},
            innerHeight:680
        },
        location:{href:'https://example.test/farcaster_client.html',origin:'https://example.test'},
        performance:{
            now:()=>10,
            getEntriesByName:url=>timingEntries.get(url)||[]
        },
        PerformanceObserver:class {
            constructor(callback){this.callback=callback;}
            observe(){observeResources=this.callback;}
            disconnect(){}
        },
        qtLoad:async config=>{
            monitorDependencies=config.monitorRunDependencies;
            passedFactory=config.qt.entryFunction;
            assert.equal(config.qt.containerElements[0],elements.get('qt-screen'));
            assert.equal(typeof config.qt.entryFunction,'function');
            loadedCallback=config.qt.onLoaded;
        },
        console,URL,Response,Headers,ReadableStream,
    };
    scope.window.setTimeout=callback=>{callback();return 0;};
    vm.runInNewContext(loader,scope);
    assert.equal(elements.get('qt-screen').style.height,'640px');
    assert.equal(passedFactory,factory);
    assert.equal(elements.get('genycaster-boot').hidden,false);
    const wasm=await scope.window.fetch('https://example.test/farcaster_client.wasm');
    assert.equal(wasm,nativeResponse,'Native WASM response must remain streamable');
    assert.equal(requestedWasm,'https://example.test/'+hashedWasm);
    assert.equal(requestedOptions.cache,'force-cache');
    assert.equal(requestedOptions.credentials,'omit');
    const received=new Uint8Array(await wasm.arrayBuffer());
    assert.deepEqual(received,sample);
    // An 8-byte native response can complete immediately: the optional
    // first-run clone accurately moves Download → Install on full consumption.
    assert.equal(elements.get('genycaster-boot').dataset.phase,'install');
    assert.equal(elements.get('boot-stage').textContent,'Installing runtime');
    const entry={name:'https://example.test/'+hashedWasm,responseEnd:20,transferSize:8,decodedBodySize:8};
    timingEntries.set(entry.name,[entry]);
    observeResources({getEntries:()=>[entry]});
    assert.equal(elements.get('genycaster-boot').dataset.phase,'install');
    assert.equal(elements.get('boot-stage').textContent,'Installing runtime');
    monitorDependencies(0);
    assert.equal(elements.get('genycaster-boot').dataset.phase,'interface');
    assert.equal(elements.get('boot-heading').textContent,'Opening Social Interface');
    assert.equal(elements.get('boot-stage').textContent,'Opening interface');
    assert.equal(elements.get('boot-percent').textContent,'');
    assert.equal(elements.get('genycaster-boot').hidden,false);
    loadedCallback();
    assert.equal(elements.get('genycaster-boot').hidden,true);
    assert.equal(memory.get('genycaster.boot.loaded.v1'),'1');
    assert.equal(elements.get('genycaster-boot').dataset.phase,'ready');
    assert.equal(collected.length,0);
});

test('missing Qt 6.12 application entry fails explicitly before qtLoad',()=>{
    const ids=['genycaster-boot','boot-heading','boot-status','boot-stage',
               'boot-fill','boot-percent','boot-retry','qt-screen'];
    const elements=new Map(ids.map(id=>[id,fakeElement()]));
    let invoked=false;
    const messages=[];
    const scope={
        document:{getElementById:id=>elements.get(id),
            documentElement:{setAttribute(){}},
            querySelector:()=>({setAttribute(){}})},
        localStorage:{getItem:()=>null},
        window:{
            fetch:async()=>{throw new Error('not reached');},
            addEventListener(){},innerHeight:640,
        },
        location:{href:'https://example.test/farcaster_client.html'},
        qtLoad:async()=>{invoked=true;},
        performance:{now:()=>0,getEntriesByName:()=>[]},
        console:{error:(...parts)=>messages.push(parts)},
        URL,Response,Headers,ReadableStream,
    };
    vm.runInNewContext(loader,scope);
    assert.equal(invoked,false);
    assert.match(elements.get('boot-status').textContent,/farcaster_client_entry/);
    assert.equal(elements.get('boot-retry').classList.contains('visible'),true);
    assert.equal(messages.length,1);
});


test('explicit Logout returns to account Setup while refresh keeps completed onboarding',()=>{
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    const header=read('src/presentation/qt/presentation_bridge.hpp');
    const app=read('ui/components/AppShell.qml');
    const setup=read('ui/components/InitialSetup.qml');
    assert.match(header,/m_signOutRequestedByUser/);
    assert.match(bridge,/setup\/showAfterSignOut/);
    assert.match(bridge,/m_authenticationPhase == QStringLiteral\("signed-out"\)/);
    assert.match(bridge,/m_showSetupAfterSignOut = true/);
    assert.match(bridge,/m_settings->remove\(QStringLiteral\("setup\/showAfterSignOut"\)\)/);
    assert.match(bridge,/return !m_setupBypassedForVerification\s*&& \(!m_initialSetupCompleted \|\| m_showSetupAfterSignOut\)/);
    assert.match(app,/returningAfterLogout: presentationBridge\.initialSetupCompleted/);
    assert.match(setup,/switchStep\(returningAfterLogout \? 4 : 0\)/);
    assert.doesNotMatch(bridge.split('void PresentationBridge::applyAuthenticationState(')[1]
        .split('void PresentationBridge::applyAuthenticationError(')[0],
        /resetInitialSetupProgress\(/);
});

test('Setup language cards and single in-place Terms/Privacy checkbox remain accessible',()=>{
    const h=read('ui/components/SetupPageHeader.qml');
    const l=read('ui/components/SetupLanguageSelector.qml');
    const setup=read('ui/components/InitialSetup.qml');
    const app=read('ui/components/AppShell.qml');
    assert.match(h,/RowLayout \{/);
    assert.match(h,/Layout\.alignment: Qt\.AlignVCenter/);
    assert.match(l,/id: languageGrid/);
    assert.match(l,/id: search/);
    assert.match(l,/id: previewTab/);
    assert.match(l,/Accessible\.role: Accessible\.RadioButton/);
    assert.match(l,/Theme\.uiDirection/);
    assert.match(setup,/id: privacyStage/);
    assert.match(setup,/qsTrId\("genycaster\.setup\.terms_body"\)/);
    assert.match(setup,/qsTrId\("genycaster\.setup\.privacy_body"\)/);
    assert.match(setup,/qsTrId\("genycaster\.setup\.single_legal_consent"\)/);
    assert.match(setup,/Qt\.openUrlExternally/);
    assert.equal((setup.match(/\bCheckBox \{/g)||[]).length,1);
    assert.doesNotMatch(setup,/id: legalReader|legalReaderReachedEnd|openLegalReader/);
    assert.match(setup,/root\.legalTermsChecked = checked/);
    assert.match(setup,/root\.legalPrivacyChecked = checked/);
    assert.match(app,/BottomNavigation \{[\s\S]*?visible: root\.mobile && !root\.initialSetupOpen/);
    assert.doesNotMatch(app,/parent: readingPane/);
    assert.match(app,/anchors\.bottom: readingPane\.bottom/);
    assert.ok(app.indexOf("id: mobileBottomNavigation") < app.indexOf("id: contextResizeHandle"),
        "mobile dock must be declared inside readingPane, not in RowLayout");
    assert.match(app,/anchors\.bottomMargin: root\.mobile/);
});

test('recovery phrase uses trailing-edge desktop drawer and mobile bottom sheet',()=>{
    const dialog=read('ui/wasm/recovery-phrase-dialog.mjs');
    assert.match(dialog,/justify-content:flex-end/);
    assert.match(dialog,/width:min\(540px,100%\)/);
    assert.match(dialog,/@media\(max-width:680px\)/);
    assert.match(dialog,/genycaster-sheet-in/);
    assert.match(dialog,/position:sticky;bottom:-1px/);
    assert.match(dialog,/inputs\[index\]\?\.setAttribute\('aria-invalid'/);
    assert.doesNotMatch(dialog,/localStorage\.setItem\(/);
});


test('mobile navigation belongs to readingPane, not the outer RowLayout or Qt root',()=>{
    const app=read('ui/components/AppShell.qml');
    const start=app.indexOf('Item {\n                            id: readingPane');
    const dock=app.indexOf('BottomNavigation {\n                            id: mobileBottomNavigation');
    const handle=app.indexOf('Item {\n                            id: contextResizeHandle');
    assert.ok(start>=0 && dock>start && handle>dock);
    assert.doesNotMatch(app.slice(dock,handle),/parent:\s*(root|readingPane)/,
        'avoid runtime reparenting of layout-owned QML controls');
    assert.match(app.slice(dock,handle),/anchors\.bottom: readingPane\.bottom/);
    assert.match(app.slice(dock,handle),/blurSourceItem: readingContent/);
    assert.match(app,/anchors\.bottomMargin: root\.mobile\s*\?\s*mobileBottomNavigation\.height/);
});

test('Qt button rings use keyboard focus, never persistent pointer-focus borders',()=>{
    const theme=read('ui/theme/Theme.qml');
    assert.match(theme,/function keyboardFocusVisible\(control\)/);
    assert.match(theme,/Qt\.TabFocusReason/);
    assert.match(theme,/Qt\.BacktabFocusReason/);
    for(const file of ['SetupPrimaryButton','ActionButton','NavItem','GenyTabButton',
        'GenySwitch','EngagementButton','GenyComboBox','SetupActionCard']){
        const code=read('ui/components/'+file+'.qml');
        assert.match(code,/Theme\.keyboardFocusVisible\(/,file);
        assert.doesNotMatch(code,/borderWidth: [\w]+\.activeFocus/,file);
    }
});

test('WASM splash keeps all stage circles fixed and shows release version and build',()=>{
    const cmake=read('ui/CMakeLists.txt');
    const pack=read('tools/package_wasm_assets.py');
    assert.match(html,/\.boot-dot\{[^}]*flex:0 0 18px/);
    assert.match(html,/\.boot-journey li\{[^}]*flex:0 0 auto/);
    assert.doesNotMatch(html,/box-shadow:0 0 0 4px/);
    assert.doesNotMatch(html,/id="boot-version"/);
    assert.match(html,/<sup class="boot-beta">Beta 1\.0<\/sup>/);
    assert.match(html,/id="boot-note"/);
    assert.match(html,/id="boot-build-credit"/);
    assert.match(cmake,/--app-version "\$\{PROJECT_VERSION\}"/);
    assert.match(pack,/"buildId": fingerprint\[:8\]/);
    assert.doesNotMatch(loader,/versionLabel/);
    assert.match(loader,/if\(bootNote\)bootNote\.textContent/);
    assert.match(loader,/if\(creditBuild\)creditBuild\.textContent/);
});

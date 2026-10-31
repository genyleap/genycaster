import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const root=new URL('../../',import.meta.url);
const read=path=>readFileSync(new URL(path,root),'utf8');
const html=read('ui/wasm/genycaster-shell.html');
const loader=read('ui/wasm/genycaster-loader.js');

test('serialized IDBFS writes finish before refresh and recover after storage failure', async () => {
    const pre=read('ui/wasm/persistence-pre.js');
    const callbacks=[];
    const scope={
        Module:{}, Promise, FS:{
            syncfs(populate,done) {
                assert.equal(populate,false);
                callbacks.push(done);
            }
        }, console
    };
    vm.runInNewContext(pre,scope);
    const flush=scope.Module.genycasterSyncBrowserData;
    assert.equal(typeof flush,'function');

    const first=flush();
    const second=flush();
    await Promise.resolve();
    assert.equal(callbacks.length,1,'second sync must wait, IDBFS forbids overlap');
    callbacks.shift()(null);
    await first;
    await Promise.resolve();
    assert.equal(callbacks.length,1);
    callbacks.shift()(null);
    await second;

    const failed=flush();
    await Promise.resolve();
    callbacks.shift()(new Error('quota exceeded'));
    await assert.rejects(failed,/quota exceeded/);
    const retry=flush();
    await Promise.resolve();
    assert.equal(callbacks.length,1);
    callbacks.shift()(null);
    await retry;
});

test('WASM refresh flushes selected node to IndexedDB before navigation',()=>{
    const app=readGenycasterMain();
    const update=read('ui/mobile/update_controller.cpp');
    const panel=read('ui/components/NodeManagementPanel.qml');
    assert.ok(app.includes('ProviderProfilesController::selectionChanged'));
    assert.ok(app.includes('ProviderProfilesController::profilesChanged'));
    assert.ok(app.includes('flushBrowserData'));
    assert.ok(app.includes('Module.genycasterSyncBrowserData()'));
    const refresh=update.split('void UpdateController::refreshWebApp()')[1]
        .split('void UpdateController::check(')[0];
    assert.ok(refresh.includes('m_settings.sync()'));
    assert.ok(refresh.includes('Module.genycasterSyncBrowserData().then'));
    assert.match(refresh,/genycasterWebRefreshSaveFailure/);
    assert.ok(refresh.indexOf('genycasterSyncBrowserData().then')
              < refresh.indexOf('window.location.replace'));
    assert.match(panel,/genycaster.nodes.pending_read/);
    assert.match(panel,/genycaster.nodes.pending_tag/);
    assert.match(panel,/genycaster.nodes.active_tag/);
});

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
    assert.match(html,/class="boot-glyph" src="\.\/genycaster-boot-icon\.svg"/);
    assert.match(html,/Powered by <strong>Genyleap<\/strong>/);
    assert.match(html,/class="boot-content"/);
    assert.doesNotMatch(html,/<div class="boot-card"/);
    assert.match(html,/src="genycaster-assets.js"/);
    assert.match(loader,/visualViewport/);
});
test('Setup owns a borderless full-window stage with CTA in the stage, not a global footer',()=>{
    const setup=read('ui/components/InitialSetup.qml');
    const language=read('ui/components/SetupLanguageSelector.qml');
    const main=read('ui/mobile/Main.qml');
    assert.match(setup,/id: stageScroll/);
    assert.match(setup,/id: setupCanvas/);
    assert.match(setup,/id: desktopBrandPane/);
    assert.match(setup,/id: stageAction/);
    assert.match(setup,/anchors\.bottom: parent\.bottom/);
    assert.doesNotMatch(setup,/Layout\.preferredHeight: visible \? \(root\.phone \? 82 : 86\) : 0/);
    assert.match(setup,/onClicked: root\.previousStep\(\)/);
    assert.match(language,/objectName: "setupLanguageWheel"/);
    assert.match(language,/highlightRangeMode: ListView\.StrictlyEnforceRange/);
    assert.match(language,/wrapMode: Text\.WordWrap/);
    assert.match(main,/minimumHeight: Qt\.platform\.os === "wasm" \? 0 : 640/);
});

test('completed Setup is independent of a volatile session and signer revocation',()=>{
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    const auth=bridge.split('void PresentationBridge::applyAuthenticationState(')[1]
        .split('void PresentationBridge::applyAuthenticationError(')[0];
    assert.doesNotMatch(auth,/resetInitialSetupProgress\(/);
    const signer=bridge.split('void PresentationBridge::applySignerState(')[1]
        .split('void PresentationBridge::applyVerifiedBrowserWrite(')[0];
    assert.doesNotMatch(signer,/resetInitialSetupProgress\(/);
    const app=readGenycasterMain();
    assert.match(app,/\/home\/web_user\/\.local\/share\/genycaster-settings\.ini/);
    assert.match(app,/QObject::connect\(&presentationBridge, &PresentationBridge::setupStateChanged/);
    assert.doesNotMatch(app,/QObject::connect\(&bridge, &PresentationBridge::setupStateChanged/);
    assert.match(app,/Module\.genycasterSyncBrowserData\(\)/);
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
    assert.match(setup,/root\.guestSelected = true/);
    assert.match(setup,/signal closeReviewRequested\(\)/);
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
    assert.equal(elements.get('boot-stage').textContent,'Preparing WebAssembly resources');
    const entry={name:'https://example.test/'+hashedWasm,responseEnd:20,transferSize:8,decodedBodySize:8};
    timingEntries.set(entry.name,[entry]);
    observeResources({getEntries:()=>[entry]});
    assert.equal(elements.get('genycaster-boot').dataset.phase,'install');
    assert.equal(elements.get('boot-stage').textContent,'Preparing WebAssembly resources');
    monitorDependencies(0);
    assert.equal(elements.get('genycaster-boot').dataset.phase,'interface');
    assert.equal(elements.get('boot-heading').textContent,'Opening Genycaster…');
    assert.equal(elements.get('boot-stage').textContent,'Almost ready');
    assert.equal(elements.get('boot-percent').textContent,'');
    assert.equal(elements.get('genycaster-boot').hidden,false);
    loadedCallback();
    assert.equal(elements.get('genycaster-boot').hidden,false,
        'Qt runtime readiness must not hide the boot screen before QML exists');
    scope.window.__genycasterQmlStartup(true, '');
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
    assert.match(setup,/switchStep\(0\)/);
    assert.doesNotMatch(bridge.split('void PresentationBridge::applyAuthenticationState(')[1]
        .split('void PresentationBridge::applyAuthenticationError(')[0],
        /resetInitialSetupProgress\(/);
});

test('eight-stage Setup preserves language wheel, legal consent and navigation',()=>{
    const setup=read('ui/components/InitialSetup.qml');
    const picker=read('ui/components/SetupLanguageSelector.qml');
    const app=read('ui/components/AppShell.qml');
    assert.match(setup,/wizardSteps: \[0,1,2,3,4,5,6,7\]/);
    assert.match(picker,/id: picker/);
    assert.match(picker,/snapMode: ListView.SnapToItem/);
    assert.match(setup,/id: termsStage/);
    assert.match(setup,/qsTrId\("genycaster.setup.single_legal_consent"\)/);
    assert.match(setup,/onClicked: root.consentChecked = !root.consentChecked/);
    assert.match(setup,/id: legalReader/);
    assert.match(setup,/root.openLegalDocument\("terms"\)/);
    assert.match(setup,/root.openLegalDocument\("privacy"\)/);
    assert.match(setup,/acceptLegalConsent\(true, true, true\)/);
    assert.doesNotMatch(setup,/Qt.openUrlExternally/);
    assert.match(setup,/function previousStep\(\)/);
    assert.match(app,/visible: presentationBridge.initialSetupRequired \|\| root.setupReviewOpen/);
});

test('recovery phrase uses centered desktop modal and matching mobile full-screen stage',()=>{
    const dialog=read('ui/wasm/recovery-phrase-dialog.mjs');
    assert.match(dialog,/justify-content:center/);
    assert.match(dialog,/width:min\(620px,calc\(100% - 48px\)\)/);
    assert.match(dialog,/@media\(max-width:680px\)/);
    assert.match(dialog,/genycaster-sheet-in/);
    assert.match(dialog,/flex:1 1 auto;min-height:0;overflow-y:auto/);
    assert.match(dialog,/flex:0 0 auto;gap:10px/);
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
    assert.match(app.slice(dock,handle),/blurSourceItem: readingBackdrop/);
    assert.match(app,/anchors\.bottomMargin: 0/);
    assert.match(read('ui/components/FeedSurface.qml'),/bottomMargin: root\.compact \? 96 : 0/);
});

test('mobile Sign Out is reachable through Settings and requires confirmation',()=>{
    const app=read('ui/components/AppShell.qml');
    const settings=read('ui/components/LocaleSettingsPopup.qml');
    assert.match(app,/function requestSignOut\(\)/);
    assert.match(app,/onSignOutConfirmationRequested: root\.requestSignOut\(\)/);
    assert.match(app,/objectName: "signOutConfirmation"/);
    assert.match(settings,/objectName: "settingsRecoverySignOut"/);
    assert.match(settings,/visible: presentationBridge\.isAuthenticated[\s\S]*?root\.signOutConfirmationRequested\(\)/);
    const bridge=read('src/presentation/qt/presentation_bridge.cpp');
    assert.match(bridge,/m_showSetupAfterSignOut = true/);
});

test('mobile Home editing collapses dashboard cards and removes floating navigation',()=>{
    const home=read('ui/components/HomeDashboard.qml');
    const app=read('ui/components/AppShell.qml');
    assert.match(home,/readonly property bool composerFocused: mobileDraftEditor\.activeFocus/);
    assert.match(home,/id: mobileFlick/);
    assert.match(home,/onComposerFocusedChanged\(\)/);
    assert.match(home,/visible: !root\.composerFocused/);
    assert.match(home,/id: mobileDraftEditor/);
    assert.match(app,/id: homeDashboard/);
    assert.match(app,/readonly property bool homeComposerFocused:/);
    assert.match(app,/visible: root\.mobile && !root\.initialSetupOpen\s*&& !root\.homeComposerFocused/);
    assert.match(app,/anchors\.bottomMargin: 0/);
    assert.match(read('ui/components/FeedSurface.qml'),/bottomMargin: root\.compact \? 96 : 0/);
});

test('account header navigates to profile when signed in and opens sign-in otherwise',()=>{
    const header=read('ui/components/AppHeader.qml');
    assert.match(header,/onClicked: root\.isAuthenticated \? root\.ownProfileRequested\(\) : root\.accountRequested\(\)/);
    const app=read('ui/components/AppShell.qml');
    assert.match(app,/onAccountRequested: root\.openAccountPopup\(\)/);
    assert.match(app,/onOwnProfileRequested: root\.openMyProfileFromNavigation\(\)/);
    assert.match(app,/objectName: "mobileAccountProfileButton"/);
});

test('mobile compose launcher avoids inline keyboard and desktop hover hints',()=>{
    const home=read('ui/components/HomeDashboard.qml');
    const app=read('ui/components/AppShell.qml');
    const button=read('ui/components/ActionButton.qml');
    assert.match(home,/objectName: "homeComposerLauncher"/);
    assert.match(home,/onClicked: root\.composeRequested\(root\.draftText\)/);
    assert.match(home,/id: mobileDraftEditor[\s\S]*?visible: false[\s\S]*?readOnly: true/);
    assert.match(app,/onComposeRequested: text => root\.openComposer\(null, "", text\)/);
    assert.match(button,/readonly property bool mobileViewport:/);
    assert.match(button,/hoverEnabled: !mobileViewport/);
    assert.match(button,/if \(control\.mobileViewport \|\| !control\.hovered/);
});

test('verification provides an adaptive, accessible approval surface',()=>{
    const setup=read('ui/components/InitialSetup.qml');
    const approval=setup.split('id: verifyingStage')[1].split('id: appearanceStage')[0];
    assert.match(approval,/ColumnLayout \{/);
    assert.match(approval,/Layout.fillWidth: true/);
    assert.match(approval,/qsTrId\("genycaster.shell.open_farcaster"\)/);
    assert.match(approval,/root.openApproval\(\)/);
    assert.match(approval,/root.cancelAuthentication\(\)/);
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

test('Splash logo, version, loading copy and progress share a centered layout axis',()=>{
    // A previous <span class="boot-glyph" src="..."> was not a void image.
    // It nested the wordmark and following block content in an inline span,
    // shifting the visible logo to the right of the progress bar.
    const branding=html.match(/<div class="boot-mark">([\s\S]*?)<\/div>\s*<h1 id="boot-heading">/);
    assert.ok(branding,'brand group must close before the loading heading');
    assert.match(branding[1],/<img class="boot-glyph" src="\.\/genycaster-boot-icon\.svg" alt="" decoding="async">/);
    assert.match(branding[1],/<img class="boot-wordmark" src="\.\/genycaster-wordmark-dark\.svg" alt="GenyCaster"/);
    assert.match(branding[1],/<span class="boot-beta">Beta 1\.0<\/span>/);
    assert.doesNotMatch(branding[1],/<span class="boot-glyph"/);
    assert.match(html,/\.boot-content\{[^}]*align-items:center[^}]*text-align:center/);
    assert.match(html,/\.boot-mark\{[^}]*align-items:center[^}]*width:100%/);
    assert.match(html,/\.boot-wordmark\{[^}]*object-position:center/);
    assert.match(html,/\.boot-beta\{[^}]*width:100%;text-align:center/);
    assert.match(html,/\.boot-track\{width:100%/);
    assert.match(html,/\.boot-foot\{display:flex;width:100%/);
});

test('WASM splash is single-stage, retains original mark and build footer',()=>{
    const cmake=read('ui/CMakeLists.txt');
    const pack=read('ci/package_wasm_assets.py');
    assert.doesNotMatch(html,/class="boot-journey/);
    assert.match(html,/genycaster-wordmark-dark\.svg/);
    assert.match(html,/Powered by <strong>Genyleap<\/strong> · WASM Engine/);
    assert.doesNotMatch(html,/box-shadow:0 0 0 4px/);
    assert.doesNotMatch(html,/id="boot-version"/);
    assert.match(html,/<span class="boot-beta">Beta 1\.0<\/span>/);
    assert.doesNotMatch(html,/id="boot-note"/);
    assert.match(html,/id="boot-build-credit"/);
    assert.match(cmake,/--app-version "\$\{PROJECT_VERSION\}"/);
    assert.match(pack,/"buildId": fingerprint\[:8\]/);
    assert.doesNotMatch(loader,/versionLabel/);
    assert.doesNotMatch(loader,/bootNote/);
    assert.match(loader,/if\(creditBuild\)creditBuild\.textContent/);
});

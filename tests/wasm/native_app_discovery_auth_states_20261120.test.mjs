import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url),'utf8');
const shell = read('ui/components/shell/AppShell.qml');
const setup = read('ui/components/setup/InitialSetup.qml');
const settings = read('ui/components/settings/SettingsPopup.qml');
const cta = read('ui/components/controls/NativeAppDiscoveryCTA.qml');
const cmake = read('ui/CMakeLists.txt');
const html = read('ui/wasm/genycaster-shell.html');
const css = read('ui/wasm/native-downloads.css');
const js = read('ui/wasm/native-downloads.js');
const bridgeCpp = read('src/presentation/qt/presentation_bridge.cpp');
const bridgeHeader = read('src/presentation/qt/presentation_bridge.hpp');

test('native app selector is invoked by a first-party, no-credential WASM bridge', () => {
    assert.match(bridgeHeader,/Q_INVOKABLE void openBrowserNativeDownloads\(\)/);
    const method=bridgeCpp.slice(bridgeCpp.indexOf('void PresentationBridge::openBrowserNativeDownloads()'),
                                bridgeCpp.indexOf('void PresentationBridge::openEmbeddedMedia('));
    assert.match(method,/#if defined\(Q_OS_WASM\)/);
    assert.match(method,/window\.__genycasterOpenNativeDownloads\?\.\(\)/);
    assert.doesNotMatch(method,/accountFid|recoveryWords|privateKey|m_isAuthenticated/);
    assert.match(js,/window\.__genycasterOpenNativeDownloads = \(\) => setOpen\(true\)/);
});
test('guest Explore shows sign-in primary and secondary download, both tied to authentication not write readiness', () => {
    const block=shell.slice(shell.indexOf('objectName: "webGuestDiscoveryActions"'),
                            shell.indexOf('HomeFeedHeader {',shell.indexOf('objectName: "webGuestDiscoveryActions"')));
    assert.match(block,/Qt\.platform\.os === "wasm"/);
    assert.match(block,/!presentationBridge\.isAuthenticated/);
    assert.doesNotMatch(block,/writeReady|signerReady/);
    assert.match(block,/showSignIn: true/);
    assert.match(block,/onSignInRequested: root\.openAccountPopup\(\)/);
    assert.match(block,/onDownloadsRequested: presentationBridge\.openBrowserNativeDownloads\(\)/);
    assert.match(cta,/objectName: "nativeAppDiscoveryExplore"/);
    assert.match(cta,/prominent: true/);
    assert.match(cta,/text: qsTrId\("genycaster\.header\.sign_in"\)/);
    assert.match(cta,/text: qsTrId\("genycaster\.apps\.download_cta_title"\)/);
});
test('setup entry is embedded in the scroll layout, never on top of 24-word input or the primary action', () => {
    assert.match(setup,/objectName: "setupNativeAppEntry"/);
    assert.match(setup,/objectName: "setupWelcomeNativeApps"/);
    assert.match(setup,/root\.currentStep === 3 \|\| root\.currentStep === 4/);
    assert.match(setup,/setupNativeAppEntry\.y \+ setupNativeAppEntry\.height/);
    assert.match(setup,/y: stagePanel\.y \+ stagePanel\.implicitHeight \+ Theme\.space4/);
    assert.match(setup,/onDownloadsRequested: root\.nativeAppDownloadsRequested\(\)/);
    assert.match(setup,/NativeAppDiscoveryCTA \{/);
    assert.match(shell,/onNativeAppDownloadsRequested: presentationBridge\.openBrowserNativeDownloads\(\)/);
});
test('authenticated users find desktop/mobile apps in settings, not permanent overlays', () => {
    assert.match(settings,/objectName: "mobileNativeAppsSettingsEntry"[\s\S]*?visible: root\.webAssembly && presentationBridge\.isAuthenticated/);
    assert.match(settings,/objectName: "desktopNativeAppsSettingsEntry"[\s\S]*?visible: root\.webAssembly && presentationBridge\.isAuthenticated/);
    assert.match(settings,/section === -3 && root\.webAssembly/);
    assert.match(settings,/presentationBridge\.openBrowserNativeDownloads\(\)/);
    assert.match(html,/id="native-downloads-dock"[^>]*hidden/);
    assert.match(css,/#native-downloads-dock,\s*#native-downloads-nudge\s*\{\s*display:none!important/);
});
test('the install invitation is visibly described, localized and registered once for every setup and guest host', () => {
    assert.match(cmake,/components\/controls\/NativeAppDiscoveryCTA\.qml/);
    assert.match(cta,/text: qsTrId\("genycaster\.apps\.download_cta_description"\)/);
    assert.match(cta,/text: qsTrId\("genycaster\.apps\.download_cta_button"\)/);
    assert.match(cta,/text: qsTrId\("genycaster\.apps\.download_cta_eyebrow"\)/);
    assert.match(cta,/columns: root\.showSignIn && !root\.narrow \? 2 : 1/);
    assert.match(cta,/implicitHeight: content\.implicitHeight/);
    assert.match(cta,/onClicked: root\.downloadsRequested\(\)/);
    assert.match(setup,/objectName: "setupWelcomeNativeApps"/);
    assert.match(setup,/height: visible \? implicitHeight : 0/);
    assert.match(shell,/height: visible \? implicitHeight : 0/);
    for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const content = read('ui/i18n/qml_' + locale + '.ts');
        for (const id of ['eyebrow','title','description','button'])
            assert.ok(content.includes('id="genycaster.apps.download_cta_' + id + '"'), locale + ' missing ' + id);
    }
});
test('release catalog refuses unverified downloads while dialog stays accessible', () => {
    assert.match(js,/function verifiedRelease\(record\)/);
    assert.match(js,/entry = verifiedRelease\(data\.platforms\[os\]\?\.\[arch\]\)/);
    assert.match(js,/download\.hidden = !entry/);
    assert.match(html,/id="native-downloads-dialog" role="dialog"/);
});

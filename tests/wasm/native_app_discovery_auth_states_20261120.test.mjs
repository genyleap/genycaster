import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url),'utf8');
const shell = read('ui/components/shell/AppShell.qml');
const setup = read('ui/components/setup/InitialSetup.qml');
const settings = read('ui/components/settings/SettingsPopup.qml');
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
test('guest Explore retains its account header without recurring native app promo cards', () => {
    assert.doesNotMatch(shell,/NativeAppDiscoveryCTA|webGuestDiscoveryActions/);
    assert.match(shell,/onAuthenticationRequested: root\.openAccountPopup\(\)/);
});

test('welcome stacks native downloads below Get Started on compact screens and preserves desktop alignment', () => {
    assert.match(setup,/readonly property bool welcomeNativeAppsVisible:/);
    assert.match(setup,/Qt\.platform\.os === "wasm"/);
    assert.match(setup,/currentStep === 0 && !reviewMode/);
    const welcomeGuard = setup.slice(
        setup.indexOf('readonly property bool welcomeNativeAppsVisible:'),
        setup.indexOf('readonly property real welcomeActionsWidth:'));
    assert.doesNotMatch(welcomeGuard,/!authenticated/,
        'a restored but unfinished browser setup must still show native downloads');
    assert.match(setup,/objectName: "setupStageAction"/);
    assert.match(setup,/objectName: "setupWelcomeNativeApps"/);
    assert.match(setup,/readonly property real welcomeRowWidth: root\.phone \? width/);
    assert.match(setup,/anchors\.leftMargin: root\.phone \? Theme\.space4 : root\.setupGutter/);
    assert.match(setup,/anchors\.rightMargin: root\.phone \? Theme\.space4 : root\.setupGutter/);
    assert.match(setup,/stageAction\.x - Theme\.space2 - width/);
    assert.match(setup,/height: stageAction\.height/);
    assert.match(setup,/readonly property bool stackWelcomeActions: phone && welcomeNativeAppsVisible/);
    assert.match(setup,/width: root\.stackWelcomeActions\s*\? setupFooterContent\.width/);
    assert.match(setup,/wrapText: true/);
    assert.match(setup,/iconGlyph: Theme\.iconDownload/);
    assert.match(setup,/onClicked: root\.nativeAppDownloadsRequested\(\)/);
    assert.match(shell,/onNativeAppDownloadsRequested: presentationBridge\.openBrowserNativeDownloads\(\)/);
    assert.doesNotMatch(setup,/NativeAppDiscoveryCTA|setupNativeAppEntry/);
});

test('native release install CTA retains the Superellipse while changing releases', () => {
    assert.match(html,/id="native-downloads-action-label"/);
    assert.match(html,/class="native-download-glyph"/);
    assert.match(js,/const downloadLabel = byId\("native-downloads-action-label"\)/);
    assert.match(js,/downloadLabel\.textContent = "Download "/);
    assert.doesNotMatch(js,/download\.textContent\s*=/,
        'textContent on the anchor destroys its mounted SVG and leaves plain text');
    assert.match(css,/#native-downloads-action\.nd-squircle\s*\{[^}]*--nd-shape-fill:var\(--nd-download\)/);
    assert.match(css,/#native-downloads-action\[hidden\]\s*\{display:none!important\}/);
    assert.match(css,/#native-downloads-action svg\.native-download-glyph/);
    assert.match(css,/#native-downloads-action\.nd-squircle:hover/);
    assert.match(css,/#native-downloads-action\.nd-squircle:active/);
    assert.match(html,/id="native-downloads-action-architecture"/);
    assert.match(js,/downloadArchitecture\.textContent = info\.arches\[arch\]/);
    assert.match(js,/Apple Silicon \(ARM64\)/);
    assert.match(js,/Intel \(x86_64\)/);
    assert.match(css,/--nd-tab-selected:#315CF5/);
    assert.match(css,/--nd-tab-selected-ink:#FFFFFF/);
    assert.match(css,/\.native-downloads-tabs button\.selected \{\s+color:var\(--nd-tab-selected-ink\)/);
    assert.match(css,/\.native-downloads-tabs button\.selected\.nd-squircle \{\s+--nd-shape-fill:var\(--nd-tab-selected\)/);
    assert.match(css,/\.native-downloads-secondary \{justify-self:stretch;width:100%\}/);
    assert.match(css,/--nd-download:#177A50/);
    assert.match(css,/--nd-download:#36CA91/);
});

test('authenticated users find desktop/mobile apps in settings, not permanent overlays', () => {
    assert.match(settings,/objectName: "mobileNativeAppsSettingsEntry"[\s\S]*?visible: root\.webAssembly && presentationBridge\.isAuthenticated/);
    assert.match(settings,/objectName: "desktopNativeAppsSettingsEntry"[\s\S]*?visible: root\.webAssembly && presentationBridge\.isAuthenticated/);
    assert.match(settings,/section === -3 && root\.webAssembly/);
    assert.match(settings,/presentationBridge\.openBrowserNativeDownloads\(\)/);
    assert.match(html,/id="native-downloads-dock"[^>]*hidden/);
    assert.match(css,/#native-downloads-dock,\s*#native-downloads-nudge\s*\{\s*display:none!important/);
});
test('the single localized welcome download action replaces every large card', () => {
    assert.doesNotMatch(cmake,/components\/controls\/NativeAppDiscoveryCTA\.qml/);
    assert.doesNotMatch(setup,/NativeAppDiscoveryCTA/);
    assert.doesNotMatch(shell,/NativeAppDiscoveryCTA/);
    assert.equal((setup.match(/objectName: "setupWelcomeNativeApps"/g)||[]).length,1);
    assert.match(setup,/text: qsTrId\("genycaster\.apps\.download_cta_button"\)/);
    for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const content = read('ui/i18n/qml_' + locale + '.ts');
        assert.ok(content.includes('<source>Download Native Apps</source>'),locale + ' missing button label');
        assert.ok(content.includes('id="genycaster.apps.download_cta_button"'),locale + ' missing native button');
    }
});

test('release catalog refuses unverified downloads while dialog stays accessible', () => {
    assert.match(js,/function verifiedRelease\(record\)/);
    assert.match(js,/entry = verifiedRelease\(data\.platforms\[os\]\?\.\[arch\]\)/);
    assert.match(js,/download\.hidden = !entry/);
    assert.match(html,/id="native-downloads-dialog" role="dialog"/);
});

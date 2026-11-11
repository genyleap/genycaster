import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const file = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');

test('Recovery in Settings is exclusively visible to authenticated accounts', () => {
  const settings = file('ui/components/settings/SettingsPopup.qml');
  const mobile = settings.slice(settings.indexOf('id: mobileSettingsIndex'));
  assert.match(mobile, /MobileSettingsGroup \{\s+visible: presentationBridge\.isAuthenticated\s+heading: ""/);
  assert.match(settings, /objectName: Number\(modelData\.section\) === 5 \? "settingsRecoveryTab"/);
  assert.match(settings, /visible: Number\(modelData\.section\) !== 5 \|\| presentationBridge\.isAuthenticated/);
  assert.match(settings, /section === 5 && !presentationBridge\.isAuthenticated/);
  assert.match(settings, /onAuthenticationStateChanged\(\) \{\s+if \(!presentationBridge\.isAuthenticated && root\.currentSection === 5\)/);
});

test('WASM Updates refreshes current deployed application instead of a desktop installer', () => {
  const qml = file('ui/components/settings/SettingsPopup.qml');
  const header = file('ui/app/update/controller.hpp');
  const source = file('ui/app/update/controller.cpp');
  assert.match(qml, /readonly property bool webAssembly: Qt\.platform\.os === "wasm"/);
  assert.match(qml, /objectName: "wasmRefreshUpdateButton"/);
  assert.match(qml, /onClicked: updateController\.refreshWebApp\(\)/);
  assert.match(qml, /text: updateController\.webRefreshing\s+\? qsTrId\("genycaster\.settings\.wasm_updating"\)/);
  assert.match(qml, /visible: !root\.webAssembly\s+titleText: qsTrId\("genycaster\.settings\.check_for_updates_automatically"\)/);
  assert.match(qml, /visible: !root\.webAssembly\s+Layout\.fillWidth: true\s+implicitHeight: updaterColumn/);
  assert.match(header, /Q_INVOKABLE void refreshWebApp\(\)/);
  assert.match(header, /Q_PROPERTY\(bool webRefreshing READ webRefreshing NOTIFY changed\)/);
  assert.match(source, /void UpdateController::refreshWebApp\(\)/);
  assert.match(source, /genycaster-assets\.json/);
  assert.match(source, /metadata\.value\(QStringLiteral\("files"\)\)\.isArray\(\)/);
  assert.match(source, /registration\.update\(\)/);
  assert.match(source, /window\.location\.replace\(next\.href\)/);
  assert.match(source, /new URL\(window\.location\.href\)/);
});

test('setup wheel preserves native script direction, accessible selection and keyboard navigation', () => {
  const selector = file('ui/components/setup/SetupLanguageSelector.qml');
  const setup = file('ui/components/setup/InitialSetup.qml');
  assert.match(selector, /objectName: "setupLanguageWheel"/);
  assert.match(selector, /snapMode: ListView.SnapToItem/);
  assert.match(selector, /onMovementEnded: root.choose\(currentIndex\)/);
  assert.match(selector, /readonly property bool nativeRtl: \["fa", "ar", "he", "ur", "ps"\]/);
  assert.match(selector, /layoutDirection: languageRow.nativeRtl \? Qt.RightToLeft : Qt.LeftToRight/);
  assert.match(selector, /Accessible.role: Accessible.RadioButton/);
  assert.match(selector, /Keys.onUpPressed/);
  assert.match(selector, /Keys.onDownPressed/);
  assert.match(selector, /Keys.onPressed: event =>/);
  assert.match(selector, /Qt.Key_Home/);
  assert.match(selector, /Qt.Key_End/);
  assert.doesNotMatch(selector, /Keys.onHomePressed|Keys.onEndPressed/);
  assert.doesNotMatch(selector, /TextField|languageGrid/);
  for (const t of ['fa', 'ar', 'en', 'fr']) assert.ok(setup.includes('tag:"'+t+'"'));
});

test('account choices follow storyboard and pending registration cannot launch Farcaster', () => {
    const setup=file('ui/components/setup/InitialSetup.qml');
    const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
    assert.match(account,/genycaster.setup.design_create_account/);
    assert.match(account,/genycaster.setup.design_existing_account/);
    assert.match(account,/objectName: "setupCreateAccountUnavailable"/);
    assert.match(account,/unavailable: true/);
    assert.match(account,/enabled: false/);
    assert.match(account,/root.accountIntent = "existing"/);
    assert.doesNotMatch(account,/root.openRegistration\(\)/);
});

test('web update copy is present in all supported locales', () => {
  const locales = ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  const keys = ['wasm_update_description','wasm_updating','wasm_refresh_hint','wasm_refresh_now'];
  for (const locale of locales) {
    const ts = file('ui/i18n/qml_'+locale+'.ts');
    for (const key of keys)
      assert.ok(ts.includes('id="genycaster.settings.'+key+'"'), locale + ': '+ key);
  }
});

test('Setup surfaces use the active accent and actual theme controller',()=>{
    const button=file('ui/components/setup/SetupPrimaryButton.qml');
    const card=file('ui/components/setup/SetupActionCard.qml');
    const setup=file('ui/components/setup/InitialSetup.qml');
    for(const s of [button,card]){
        assert.match(s,/Theme.primaryFill/);
        assert.match(s,/Theme.accentInk/);
        assert.doesNotMatch(s,/#111315|#FFFFFF|#F6F7F8/);
    }
    assert.match(setup,/appearanceController.setThemeMode/);
    assert.match(setup,/appearanceController.setColorThemeId/);
    assert.match(setup,/id: appearanceStage/);
});

test('Setup re-entry and authentication routes stay connected',()=>{
    const setup=file('ui/components/setup/InitialSetup.qml');
    const shell=file('ui/components/shell/AppShell.qml');
    assert.match(setup,/function switchStep\(step\)/);
    assert.match(setup,/root.continueWithGenyleap\(\)/);
    assert.match(setup,/root.continueWithFarcaster\(\)/);
    assert.match(setup,/root.nativeRecoverAccount\(words\)/);
    assert.match(shell,/function openSetupReview\(step\)/);
    assert.match(shell,/onCloseReviewRequested: root.setupReviewOpen = false/);
});

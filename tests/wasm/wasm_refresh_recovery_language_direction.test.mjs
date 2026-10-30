import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const file = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');

test('Recovery in Settings is exclusively visible to authenticated accounts', () => {
  const settings = file('ui/components/LocaleSettingsPopup.qml');
  const mobile = settings.slice(settings.indexOf('id: mobileSettingsIndex'));
  assert.match(mobile, /MobileSettingsGroup \{\s+visible: presentationBridge\.isAuthenticated\s+heading: ""/);
  assert.match(settings, /objectName: Number\(modelData\.section\) === 5 \? "settingsRecoveryTab"/);
  assert.match(settings, /visible: Number\(modelData\.section\) !== 5 \|\| presentationBridge\.isAuthenticated/);
  assert.match(settings, /section === 5 && !presentationBridge\.isAuthenticated/);
  assert.match(settings, /onAuthenticationStateChanged\(\) \{\s+if \(!presentationBridge\.isAuthenticated && root\.currentSection === 5\)/);
});

test('WASM Updates refreshes current deployed application instead of a desktop installer', () => {
  const qml = file('ui/components/LocaleSettingsPopup.qml');
  const header = file('ui/mobile/update_controller.hpp');
  const source = file('ui/mobile/update_controller.cpp');
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
  const selector = file('ui/components/SetupLanguageSelector.qml');
  const setup = file('ui/components/InitialSetup.qml');
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
  for (const t of ['fa', 'ar', 'en', 'fr']) assert.ok(setup.includes('tag: "'+t+'"'));
});

test('setup account choices are stacked and do not advertise unimplemented registration', () => {
  const setup = file('ui/components/InitialSetup.qml');
  const account = setup.slice(setup.indexOf('id: accountStage'));
  assert.match(account, /ColumnLayout \{\s+visible: !root.browserReadOnly/);
  assert.match(account, /title: qsTrId\("genycaster.setup.new_profile"\)/);
  assert.match(account, /trailingText: qsTrId\("genycaster.shell.coming_soon"\)\s+enabled: false/);
  assert.match(account, /title: qsTrId\("genycaster.setup.existing_profile"\)/);
  assert.match(setup, /visible: false \/\/ Minimal, single-task canvas on desktop/);
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

test('Setup CTAs and action cards follow the active accent palette', () => {
  const button = file('ui/components/SetupPrimaryButton.qml');
  const card = file('ui/components/SetupActionCard.qml');
  const setup = file('ui/components/InitialSetup.qml');
  for (const code of [button, card]) {
    assert.match(code, /Theme.primaryFill/);
    assert.match(code, /Theme.primaryFillHover/);
    assert.match(code, /Theme.primaryPressed/);
    assert.match(code, /Theme.accentInk/);
    assert.doesNotMatch(code, /#111315|#FFFFFF|#F6F7F8|#16191D/);
  }
  assert.match(setup, /modeButton.selected\s+\? Theme.surfaceSelected/);
  assert.match(setup, /color: modeButton.selected \? Theme.accent : Theme.inkMuted/);
  assert.match(setup, /id: termsSection/);
  assert.match(setup, /id: privacySection/);
  assert.match(setup, /columns: 1[\s\S]*?genycaster.setup.network_hypersnap_category/);
});

test('account Setup and drawer share a staged, accessible existing-account flow', () => {
  const setup = file('ui/components/InitialSetup.qml');
  const shell = file('ui/components/AppShell.qml');
  assert.match(setup, /property bool existingAccountExpanded: false/);
  assert.match(setup, /root.existingAccountExpanded = !root.existingAccountExpanded/);
  assert.match(setup, /root.browserReadOnly \? root.alternateMethodsExpanded\s+: root.existingAccountExpanded/);
  assert.match(shell, /id: accountPopup[\s\S]*?property bool existingMethodsExpanded: false/);
  assert.match(shell, /id: accountScroll[\s\S]*?id: accountActions/);
  assert.match(shell, /SetupPageHeader \{[\s\S]*?genycaster.shell.sign_in_to_interact/);
  assert.match(shell, /trailingText: qsTrId\("genycaster.shell.coming_soon"\)\s+enabled: false/);
  assert.match(shell, /visible: accountPopup.existingMethodsExpanded && !presentationBridge.isAuthenticated/);
  assert.match(shell, /onClicked: presentationBridge.beginSignIn\(\)/);
  assert.match(shell, /onClicked: presentationBridge.beginFarcasterSignIn\(\)/);
  assert.match(shell, /onClicked: presentationBridge.openAuthenticationApproval\(\)/);
});

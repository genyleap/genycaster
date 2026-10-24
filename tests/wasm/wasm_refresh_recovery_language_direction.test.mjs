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

test('each setup language card aligns by its own script, independent of interface language', () => {
  const selector = file('ui/components/SetupLanguageSelector.qml');
  const setup = file('ui/components/InitialSetup.qml');
  assert.match(selector, /readonly property bool nativeRtl: \["fa", "ar", "he", "ur", "ps"\]/);
  assert.match(selector, /layoutDirection: languageCard\.nativeRtl \? Qt\.RightToLeft : Qt\.LeftToRight/);
  assert.equal((selector.match(/horizontalAlignment: languageCard\.nativeRtl \? Text\.AlignRight : Text\.AlignLeft/g) || []).length, 2);
  for (const t of ['fa', 'ar', 'en', 'fr']) assert.ok(setup.includes('tag: "'+t+'"'));
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

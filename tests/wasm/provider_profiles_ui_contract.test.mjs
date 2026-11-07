import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const root = new URL('../../', import.meta.url);
const read = file => readFileSync(new URL(file, root), 'utf8');
const languages = ['en_US', 'fa_IR', 'ar_SA', 'de_DE', 'es_ES', 'fr_FR',
                   'hi_IN', 'ja_JP', 'ko_KR', 'pt_BR', 'ru_RU', 'zh_CN'];

test('provider profiles UI is wired to persisted composition settings', () => {
  const controller = read('ui/app/provider_profiles_controller.cpp');
  const bootstrap = read('ui/app/main.cpp');
  const layout = read('ui/components/ProviderProfilesPanel.qml');
  assert.match(controller, /network\/readProviderProfilesJson/);
  assert.match(controller, /network\/writeProviderProfilesJson/);
  assert.match(controller, /network\/preferredReadProviderId/);
  assert.match(controller, /network\/preferredWriteProviderId/);
  assert.match(bootstrap, /make_unique<ProviderProfilesController>/);
  assert.match(bootstrap, /\{"providerProfiles", &providerProfilesController\}/);
  assert.match(layout, /providerProfiles\.addReadProfile/);
  assert.match(layout, /providerProfiles\.addWriteProfile/);
  assert.match(layout, /providerProfiles\.selectReadProvider/);
  assert.match(layout, /providerProfiles\.selectWriteProvider/);
  assert.match(layout, /providerProfiles\.removeReadProfile/);
  assert.match(layout, /providerProfiles\.removeWriteProfile/);
});

test('new provider editor is in network/content settings, not language settings', () => {
  const popup = read('ui/components/LocaleSettingsPopup.qml');
  const section = popup.indexOf('id: readSettingsScroll');
  const panel = read('ui/components/NodeManagementPanel.qml');
  const locale = popup.indexOf('id: localeScroll');
  const nodeSettings = popup.indexOf('id: nodeSettingsScroll');
  assert(section > 0 && nodeSettings > section);
  assert(locale > 0 && locale < section);
  assert.match(popup.slice(nodeSettings), /NodeManagementPanel \{/);
  assert.match(panel, /providerProfiles\.addReadProfile/);
  assert.match(panel, /providerProfiles\.addWriteProfile/);
  assert.match(panel, /providerProfiles\.selectReadProvider/);
  assert.match(panel, /providerProfiles\.selectWriteProvider/);
  assert(!popup.slice(locale, section).includes('id: validatorEndpointField'));
  assert.match(read('ui/CMakeLists.txt'), /components\/ProviderProfilesPanel\.qml/);
});

test('provider operations have complete native localization in all installed languages', () => {
  const keys = new Set([...read('ui/components/ProviderProfilesPanel.qml')
    .matchAll(/qsTrId\("(genycaster\.providers\.[^"]+)"\)/g)].map(match => match[1]));
  assert(keys.size >= 20);
  for (const lang of languages) {
    const catalog = read(`ui/i18n/qml_${lang}.ts`);
    for (const key of keys) {
      const escaped = key.replace(/[.*+?^$()|[\]{}\\]/g, '\\$&');
      const message = catalog.match(new RegExp(`<message id="${escaped}">([\\s\\S]*?)<\\/message>`));
      assert(message, `missing ${key} in ${lang}`);
      assert.match(message[1], /<translation>\S[\s\S]*?<\/translation>/,
        `missing translated text ${key} in ${lang}`);
    }
  }
});

test('protocol choice is explicit: HyperSnap default, selectable hosted Farcaster, and separately pinned signer writer', () => {
  const bootstrap=read('ui/app/bootstrap/profiles.cpp');
  const appEntry=read('ui/app/main.cpp');
  const cpp=read('ui/app/provider_profiles_controller.cpp');
  const h=read('ui/app/provider_profiles_controller.hpp');
  const qml=read('ui/components/ProviderProfilesPanel.qml');
  const composition=read('ui/app/runtime_composition.cpp');
  assert.match(bootstrap,/preferredReadProviderText = QStringLiteral\("hypersnap-v2"\)/);
  assert.match(bootstrap,/preferredWriteProviderId/);
  assert.match(appEntry,/farcasterReadBaseUrlText\);/);
  assert.match(h,/Q_PROPERTY\(bool hostedReadSupported READ hostedReadSupported CONSTANT\)/);
  assert.match(cpp,/return !m_hostedReadEndpoint.isEmpty\(\)/);
  assert.match(cpp,/selected == QStringLiteral\("farcaster-snapchain"\) && !hostedReadSupported\(\)/);
  assert.match(qml,/providerProfiles\.selectReadProvider\("farcaster-snapchain"\)/);
  assert.match(qml,/providerProfiles\.selectReadProvider\("hypersnap-v2"\)/);
  assert.match(composition,/\.provider = selected, \.require = true/);
  assert.match(composition,/runtime->signerRegistry->bindRouter\(\*runtime->router\)/);
  // HyperSnap is an indexed read adapter until a separate compatible writer
  // contract exists: never mislabel it as a write signer provider.
  const indexed=composition.slice(composition.indexOf('ProviderId{"hypersnap-v2"}'),
      composition.indexOf('runtime->independentReadProviderId'));
  assert.doesNotMatch(indexed,/protocolSignerManage\(\)/);
});

test('profile controller accepts known adapters only and refuses credential-bearing URLs', () => {
  const cpp = read('ui/app/provider_profiles_controller.cpp');
  assert.match(cpp, /knownContract\(contractId, writer\)/);
  assert.match(cpp, /QUrl::StrictMode/);
  assert.match(cpp, /!url\.userInfo\(\)\.isEmpty\(\)/);
  assert.match(cpp, /url\.hasQuery\(\) \|\| url\.hasFragment\(\)/);
  assert.match(cpp, /m_settings\.sync\(\)/);
  assert.match(cpp, /kMaximumProfiles = 16/);
  const writer = read('src/routing/write_gateway.cpp');
  assert.match(writer, /source\.endpointIdentity == binding\.endpointIdentity/);
});

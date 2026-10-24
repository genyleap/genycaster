import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');
const settings = read('ui/components/LocaleSettingsPopup.qml');
const manager = read('ui/components/NodeManagementPanel.qml');
const controller = read('ui/mobile/provider_profiles_controller.cpp');
const header = read('ui/mobile/provider_profiles_controller.hpp');
const bootstrap = read('ui/mobile/main.cpp');

test('Node Management is its own settings destination on desktop and mobile', () => {
  assert.match(settings, /currentSection === 6/);
  assert.match(settings, /section: 6/);
  assert.match(settings, /id: nodeSettingsScroll/);
  assert.match(settings, /NodeManagementPanel\s*\{/);
  assert.doesNotMatch(settings, /ProviderProfilesPanel\s*\{/);
  assert.match(settings, /readSettingsScroll/);
  assert.match(settings, /nodeSettingsScroll\]/);
});

test('the active read route and pending choice are distinguishable', () => {
  assert.match(header, /activeReadEndpoint READ activeReadEndpoint/);
  assert.match(header, /readRestartRequired READ readRestartRequired/);
  assert.match(manager, /providerProfiles\.activeReadEndpoint/);
  assert.match(manager, /providerProfiles\.readRestartRequired/);
  assert.match(manager, /providerProfiles\.selectedReadProviderId/);
  assert.match(manager, /providerProfiles\.selectReadProvider/);
});

test('read health checks validate real typed API results with timeout and no redirect', () => {
  assert.match(controller, /\/v1\/info/);
  assert.match(controller, /\/v2\/farcaster\/user\?fid=3/);
  assert.match(controller, /QNetworkRequest::ManualRedirectPolicy/);
  assert.match(controller, /setTransferTimeout\(8'000\)/);
  assert.match(controller, /json\.contains\(QStringLiteral\("dbStats"\)\)/);
  assert.match(controller, /"incompatible"/);
  assert.match(controller, /"auth_required"/);
  assert.match(manager, /probeReadProvider/);
  assert.match(manager, /probeDraftNode/);
  assert.match(read('ui/tests/provider_profiles_controller_tests.cpp'),
               /testLiveReadProbeDistinguishesValidProtocolFromAnyHttp200/);
});

test('Safari browser paste is available for address and API fields', () => {
  assert.match(controller, /navigator\.clipboard\.readText\(\)/);
  assert.match(controller, /genycasterNodeClipboardResult/);
  assert.match(manager, /requestBrowserPaste\(\)/);
  assert.match(manager, /readEndpoint\.text = copied\.trim\(\)/);
  assert.match(manager, /readKey\.text = copied\.trim\(\)/);
  assert.match(manager, /echoMode: TextInput\.Password/);
});

test('opt-in indexed API keys are session-scoped, never stored in QSettings', () => {
  assert.match(controller, /setSessionReadApiKey/);
  assert.match(controller, /sessionStorage\.setItem\(key, JSON\.stringify\(saved\)\)/);
  assert.match(bootstrap, /sessionStorage\.getItem\(/);
  assert.match(bootstrap, /EM_ASM_PTR/);
  assert.match(bootstrap, /GENYCASTER_READ_PROVIDER_API_KEYS_JSON/);
  assert.match(bootstrap, /Indexed read credentials must be a JSON object/);
  assert.doesNotMatch(controller, /m_settings\.setValue\([^\n]*[aA]pi[Kk]ey/);
  assert.match(manager, /genycaster\.nodes\.api_warning/);
});

test('localhost is a browser-side example, not a managed fallback', () => {
  assert.match(manager, /http:\/\/127\.0\.0\.1:3381/);
  assert.match(manager, /genycaster\.nodes\.local_warning/);
  assert.match(manager, /setCompatibleReadFallback/);
  assert.match(manager, /root\.showAdvanced/);
});

test('32 new UI labels are translated in every supported locale', () => {
  const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for (const locale of locales) {
    const content = read('ui/i18n/qml_'+locale+'.ts');
    for (const id of ['title','description','active','test','checking','connected',
                      'auth_required','incompatible','unreachable','not_tested',
                      'raw_description','indexed_description','paste','local_warning',
                      'api_warning','save_key','reload','test_note']) {
      assert.match(content,
        new RegExp('<message id="genycaster\\.nodes\\.' + id +
          '">[\\s\\S]*?<translation>[^<]+</translation>'));
    }
  }
});

test('a custom raw node can receive only its own explicit HTTPS x-api-key', () => {
  const composition = read('ui/mobile/runtime_composition.cpp');
  assert.match(bootstrap, /profile\.adapterContract == "snapchain-v1"/);
  assert.match(composition, /const auto rawCredential = readProviderApiKeys\.find\(profile\.id\)/);
  assert.match(composition, /\.authorizationHeaderName = rawCredential/);
  assert.match(composition, /std::string\{"x-api-key"\}/);
  assert.match(composition, /Authenticated raw read providers require HTTPS/);
  const controller = read('ui/mobile/provider_profiles_controller.cpp');
  assert.match(controller, /QStringLiteral\("snapchain-v1"\)\)/);
  assert.match(manager, /readKey\.text\)/);
});

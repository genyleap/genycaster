import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');
const settings = read('ui/components/settings/SettingsPopup.qml');
const manager = read('ui/components/settings/NodeManagementPanel.qml');
const controller = read('ui/app/settings/providers.cpp');
const header = read('ui/app/settings/providers.hpp');
const bootstrap = read('ui/app/bootstrap/profiles.cpp');

test('Node Management is its own settings destination on desktop and mobile', () => {
  assert.match(settings, /root\.currentSection = Number\(modelData\.section\)/);
  assert.match(settings, /id: settingsNavigation/);
  // Full-viewport Settings uses one responsive semantic sidebar width.
  assert.match(settings, /readonly property real navigationColumnWidth: Math\.min\(228, Math\.max\(186, width \* 0\.24\)\)/);
  assert.match(settings, /Layout\.preferredWidth: root\.navigationColumnWidth/);
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
  assert.match(controller, /\/v1\/castsByFid\?fid=3&pageSize=1&reverse=true/);
  assert.match(controller, /\/v1\/userDataByFid\?fid=3&pageSize=100/);
  assert.match(controller, /\/v2\/farcaster\/user\?fid=3/);
  assert.match(controller, /QNetworkRequest::ManualRedirectPolicy/);
  assert.match(controller, /setTransferTimeout\(8'000\)/);
  assert.match(controller, /runProbeStep\(id, contract, origin, draftApiKey, 1\)/);
  assert.match(controller, /QStringLiteral\("castAddBody"\)/);
  assert.match(controller, /QStringLiteral\("userDataBody"\)/);
  assert.match(controller, /"incompatible"/);
  assert.match(controller, /"auth_required"/);
  assert.match(controller, /"contract_mismatch"/);
  assert.match(controller, /checkingAlternative/);
  assert.match(controller, /m_probeSuggestedContract = contract/);
  assert.match(manager, /genycaster\.nodes\.contract_mismatch/);
  assert.match(manager, /genycaster\.nodes\.use_detected/);
  assert.match(manager, /root\.readContract = providerProfiles\.probeSuggestedContract/);
  assert.match(manager, /probeReadProvider/);
  assert.match(manager, /probeDraftNode/);
  assert.match(read('tests/ui/provider_profiles_controller_tests.cpp'),
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
  assert.match(manager, /genycaster\.nodes\.extra_data/);
  assert.match(manager, /checked: providerProfiles\.compatibleReadFallback/);
});

test('Node Management labels including contract correction are translated in every supported locale', () => {
  const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for (const locale of locales) {
    const content = read('ui/i18n/qml_'+locale+'.ts');
    for (const id of ['title','description','active','test','checking','connected',
                      'auth_required','incompatible','unreachable','not_tested',
                      'raw_description','raw_limitations','indexed_description','paste','local_warning',
                      'api_warning','save_key','reload','test_note','contract_mismatch','use_detected',
                      'pending_read','active_tag','pending_tag']) {
      assert.match(content,
        new RegExp('<message id="genycaster\\.nodes\\.' + id +
          '">[\\s\\S]*?<translation>[^<]+</translation>'));
    }
  }
});

test('a custom raw node can receive only its own explicit HTTPS x-api-key', () => {
  const composition = read('ui/app/runtime/composition.cpp');
  assert.match(bootstrap, /profile\.adapterContract == "snapchain-v1"/);
  assert.match(composition, /const auto rawCredential = readProviderApiKeys\.find\(profile\.id\)/);
  assert.match(composition, /\.authorizationHeaderName = rawCredential/);
  assert.match(composition, /std::string\{"x-api-key"\}/);
  assert.match(composition, /Authenticated raw read providers require HTTPS/);
  const controller = read('ui/app/settings/providers.cpp');
  assert.match(controller, /QStringLiteral\("snapchain-v1"\)\)/);
  assert.match(manager, /readKey\.text\)/);
});

test('all Node Management Flow positioners use writable Layout.preferredHeight', () => {
  // QQuickPositioner::implicitHeight is read-only: qmllint may pass, but
  // QQmlApplicationEngine aborts the entire Main interface at runtime.
  assert.doesNotMatch(manager, /implicitHeight:\s*childrenRect\.height/);
  assert.equal([...manager.matchAll(/Layout\.preferredHeight:\s*childrenRect\.height/g)].length, 4);
});

test('probing a real raw node requires both Cast and profile endpoints, not just info', () => {
  assert.match(controller, /runProbeStep\(id, contract, origin, draftApiKey, 0\)/);
  assert.match(controller, /runProbeStep\(id, contract, origin, draftApiKey, 1\)/);
  assert.match(controller, /m_probeTimer\.start\(\)/);
  assert.doesNotMatch(controller, /QStringLiteral\("\/v1\/info"\)/);
  assert.match(read('tests/ui/provider_profiles_controller_tests.cpp'), /assert\(castRequests == 2 && profileRequests == 2\)/);
  assert.match(read('tests/ui/provider_profiles_controller_tests.cpp'),
               /probeSuggestedContract\(\) == QStringLiteral\("snapchain-v1"\)/);
  assert.match(read('tests/ui/provider_profiles_controller_tests.cpp'), /probeHttpStatus\(\) == 402/);
});

test('raw node limitations are explicit, and saving does not automatically switch provider', () => {
  assert.match(manager, /genycaster\.nodes\.raw_brief/);
  assert.match(manager, /genycaster\.nodes\.counts_short/);
  assert.match(manager, /providerProfiles\.activeReadContract/);
  assert.doesNotMatch(manager, /providerProfiles\.selectReadProvider\(nodeId\)/);
  assert.match(read('ui/app/main.cpp'), /activeReadProviderLabel\(\)/);
});

test('unsupported raw notifications do not masquerade as a transient node outage', () => {
  const notifications = read('ui/components/feed/NotificationSurface.qml');
  const shell = read('ui/components/shell/AppShell.qml');
  assert.match(header, /appliedCompatibleReadFallback READ appliedCompatibleReadFallback/);
  assert.match(shell, /providerProfiles\.activeReadContract === "snapchain-v1"/);
  assert.match(shell, /!providerProfiles\.appliedCompatibleReadFallback/);
  assert.match(notifications, /root\.unsupportedRawNotifications/);
  assert.match(notifications, /genycaster\.nodes\.notifications_unavailable/);
  assert.match(notifications, /genycaster\.nodes\.raw_limitations/);
});

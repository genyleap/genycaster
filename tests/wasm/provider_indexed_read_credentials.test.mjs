import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('credential injection is strictly bound to explicit HTTPS indexed provider IDs', () => {
  const boot = read('ui/mobile/main.cpp');
  const iface = read('ui/mobile/runtime_composition.cppm');
  const runtime = read('ui/mobile/runtime_composition.cpp');
  assert.match(boot, /GENYCASTER_READ_PROVIDER_API_KEYS_JSON/);
  assert.match(boot, /readProviderApiKeys\.emplace\(it\.key\(\)\.toStdString\(\), value\.toStdString\(\)\)/);
  assert.match(boot, /profile\.id == it\.key\(\)\.toStdString\(\)/);
  assert.match(boot, /profile\.adapterContract == "hypersnap-v2"/);
  assert.match(boot, /\.scheme\(\)\.compare\(QStringLiteral\("https"\)/);
  assert.match(boot, /ascii < 33U \|\| ascii > 126U/);
  assert.match(boot, /additionalReadProviders, readProviderApiKeys/);
  assert.match(iface, /const std::unordered_map<std::string, std::string>& readProviderApiKeys/);
  assert.match(runtime, /const auto credential = readProviderApiKeys\.find\(profile\.id\)/);
  assert.match(runtime, /\.apiKey = credential == readProviderApiKeys\.end\(\)/);
  assert.doesNotMatch(read('ui/mobile/provider_profiles_controller.cpp'),
    /GENYCASTER_READ_PROVIDER_API_KEYS_JSON|readProviderApiKeys|\.apiKey/);
  assert.doesNotMatch(read('ui/mobile/provider_profiles_controller.hpp'),
    /readProviderApiKeys|\.apiKey/);
});

test('both synchronous and asynchronous requests validate credentials and never follow credentialed redirects', () => {
  const blocking = read('src/providers/hypersnap/request.cpp');
  const async = read('src/providers/hypersnap/async_request.cpp');
  const qtAsync = read('src/transport/qt_network/async_http.cpp');
  const qtBlocking = read('src/transport/qt_network/http.cpp');
  assert.match(blocking, /!config\.baseUrl\.starts_with\("https:\/\/"\) \|\| !validKey/);
  assert.match(blocking, /"x-api-key: " \+ m_config\.apiKey/);
  assert.match(blocking, /"x-api-key: " \+ apiKey/);
  assert.match(async, /request::validateConfig\(m_config\)/);
  assert.match(async, /"x-api-key: " \+ m_config\.apiKey/);
  assert.match(qtAsync, /credentialedApiRequest \? QNetworkRequest::ManualRedirectPolicy/);
  assert.match(qtBlocking, /QNetworkRequest::ManualRedirectPolicy/);
  const tests = read('tests/providers/hypersnap/contract_tests.cpp');
  assert.match(tests, /testReadApiKeysStayOnTheirConfiguredIndexedInstance\(\)/);
  assert.match(tests, /alphaTransport\.requests\(\)\.size\(\)/);
  assert.match(tests, /betaTransport\.postRequests\(\)\.size\(\)/);
  assert.match(tests, /insecureResult\.error\(\)\.code == foundation::error::ErrorCode::InvalidArgument/);
});

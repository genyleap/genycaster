import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const bridge = readFileSync('src/providers/compatibility/browser_wallet/bridge.cpp', 'utf8');
const nativeTests = readFileSync('tests/providers/browser_wallet/bridge_contract_tests.cpp', 'utf8');

test('Wallet Bridge independently canonicalizes HTTPS origins before replacing sessions', () => {
  assert.match(bridge, /std::optional<std::string> canonicalWalletOrigin\(std::string_view raw\)/);
  assert.match(bridge, /constexpr std::string_view scheme\{"https:\/\/"\}/);
  assert.match(bridge, /authority\.find_first_of\(/);
  assert.match(bridge, /authority\.substr\(colon\) != ":443"/);
  assert.match(bridge, /if \(allNumeric\) return std::nullopt/);
  assert.match(bridge, /"\.local", "\.localhost", "\.internal", "\.test", "\.invalid"/);
  const start = bridge.indexOf('foundation::result::Status beginSession(');
  const end = bridge.indexOf('void endSession()', start);
  const begin = bridge.slice(start, end);
  assert.match(begin, /const auto canonicalOrigin = canonicalWalletOrigin\(origin\)/);
  assert.ok(begin.indexOf('if (appName.empty() || !canonicalOrigin)') <
    begin.indexOf('endSession();'), 'reject before any prior session teardown');
  assert.match(begin, /m_origin = std::move\(\*canonicalOrigin\)/);
});

test('Native integration rejects hostile wallet origin shapes and normalizes default TLS port', () => {
  assert.match(nativeTests, /void testCanonicalWalletOriginIsEnforcedAtBridgeBoundary\(\)/);
  for (const value of [
    'http://mini.example', 'https://mini.example:8080',
    'https://mini.example/path', 'https://mini.example?query=1',
    'https://mini.example@evil.example', 'https://127.0.0.1',
    'https://mini.example.local', 'https://mini.example.',
  ]) assert.ok(nativeTests.includes(value), value);
  assert.match(nativeTests, /HTTPS:\/\/MINI\.EXAMPLE:443/);
  assert.match(nativeTests, /page\.body\(\)\.find\("https:\/\/mini\.example"\)/);
  assert.match(nativeTests, /testCanonicalWalletOriginIsEnforcedAtBridgeBoundary\(\);/);
});

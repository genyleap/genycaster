import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL(path, import.meta.url), 'utf8');
const qml = read('../../ui/components/ProfileSurface.qml');
const popup = read('../../ui/components/AccountConnectionsPopup.qml');
const shell = read('../../ui/components/AppShell.qml');
const controller = read('../../ui/mobile/openproof_connections_controller.cpp');
const migration = read('../../services/account-sync/migrations/001_bookmarks.sql');
const config = JSON.parse(read('../../ui/config/public_endpoints.json'));

test('only the currently authenticated OpenProof profile may expose connections', () => {
  assert.match(qml, /root\.isOwnProfile && Number\(root\.accountFid\) > 0/);
  assert.match(qml, /root\.accountIssuer === openProofConnections\.identityIssuer/);
  assert.match(qml, /typeof openProofConnections !== "undefined"/);
  assert.match(qml, /onIsOwnProfileChanged:[\s\S]*?!root\.isOwnProfile[\s\S]*?close\(\)/);
  assert.match(qml, /onVisibleChanged:[\s\S]*?!root\.visible[\s\S]*?close\(\)/);
  assert.match(shell, /accountIssuer: presentationBridge\.accountIdentityIssuer/);
});

test('modal delegates mutations to an official first-party account ceremony', () => {
  assert.match(qml, /openProofConnections\.refresh\(\)/);
  assert.match(popup, /providerSelected\(string provider\)/);
  assert.match(shell, /openProofConnections\.openManager\(provider\)/);
  assert.match(controller, /QDesktopServices::openUrl\(url\)/);
  assert.match(controller, /trustedHttpsEndpoint\(m_accountUrl\)/);
  assert.match(controller, /if \(!m_providers\.contains\(provider\)/);
  assert.match(popup, /Connection status and confirmation are verified in your OpenProof account/);
  assert.doesNotMatch(popup, /XMLHttpRequest|fetch\(|http[s]?:\/\//);
  assert.doesNotMatch(controller, /Authorization.*Bearer|refreshToken|accessToken/);
});

test('origins remain configurable and social data is not copied from OpenProof', () => {
  assert.match(controller, /GENYCASTER_OPENPROOF_PROVIDERS_URL/);
  assert.match(controller, /GENYCASTER_ACCOUNT_MANAGEMENT_URL/);
  assert.match(controller, /GENYCASTER_OPENPROOF_ISSUER_URL/);
  for (const key of ['openproofProviderCatalogUrl', 'accountManagementUrl', 'openproofIssuerUrl'])
    assert.ok(new URL(config[key]).protocol === 'https:');
  assert.match(migration, /CREATE TABLE IF NOT EXISTS bookmarks/);
  assert.match(migration, /CREATE TABLE IF NOT EXISTS bookmark_changes/);
  assert.match(migration, /UNIQUE \(openproof_subject, mutation_id\)/);
  assert.match(migration, /is_deleted boolean NOT NULL/);
  assert.doesNotMatch(migration, /CREATE TABLE.*\b(users|social_connections|identities|profile_tokens)\b/i);
});

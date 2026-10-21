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

test('own signed-in profile always exposes connection manager regardless of login issuer', () => {
  assert.match(qml, /root\.isOwnProfile && Number\(root\.accountFid\) > 0/);
  assert.doesNotMatch(qml, /root\.accountIssuer === openProofConnections\.identityIssuer/);
  assert.match(qml, /typeof openProofConnections === "undefined"/);
  assert.equal((qml.match(/objectName: "profileConnectionsButton"/g) || []).length, 1);
  assert.ok(qml.indexOf('objectName: "profileConnectionsButton"') < qml.indexOf('id: followsYouBadge'), 'action belongs in the avatar area, not next to the long profile name');
  assert.match(qml, /onIsOwnProfileChanged:[\s\S]*?!root\.isOwnProfile[\s\S]*?close\(\)/);
  assert.match(qml, /onVisibleChanged:[\s\S]*?!root\.visible[\s\S]*?close\(\)/);
  assert.match(shell, /accountIssuer: presentationBridge\.accountIdentityIssuer/);
});

test('Genycaster owns account connection actions; OpenProof controls the proof and consent', () => {
  assert.match(qml, /openProofConnections\.refresh\(\)/);
  assert.match(popup, /connectRequested\(string provider\)/);
  assert.match(popup, /disconnectRequested\(string provider, string subject\)/);
  assert.match(popup, /authorizeRequested\(\)/);
  assert.match(popup, /picker\.authorized/);
  assert.match(popup, /picker\.connectionFor\(modelData\)/);
  assert.match(popup, /confirmUnlink = true/);
  assert.match(qml, /openProofConnections\.connectProvider\(provider\)/);
  assert.match(qml, /openProofConnections\.disconnectProvider\(provider, subject\)/);
  assert.match(qml, /openProofConnections\.authorizeAccount\(\)/);
  assert.doesNotMatch(shell, /openProofConnections\.openManager\(provider\)/);
  assert.doesNotMatch(popup, /XMLHttpRequest|fetch\(|http[s]?:\/\//);
  assert.match(controller, /gatewayActionUrl\(QStringLiteral\("\/handoff"\)\)/);
  assert.match(controller, /gatewayActionUrl\(QStringLiteral\("\/disconnect"\)\)/);
  assert.match(controller, /m_viewerFid != requiredFid \|\| fid != requiredFid/);
  assert.match(controller, /m_accountBearer\.fill\(/);
  assert.match(controller, /m_authorization\.begin/);
  assert.match(controller, /QDesktopServices::openUrl\(url\)/);
});

test('origins remain configurable and social data is not copied from OpenProof', () => {
  assert.match(controller, /GENYCASTER_OPENPROOF_PROVIDERS_URL/);
  assert.match(controller, /GENYCASTER_OPENPROOF_CONNECTIONS_GATEWAY_URL/);
  assert.match(controller, /GENYCASTER_OPENPROOF_ISSUER_URL/);
  for (const key of ['openproofProviderCatalogUrl', 'openproofConnectionsGatewayUrl', 'openproofIssuerUrl'])
    assert.ok(new URL(config[key]).protocol === 'https:');
  assert.match(migration, /CREATE TABLE IF NOT EXISTS bookmarks/);
  assert.match(migration, /CREATE TABLE IF NOT EXISTS bookmark_changes/);
  assert.match(migration, /UNIQUE \((?:openproof_issuer,\s*)?openproof_subject, mutation_id\)/);
  assert.match(migration, /is_deleted boolean NOT NULL/);
  assert.doesNotMatch(migration, /CREATE TABLE.*\b(users|social_connections|identities|profile_tokens)\b/i);
});


test('verified OpenProof session commits before its transient account grant (same Qt event queue)', () => {
  const auth = read('../../ui/mobile/browser_openproof_auth.cpp');
  const runtime = read('../../ui/mobile/runtime_ui_bindings.cpp');
  const connectionEvidence = auth.slice(
    auth.indexOf('BrowserOpenProofState authenticated{}'),
    auth.indexOf('m_sessionExpiryTimer.start(m_tokenExpiresSeconds * 1000);'));
  assert.ok(connectionEvidence.indexOf('emitState(std::move(authenticated));')
    < connectionEvidence.indexOf('m_connectionGrant(std::move(m_accessToken)'));
  const queuedGrant = runtime.slice(
    runtime.indexOf('browserAuth->begin(browserObserver,'),
    runtime.indexOf('} else if (route == QStringLiteral("farcaster"))'));
  assert.match(queuedGrant,/QMetaObject::invokeMethod\(&bridge,/);
  assert.match(queuedGrant,/Qt::QueuedConnection/);
  assert.match(queuedGrant,/bridge.accountFid\(\) != fid/);
  assert.match(queuedGrant,/bridge.accountIdentityIssuer\(\) != connections.identityIssuer\(\)/);
  assert.doesNotMatch(queuedGrant,/QTimer::singleShot\(0,/);
  const gui = []; let fid = 0, issuer = '';
  gui.push(() => { fid = 42; issuer = 'openproof'; });
  gui.push(() => fid === 42 && issuer === 'openproof');
  assert.equal(gui.shift()(),undefined);
  assert.equal(gui.shift()(),true);
});

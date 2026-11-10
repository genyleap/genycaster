import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL(path, import.meta.url), 'utf8');
const qml = read('../../ui/components/profile/ProfileSurface.qml');
const popup = read('../../ui/components/profile/ConnectionsPopup.qml');
const shell = read('../../ui/components/shell/AppShell.qml');
const controller = read('../../ui/app/identity/connections.cpp');
const migration = read('../../services/account-sync/migrations/001_bookmarks.sql');
const config = JSON.parse(read('../../ui/config/public_endpoints.json'));

test('profile links, profession and holder trust remain separately scoped and disclosed', () => {
  assert.match(qml,/category: "genycaster-profile-extras"/);
  assert.match(qml,/profileExtrasOwnerKey: root.isOwnProfile/);
  assert.match(qml,/function saveExtraDetails\(profession, rawLinks, category, secondaryCategories, roles\)/);
  assert.match(qml,/profileExtrasSettings.entriesJson = JSON.stringify\(entries\)/);
  assert.match(qml,/genycaster.profile.links_editor_hint/);
  assert.match(qml,/objectName: "profileMoreLinksInline"/);
  assert.match(qml,/id: profileLinksDrawer/);
  assert.match(qml,/id: linksSheetRows/);
  assert.match(qml,/id: profileBackgroundRefresh/);
  assert.match(qml,/GenyDrawer \{\s*id: profileExtrasEditor/);
  assert.match(qml,/GenyDrawer \{\s*id: profileLinksDrawer/);
  assert.match(qml,/root.profileExtraLabel/);
  assert.match(qml,/property bool genyHolderAttested: false/);
  assert.match(qml,/visible: root.genyHolderAttested/);
  assert.match(qml,/id: genyHolderBadge/);
  assert.doesNotMatch(qml,/genyHolderAttested:\s*root\.activeDetail/);
  assert.match(qml,/objectName: "profileProfessionalCategory"/);
  assert.match(qml,/objectName: "profileFollowingCount"/);
  assert.match(qml,/objectName: "profileFollowersCount"/);
  assert.match(qml,/text: qsTrId\("genycaster\.profile\.followers"\)[\s\S]*?color: followersCountButton\.hovered \? Theme\.ink : Theme\.inkMuted/);
});

test('owner profile anchors linked networks to the right with blue or green proof checks', () => {
  const start = qml.indexOf('id: ownerSocialBadgeStrip');
  const end = qml.indexOf('RichText {', start);
  const group = qml.slice(start, end);
  assert.ok(start > 0 && end > start);
  assert.match(group, /layoutDirection: root\.profileContentRtl \? Qt\.RightToLeft : Qt\.LeftToRight/);
  assert.match(group, /width: Math\.min\(implicitWidth/);
  assert.match(group, /visible: root\.isOwnProfile/);
  assert.doesNotMatch(group,/genycaster\.profile\.linked_networks/);
  assert.match(group, /socialBadge\.liveVerified \? Theme\.success : Theme\.accent/);
  assert.match(group, /Theme\.iconCheck/);
  assert.match(group, /objectName: "profileConnectionsButton"/);
  assert.match(qml, /objectName: "profileOwnerActions"/);
});

test('connections opens as mobile bottom sheet and desktop right drawer', () => {
  assert.ok(popup.includes('GenyDrawer {'));
  assert.ok(popup.includes('bottomSheet: mobilePresentation'));
  assert.ok(!popup.includes('edge: mobilePresentation ? Qt.BottomEdge : Qt.RightEdge'));
  assert.match(read('../../ui/components/controls/GenyDrawer.qml'), /edge:\s*bottomSheet\s*\?\s*Qt\.BottomEdge\s*:\s*\(Theme\.rtl\s*\?\s*Qt\.LeftEdge\s*:\s*Qt\.RightEdge\)/);
  assert.ok(popup.includes('Theme.breakpointMobile'));
  assert.ok(popup.includes('dimBackground: true'));
  assert.ok(!popup.includes('GenyModal {'));
});

test('LinkedIn reauthorization waits for a new provider-backed URL rather than an already linked subject', () => {
  assert.match(controller, /m_reconnectingLinkedIn = alreadyLinked/);
  assert.match(controller, /!m_reconnectingLinkedIn \|\| linkedInUrlReady/);
  assert.match(controller, /m_reconnectingLinkedIn = false/);
  assert.match(popup, /picker\.connectRequested\("linkedin"\)/);
});

test('provider Connect opens secure OAuth from the first user gesture and updates the drawer from server confirmation', () => {
  const start = controller.indexOf('void OpenProofConnectionsController::connectProvider(');
  const end = controller.indexOf('void OpenProofConnectionsController::disconnectProvider(', start);
  const connect = controller.slice(start, end);
  assert.ok(controller.includes('prepareGenycasterConnectionWindow'));
  assert.ok(controller.includes('navigateGenycasterConnectionWindow'));
  assert.ok(controller.includes('closeGenycasterConnectionWindow'));
  assert.ok(controller.includes("page.opener = null"));
  assert.ok(connect.indexOf('prepareGenycasterConnectionWindow()') < connect.indexOf('sendAccountRequest('));
  assert.ok(connect.indexOf('sendAccountRequest(') < connect.indexOf('navigateGenycasterConnectionWindow(destination.constData())'));
  assert.ok(connect.includes('opened = QDesktopServices::openUrl(url)'));
  assert.ok(connect.includes('m_connectionPoll.start()'));
  assert.ok(controller.includes('isConnected(m_pendingProvider)'));
  assert.ok(!popup.includes('Continue to secure approval'));
  assert.ok(!popup.includes('Verify with OpenProof'));
  assert.ok(!controller.includes('m_consentUrl'));
  assert.ok(!controller.includes('window.location ='));
});

test('canonical Farcaster proof grants transient Connections access without a second OpenProof device approval', () => {
  const auth = read('../../ui/app/identity/browser_auth.cpp');
  const runtime = read('../../ui/app/runtime/bindings.cpp');
  const broker = read('../../services/media-gateway/browser_farcaster_auth.mjs');
  assert.ok(broker.includes('issueVerifiedAccountTicket('));
  assert.ok(broker.includes('account_expires_in: Math.floor(accountLifetimeMs / 1000)'));
  assert.ok(auth.includes('m_connectionGrant(grant.toUtf8(), subject, fid, grantSeconds)'));
  assert.ok(auth.indexOf('emitState(std::move(authenticated));', auth.indexOf('void BrowserOpenProofAuth::pollFarcaster()'))
    < auth.indexOf('m_connectionGrant(grant.toUtf8(), subject, fid, grantSeconds)'));
  assert.ok(runtime.includes('browserAuth->beginFarcaster(browserObserver,'));
  assert.ok(runtime.includes('connections.acceptRecoveryGrant('));
  assert.ok(runtime.includes('bridge.authenticationRoute() != QStringLiteral("farcaster")'));
  assert.ok(controller.includes('ticketFormat'));
  assert.ok(controller.includes('m_viewerFid != requiredFid || fid != requiredFid'));
});

test('owner remembers only last-known social proofs and never persists account bearers', () => {
  assert.match(controller, /restoreCachedConnections\(\)/);
  assert.match(controller, /saveCachedConnections\(\)/);
  assert.match(controller, /openProofConnections\/lastKnown\//);
  assert.match(controller, /m_cachedSnapshot = true/);
  assert.match(controller, /if \(!m_settings \|\| !m_verified \|\| !m_viewerFid\) return/);
  assert.match(controller, /m_accountBearer\.fill\(/);
  assert.match(qml, /cachedSnapshot: openProofConnections \? openProofConnections\.cachedSnapshot/);
  assert.match(popup, /editAccessRequested/);
  assert.match(popup, /genycaster\.connections\.copy_edit_connections/);
  assert.match(qml, /ownerVerifiedSocialBadgeStrip/);
  assert.match(qml, /root\.isOwnProfile/);
});

test('Farcaster ownership unlocks Connections without a Genyleap password and Telegram callback uses its registered host', () => {
  assert.match(controller, /m_authorization\.beginFarcaster\(/);
  // Connection management still uses Farcaster; private cloud sync has a\n  // separate, opt-in OpenProof device grant.\n  assert.match(controller, /void OpenProofConnectionsController::authorizeCloudAccount\(\)/);
  assert.match(controller, /m_browserConnectionBaseUrl\.path\(\) \+ QStringLiteral\("\/account\/connections\/handoff"\)/);
  assert.match(popup, /genycaster\.connections\.copy_these_links_are_saved_with_your_genycaster_account/);
  assert.match(popup, /genycaster\.connections\.copy_verify_ownership_with_farcaster_to_manage_your_connections/);
  assert.ok(!popup.includes('Continue approval'));
  assert.equal(config.openproofBrowserConnectionBaseUrl, 'https://genyleap.com/openproof');
});

test('OpenProof verification no longer sends the user to the Genyleap account portal', () => {
  const auth = read('../../ui/app/identity/browser_auth.cpp');
  assert.ok(auth.includes('verification_uri_complete'));
  assert.ok(auth.includes('approval.host() != canonicalIssuer.host()'));
  assert.ok(auth.includes('approval.path() != QStringLiteral("/oauth/device")'));
  assert.ok(auth.includes('verificationQuery.queryItemValue(QStringLiteral("user_code")) != userCode'));
  assert.ok(!auth.includes('https://genyleap.com/account'));
  assert.ok(!auth.includes('/openproof/oauth/device?user_code='));
});

test('own signed-in profile always exposes connection manager regardless of login issuer', () => {
  assert.match(qml, /readonly property bool isOwnProfile: Number\(root\.activeDetail\.fid/);
  assert.match(qml, /visible: root\.isOwnProfile/);
  assert.doesNotMatch(qml, /root\.accountIssuer === openProofConnections\.identityIssuer/);
  assert.match(qml, /typeof openProofConnections === "undefined"/);
  assert.equal((qml.match(/objectName: "profileConnectionsButton"/g) || []).length, 1);
  assert.match(qml, /id: ownerSocialBadgeStrip[\s\S]*?objectName: "profileConnectionsButton"/);
  assert.doesNotMatch(qml, /iconGlyph: Theme\.iconPlus\s*\n\s*text: qsTr\("Connections"\)/);
  assert.match(qml, /onIsOwnProfileChanged:[\s\S]*?!root\.isOwnProfile[\s\S]*?close\(\)/);
  assert.match(qml, /onVisibleChanged:[\s\S]*?!root\.visible[\s\S]*?close\(\)/);
  assert.match(shell, /accountIssuer: presentationBridge\.accountIdentityIssuer/);
});

test('Genycaster owns account connection actions; OpenProof controls the proof and consent', () => {
  assert.match(qml, /openProofConnections\.refresh\(\)/);
  assert.match(qml, /AccountConnectionsPopup\s*\{[\s\S]*?parent: Overlay\.overlay/);
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
  assert.doesNotMatch(popup, /XMLHttpRequest|fetch\(/);
  assert.match(popup, /profileUrlFor\(provider\)/);
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
  const auth = read('../../ui/app/identity/browser_auth.cpp');
  const runtime = read('../../ui/app/runtime/bindings.cpp');
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

test('connection selector packages provider-owned SVGs and keeps links in the app', () => {
  const path = '../../ui/assets/icons/genyleap/icons/brands/providers/';
  for (const provider of ['x','github','google','linkedin','telegram','farcaster','ethereum-wallet']) {
    const svg = read(path + provider + '.svg');
    assert.match(svg, /<svg\b/);
    assert.doesNotMatch(svg, /<script\b|<image\b|<use\b|foreignObject|onload\s*=|\bhref\s*=|url\s*\(/i);
    assert.match(popup, new RegExp('case "' + provider + '": return logoRoot \\+ "' + provider + '\\.svg"'));
  }
  assert.match(popup, /qrc:\/qt\/qml\/Farcaster\/Client\/assets\/icons\/genyleap/);
  assert.match(popup, /source: picker\.providerIcon\(providerRow\.modelData\)/);
  assert.match(popup, /Theme\.uiDirection/);
  assert.match(popup, /Theme\.dark|Theme\.surfaceMuted/);
  assert.match(popup, /providerRow\.confirmUnlink = true/);
  assert.match(popup, /picker\.catalogFallback/);
  assert.doesNotMatch(popup, /XMLHttpRequest|fetch\(/);
  assert.match(popup, /genycaster\.connections\.copy_linkedin_does_not_share_a_public_profile_url_through_standard_sign/);
  assert.match(popup, /linkedinSearchFor\(provider\)/);
  assert.match(popup, /encodeURIComponent\(name\)/);
  assert.match(popup, /destinationFor\(providerRow\.modelData\)/);
  assert.match(popup, /qsTrId\("genycaster\.connections\.copy_find"\)/);
  assert.match(popup, /qsTrId\("genycaster\.connections\.copy_reconnect"\)/);
  assert.match(popup, /profile-thirdparty-redirect/);
  assert.match(qml, /profile-thirdparty-redirect/);
  assert.match(controller, /verifiedRedirect/);
  assert.match(popup, /Theme\.iconCheck/);
  assert.doesNotMatch(popup, /Verify profile first|Provider catalog unavailable; showing known options/);
});

test('desktop Connections and shared Settings drawer dismiss by backdrop and Escape', () => {
  const base = read('../../ui/components/controls/GenyDrawer.qml');
  const settings = read('../../ui/components/settings/SettingsPopup.qml');
  assert.match(base, /closePolicy: Popup.CloseOnEscape \| Popup.CloseOnPressOutside/);
  assert.match(base, /sequence: "Escape"/);
  assert.match(base, /enabled: control.visible && !control.bottomSheet/);
  assert.match(base, /onBackgroundTapped: control.close\(\)/);
  assert.match(base, /visible: !control.bottomSheet/);
  assert.match(settings, /GenyDrawer/);
  assert.match(popup, /closePolicy: Popup.CloseOnEscape \| Popup.CloseOnPressOutside/);
});

test('social network badges have stateful checkmarks and real destinations', () => {
  assert.match(qml, /id: ownerSocialBadgeStrip/);
  assert.match(qml, /text: Theme.iconCheck/);
  assert.match(qml, /socialBadge\.liveVerified \? Theme.success/);
  assert.match(qml, /root.openConnections\(\)/);
  assert.match(qml, /item\.profileUrl/);
  assert.match(qml, /searchHref: searchHref/);
  assert.match(qml, /socialBadge\.hovered \? Theme\.surfaceHover/);
  assert.match(qml, /width: Theme\.scaleUi\(248\)/);
  assert.match(qml, /else if \(canFind\) root\.externalRequested/);
  assert.doesNotMatch(qml, /public profile URL unavailable/);
  assert.match(popup, /onClicked: picker.openProfileRequested/);
  assert.doesNotMatch(popup, /saveLinkedInUrlRequested|linkedInUrlError/);
  assert.match(controller, /json\.value\(QStringLiteral\("profile_url"\)\)/);
  assert.match(controller, /url\.host\(\) == QStringLiteral\("linkedin.com"\)/);
});

test('provider OAuth success completes in a self-closing first-party popup', () => {
  const gateway = read('../../services/media-gateway/gateway.mjs');
  const completion = read('../../services/media-gateway/public/genycaster-connect-complete.html');
  assert.match(gateway, /return_to:\s*'\/genycaster-connect-complete.html'/);
  assert.match(completion, /window.close\(\)/);
  assert.match(completion, /id="close"/);
  assert.match(completion, /URLSearchParams\(window.location.search\).has\('op_error'\)/);
  assert.doesNotMatch(completion, /fetch\(|XMLHttpRequest|localStorage|document.cookie/);
  assert.match(controller, /closeGenycasterConnectionWindow\(\)/);
});

test('provider listing failure is a subdued offline hint, never an auth error', () => {
  assert.match(controller, /m_catalogFallback = latest\.isEmpty\(\)/);
  assert.doesNotMatch(controller, /m_error = QStringLiteral\("Provider catalog unavailable/);
  assert.match(qml, /catalogFallback: openProofConnections \? openProofConnections\.catalogFallback/);
});

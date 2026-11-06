import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('existing Farcaster/OpenProof authorization is reused, never exposed to QML',()=>{
  const header=read('ui/app/openproof_connections_controller.hpp');
  const code=read('ui/app/openproof_connections_controller.cpp');
  const api=read('services/account-sync/cloud_state.mjs');
  assert.match(header,/Q_PROPERTY\(bool cloudAuthorizationReady READ cloudAuthorizationReady/);
  assert.match(header,/\(m_verified && !m_accountBearer\.isEmpty\(\)\) \|\| m_cloudSessionVerified/);
  // FID-only local continuation is not enough; a server-verified
  // browser session is recovered automatically before account API use.
  assert.match(code,/restoreAccountSession\(\)/);
  assert.match(code,/bootstrapAccountSession\(\)/);
  assert.match(code,/authorizeCloudRequest\(request\)/);
  assert.doesNotMatch(header,/authorizeCloudAccount\(/);
  assert.doesNotMatch(code,/authorizeCloudAccount\(/);
  assert.match(api,/subject:'farcaster:'\+fid/);
  assert.match(api,/storage-farcaster\.genyleap\.com\/v1\/browser\/openproof\/connections/);
  assert.match(api,/linked\.length!==1/);
  assert.match(code,/request\.setRawHeader\("Authorization", "Bearer " \+ m_accountBearer\)/);
  assert.match(code,/QNetworkRequest::ManualRedirectPolicy/);
  assert.doesNotMatch(header,/Q_PROPERTY\([^\n]*[Bb]earer/);
});

test('native Farcaster recovery grant boots a verified account session before loading profile details',()=> {
  const code=read('ui/app/openproof_connections_controller.cpp');
  const api=read('services/account-sync/cloud_state.mjs');
  const body=code.slice(code.indexOf('void OpenProofConnectionsController::acceptRecoveryGrant('),
    code.indexOf('void OpenProofConnectionsController::stopAuthorization('));
  assert.match(body,/m_accountBearer = std::move\(ticket\)/);
  assert.match(body,/bootstrapAccountSession\(\)/);
  assert.match(body,/checkCloudService\(\)/);
  assert.match(code,/m_verified \|\| m_accountBearer.startsWith\("recovery\."\)/);
  assert.match(code,/void OpenProofConnectionsController::restoreAccountSession\(\)/);
  assert.match(code,/m_cloudSessionVerified=true/);
  assert.match(code,/status==200 && reply->isReadable\(\)/);
  assert.match(code,/QNetworkReply::abort\(\) can synchronously emit finished\(\)/);
  assert.match(api,/header.startsWith\('Bearer recovery\.'\)/);
  assert.match(api,/if\(owner.cookieAuth \|\| typeof store.createSession/);
  // A local recovered FID or an unverified ticket cannot bypass the API.
  assert.doesNotMatch(body,/m_cloudSessionVerified=true/);
});

test('canceling in-flight native cloud login cannot dereference a reentrantly cleared reply',()=>{
  const cpp=read('ui/app/openproof_connections_controller.cpp');
  const header=read('ui/app/openproof_connections_controller.hpp');
  const testSource=read('tests/ui/openproof_connections_tests.cpp');
  const cancel=cpp.slice(cpp.indexOf('void cancelNetworkReply('),
    cpp.indexOf('[[nodiscard]] bool validProviderName('));
  assert.match(header,/void cancelAccountSessionRequest\(\)/);
  assert.match(cpp,/void OpenProofConnectionsController::cancelAccountSessionRequest\(\)[\s\S]*?cancelNetworkReply\(m_sessionRequest, this\)/);
  assert.match(cancel,/QPointer<QNetworkReply> pending = tracked;/);
  assert.match(cancel,/tracked\.clear\(\);[\s\S]*?pending->abort\(\)/);
  assert.match(cancel,/QObject::disconnect\(pending\.data\(\), nullptr, receiver, nullptr\)/);
  assert.match(cancel,/if \(pending\) pending->deleteLater\(\)/);
  assert.doesNotMatch(cpp,/m_[A-Za-z]+(?:Request|Read)->(?:abort|deleteLater)\(\)/);
  assert.match(cpp,/if \(m_sessionRequest != reply\)/);
  assert.equal((cpp.match(/    cancelAccountSessionRequest\(\);/g) || []).length, 2);
  assert.ok((cpp.match(/cancelNetworkReply\(m_[A-Za-z]+(?:Request|Read), this\)/g)||[]).length >= 10);
  assert.match(testSource,/Cancel POST while switching accounts/);
});

test('profile category and links save locally then replicate without sync controls',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  assert.match(profile,/category: "genycaster-profile-extras"/);
  assert.match(profile,/function saveExtraDetails\(/);
  assert.match(profile,/root\.setCloudFlags\(true\)/);
  assert.match(profile,/id: profileBackgroundRefresh/);
  assert.match(profile,/onCloudProfileExtrasReceived\(item\)/);
  assert.match(profile,/root\.restoreCloudProfile\(item\)/);
  assert.match(profile,/profileExtrasOwnerKey: root\.isOwnProfile[\s\S]*?"farcaster:" \+ String\(root\.accountFid\)/);
  assert.match(profile,/function migrateLegacyProfileExtras\(\)/);
  assert.match(profile,/onProfileExtrasOwnerKeyChanged: root\.migrateLegacyProfileExtras\(\)/);
  assert.match(profile,/genycaster:device-local:/);
  assert.doesNotMatch(profile,/objectName: "(?:uploadProfileToCloud|restoreProfileFromCloud|authorizeCloudAccount)"/);
  assert.doesNotMatch(profile,/cloudSyncEnabled/);
  assert.match(profile,/profileCategoryOptions:[\s\S]*?"Technology"/);
  assert.doesNotMatch(profile,/qsTr\("Technology"\)/);
});

test('native profile extras sequences are normalized before category restore and save',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  assert.match(profile,/function profileSequence\(value, limit\)/);
  assert.match(profile,/root\.profileSequence\(item\.links, 5\)/);
  assert.match(profile,/root\.profileSequence\(server\.links, 5\)/);
  assert.match(profile,/root\.profileSequence\(saved\.links, 5\)/);
  assert.doesNotMatch(profile,/Array\.isArray\(server\.links\)/);
  assert.doesNotMatch(profile,/!Array\.isArray\(item\.links\)/);
});
test('LinkedIn saves automatically and does not display upload or restore buttons',()=>{
  const code=read('ui/app/openproof_connections_controller.cpp');
  const popup=read('ui/components/AccountConnectionsPopup.qml');
  const profile=read('ui/components/ProfileSurface.qml');
  assert.match(code,/\.cloudDirty/);
  assert.match(code,/if \(!m_applyingCloudLinkedIn && m_cloudServiceAvailable/);
  assert.match(code,/restoreManualLinkedInCloud\(\)/);
  assert.match(popup,/manualLinkedInRequested\(manualLinkedInField\.text\)/);
  assert.doesNotMatch(popup,/(?:Sync URL|Restore from cloud|Cloud sync awaits|Cloud sync requires)/);
  assert.doesNotMatch(profile,/onUploadCloudLinkedInRequested|onRestoreCloudLinkedInRequested/);
});

test('bookmark changes are owner-scoped, idempotent, and replayed without user intervention',()=>{
  const cpp=read('ui/app/openproof_connections_controller.cpp');
  const header=read('src/presentation/qt/presentation_bridge.hpp');
  const presentation=read('src/presentation/qt/presentation_bridge.cpp');
  const main=read('ui/app/main.cpp');
  const store=read('services/account-sync/pg_store.mjs');
  assert.match(header,/bookmarkMutated\(const QString& castId, bool isDeleted\)/);
  assert.match(header,/applyCloudBookmarkChanges\(const QVariantList& changes\)/);
  assert.match(presentation,/emit bookmarkMutated\(id, true\)/);
  assert.match(presentation,/emit bookmarkMutated\(id, false\)/);
  assert.match(presentation,/emit cloudBookmarkChangesReceived|emit bookmarksChanged/);
  assert.match(cpp,/m_bookmarkOutbox\.push_back\(item\)/);
  assert.match(cpp,/\/outbox/);
  assert.match(cpp,/\/cursor/);
  assert.match(cpp,/QUuid::createUuid\(\)/);
  assert.match(cpp,/emit cloudBookmarkChangesReceived\(changes\)/);
  assert.match(cpp,/m_bookmarkHistoryKeys\.contains\(hash\)/);
  assert.match(cpp,/m_legacyBookmarksPending/);
  assert.match(cpp,/bookmarkStorageKey\(\)\+QStringLiteral\("\/migrated"\)/);
  assert.match(main,/PresentationBridge::bookmarkMutated/);
  assert.match(main,/PresentationBridge::cloudBookmarkChangesReceived|OpenProofConnectionsController::cloudBookmarkChangesReceived/);
  assert.match(store,/ON CONFLICT \(openproof_issuer,openproof_subject,mutation_id\) DO NOTHING/);
  assert.match(store,/ORDER BY sequence ASC LIMIT 200/);
});

test('notification read state is replicated invisibly with a monotonic owner-scoped watermark',()=>{
  const controller=read('ui/app/openproof_connections_controller.cpp');
  const header=read('ui/app/openproof_connections_controller.hpp');
  const main=read('ui/app/main.cpp');
  const pg=read('services/account-sync/pg_store.mjs');
  assert.match(header,/cloudNotificationSeenAtReceived\(qulonglong fid, qlonglong observedAtMs\)/);
  assert.match(header,/recordNotificationSeenAt\(qulonglong fid, qlonglong observedAtMs\)/);
  assert.match(controller,/\/api\/v1\/cloud\/notification_seen/);
  assert.match(controller,/QStringLiteral\("seen_at_ms"\)/);
  assert.match(controller,/observedAtMs <= m_notificationSeenAtMs/);
  assert.match(controller,/m_notificationSeenAtMs>m_notificationRemoteAtMs/);
  assert.match(controller,/m_notificationRemoteAtMs=observed/);
  assert.match(main,/PresentationBridge::notificationSeenAtChanged/);
  assert.match(main,/OpenProofConnectionsController::cloudNotificationSeenAtReceived/);
  assert.match(pg,/normalizeRow\(entity,row\)/);
  assert.match(pg,/return normalizeRow\('notifications',result.rows\[0\]\)/);
});

test('profile table is private by default with optimistic revision checks',()=>{
  const sql=read('services/account-sync/migrations/003_profile_extras.sql');
  const db=read('services/account-sync/pg_store.mjs');
  assert.match(sql,/CREATE TABLE IF NOT EXISTS profile_extras/);
  assert.match(sql,/PRIMARY KEY \(issuer, subject\)/);
  assert.match(sql,/publicly_visible boolean NOT NULL DEFAULT false/);
  assert.match(db,/WHERE profile_extras\.revision=\$6/);
});

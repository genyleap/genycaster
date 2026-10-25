import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('existing Farcaster/OpenProof authorization is reused, never exposed to QML',()=>{
  const header=read('ui/mobile/openproof_connections_controller.hpp');
  const code=read('ui/mobile/openproof_connections_controller.cpp');
  const api=read('services/account-sync/cloud_state.mjs');
  assert.match(header,/Q_PROPERTY\(bool cloudAuthorizationReady READ cloudAuthorizationReady/);
  assert.match(header,/return m_verified && !m_accountBearer\.isEmpty\(\)/);
  assert.doesNotMatch(header,/authorizeCloudAccount\(/);
  assert.doesNotMatch(code,/authorizeCloudAccount\(/);
  assert.match(api,/subject:'farcaster:'\+fid/);
  assert.match(api,/storage-farcaster\.genyleap\.com\/v1\/browser\/openproof\/connections/);
  assert.match(api,/linked\.length!==1/);
  assert.match(code,/request\.setRawHeader\("Authorization", "Bearer " \+ m_accountBearer\)/);
  assert.match(code,/QNetworkRequest::ManualRedirectPolicy/);
  assert.doesNotMatch(header,/Q_PROPERTY\([^\n]*[Bb]earer/);
});

test('profile category and links save locally then replicate without sync controls',()=>{
  const profile=read('ui/components/ProfileSurface.qml');
  assert.match(profile,/category: "genycaster-profile-extras"/);
  assert.match(profile,/function saveExtraDetails\(/);
  assert.match(profile,/root\.setCloudFlags\(true\)/);
  assert.match(profile,/id: profileBackgroundRefresh/);
  assert.match(profile,/onCloudProfileExtrasReceived\(item\)/);
  assert.match(profile,/root\.restoreCloudProfile\(item\)/);
  assert.doesNotMatch(profile,/objectName: "(?:uploadProfileToCloud|restoreProfileFromCloud|authorizeCloudAccount)"/);
  assert.doesNotMatch(profile,/cloudSyncEnabled/);
  assert.match(profile,/profileCategoryOptions:[\s\S]*?"Technology"/);
  assert.doesNotMatch(profile,/qsTr\("Technology"\)/);
});

test('LinkedIn saves automatically and does not display upload or restore buttons',()=>{
  const code=read('ui/mobile/openproof_connections_controller.cpp');
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
  const cpp=read('ui/mobile/openproof_connections_controller.cpp');
  const header=read('src/presentation/qt/presentation_bridge.hpp');
  const presentation=read('src/presentation/qt/presentation_bridge.cpp');
  const main=read('ui/mobile/main.cpp');
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
  assert.match(main,/PresentationBridge::bookmarkMutated/);
  assert.match(main,/PresentationBridge::cloudBookmarkChangesReceived|OpenProofConnectionsController::cloudBookmarkChangesReceived/);
  assert.match(store,/ON CONFLICT \(openproof_issuer,openproof_subject,mutation_id\) DO NOTHING/);
  assert.match(store,/ORDER BY sequence ASC LIMIT 200/);
});

test('profile table is private by default with optimistic revision checks',()=>{
  const sql=read('services/account-sync/migrations/003_profile_extras.sql');
  const db=read('services/account-sync/pg_store.mjs');
  assert.match(sql,/CREATE TABLE IF NOT EXISTS profile_extras/);
  assert.match(sql,/PRIMARY KEY \(issuer, subject\)/);
  assert.match(sql,/publicly_visible boolean NOT NULL DEFAULT false/);
  assert.match(db,/WHERE profile_extras\.revision=\$6/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');
const bridge=source('src/presentation/qt/presentation_bridge.cpp');
const controller=source('ui/mobile/openproof_connections_controller.cpp');
const surface=source('ui/components/BookmarksSurface.qml');
const profile=source('ui/components/ProfileSurface.qml');
const store=source('services/account-sync/pg_store.mjs');
test('bookmark second click compares canonical prefixed hash rather than creating duplicates',()=>{
  assert.match(bridge,/if \(canonicalProtocolCastId\(m_bookmarkedCasts\.at\(index\)/);
  assert.match(bridge,/normalizedAccountBookmarks\(m_bookmarkedCasts\)/);
  assert.match(bridge,/QHash<QString,qsizetype> positions/);
  assert.match(bridge,/emit bookmarkMutated\(id, true\)/);
  assert.match(bridge,/void PresentationBridge::removeBookmark/);
});
test('bookmark view supports account refresh, remove, priority and sorting without deleting different hashes',()=>{
  assert.match(surface,/PullToRefresh \{/);
  assert.match(surface,/onRefreshRequested: root\.refreshRequested\(\)/);
  assert.match(surface,/id: bookmarkSortPicker/);
  assert.match(surface,/objectName: "bookmarkRemoveMobileButton"/);
  assert.match(surface,/objectName: "bookmarkPriorityMobileButton"/);
  assert.match(surface,/customIconColor: Number\(modelData.priority/);
  assert.match(surface,/iconOnly: true/);
  assert.match(surface,/root\.priorityRequested/);
  assert.match(controller,/void OpenProofConnectionsController::refreshAccountBookmarks/);
  assert.match(controller,/QStringLiteral\("state"\),QStringLiteral\("1"\)/);
  assert.match(store,/async listBookmarkSnapshot/);
});
test('bookmark view accepts native C++ QVariantList and exposes pending/error states',()=>{
  const header=source('ui/mobile/openproof_connections_controller.hpp');
  const shell=source('ui/components/AppShell.qml');
  assert.doesNotMatch(surface,/Array\.isArray\(root\.rows\)/);
  assert.match(surface,/Number\(sequence\.length \|\| 0\)/);
  assert.match(surface,/objectName: "bookmarkAccountStatus"/);
  assert.match(shell,/pendingCount: openProofConnections\.bookmarkPendingCount/);
  assert.match(header,/Q_PROPERTY\(QString bookmarkError READ bookmarkError/);
  assert.match(controller,/pendingHashes\.contains\(hash\)/);
  assert.match(controller,/first\.remove\(QStringLiteral\("preview"\)\)/);
});
test('bookmarked Cast preview and priority are owner-scoped with explicit v5 schema gating',()=>{
  assert.match(store,/bookmarkPreviewsReady=versions\.has\(5\)/);
  assert.match(store,/async listBookmarkChanges[\s\S]*?preview/);
  assert.match(store,/preview=COALESCE\(EXCLUDED\.preview,bookmarks\.preview\)/);
  assert.match(source('services/account-sync/migrations/005_bookmark_previews.sql'),/VALUES\(5\)/);
  assert.match(controller,/publishBookmarkPreviewIfMissing/);
  assert.match(source('ui/mobile/main.cpp'),/presentationBridge\.bookmarkEntry\(castId\)/);
});
test('profile display prefers server-confirmed same-FID account fields to clean QSettings cache',()=>{
  assert.match(profile,/const server = openProofConnections\.cloudProfileExtras/);
  assert.match(profile,/!root\.cloudLocalDirty/);
  assert.match(source('ui/mobile/openproof_connections_controller.hpp'),/Q_PROPERTY\(QVariantMap cloudProfileExtras/);
  assert.match(controller,/m_cloudProfileExtras=fields/);
});
test('connection labels are portable yet separate from upstream verified authority',()=>{
  assert.match(controller,/void OpenProofConnectionsController::publishPortableConnectionLabels/);
  assert.match(controller,/void OpenProofConnectionsController::refreshAccountConnectionsSnapshot/);
  assert.match(controller,/m_cachedSnapshot=!m_connections\.isEmpty\(\)/);
  assert.match(store,/async importConnectionLabels/);
  assert.match(store,/last_confirmed_at IS NULL/);
  assert.match(source('ui/components/AccountConnectionsPopup.qml'),/genycaster\.connections\.copy_saved_account_links/);
});

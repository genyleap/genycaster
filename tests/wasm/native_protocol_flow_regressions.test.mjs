import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('followers and following never reserve the Explorer feed header',()=>{
  const source=read('ui/components/feed/FeedSurface.qml');
  assert.equal((source.match(/header:\s*root\.socialMode \? null : feedHeaderComponent/g)||[]).length,2);
  assert.match(source,/implicitHeight:\s*root\.socialMode \? 0/);
  assert.match(source,/root\.socialMode \? profileRowComponent/);
});

test('attached drawer corners are square across themes and display directions',()=>{
  const drawer=read('ui/components/controls/GenyDrawer.qml');
  assert.match(drawer,/neonAmbient:\s*false/);
  assert.match(drawer,/neonInteractive:\s*false/);
  assert.match(drawer,/shadowEnabled:\s*false/);
  assert.match(drawer,/leftInset:\s*floatingSheet && edge !== Qt\.LeftEdge/);
  assert.match(drawer,/rightInset:\s*floatingSheet && edge !== Qt\.RightEdge/);
  assert.match(drawer,/bottomInset:\s*floatingSheet && edge !== Qt\.BottomEdge/);
});

test('native FID session renewal only after matching shard-zero registry and local custody',()=>{
  const transport=read('ui/app/identity/native_publish.cpp');
  const runtime=read('ui/app/runtime/bindings.cpp');
  assert.match(transport,/lookupByCustody\(custody/);
  assert.match(runtime,/nativeRegistry->verifyCustodyForSession\(address/);
  assert.match(runtime,/\*confirmed != previous\.fid\.value/);
  assert.match(runtime,/!protectedCustody->backupVerified\(\)/);
  assert.match(runtime,/current->identityIssuer != previous\.identityIssuer/);
  assert.match(runtime,/current->authenticatedAt != previous\.authenticatedAt/);
  assert.match(runtime,/acceptVerifiedNativeCustody\(/);
  assert.ok(!runtime.includes('renewed.identityIssuer = "farcaster:optimism-id-registry"'));
});

test('streak records every confirmed authored cast, not only the rich composer path',()=>{
  const shell=read('ui/components/shell/AppShell.qml');
  const start=shell.indexOf('Streak is not a rich-composer feature');
  const rich=shell.indexOf('if (root.awaitingRichCommit',start);
  assert.ok(start>0 && rich>start);
  assert.match(shell.slice(start,rich),/openProofConnections\.recordStreakCast\(castId\)/);
  const connections=read('ui/app/identity/connections.cpp');
  assert.match(connections,/void OpenProofConnectionsController::retryAccountSession\(\)/);
  const editor=read('ui/components/profile/ProfileSurface.qml');
  assert.match(editor,/openProofConnections\.retryAccountSession\(\)/);
  assert.match(editor,/genycaster\.streak\.verified_session_required/);
});

test('native connections identify separate OpenProof wallet links, never fake FID verification',()=>{
  const popup=read('ui/components/profile/ConnectionsPopup.qml');
  assert.match(popup,/objectName:\s*"nativeOpenProofConnections"/);
  assert.match(popup,/genycaster\.connections\.native_links_not_fid_verified/);
  assert.match(popup,/https:\/\/genyleap\.com\/openproof/);
  const runtime=read('ui/app/runtime/bindings.cpp');
  assert.match(runtime,/genycaster\.profile\.username_proof_missing/);
  assert.match(read('ui/components/profile/ProfileSurface.qml'),
    /genycaster\.profile\.native_username_registration_pending/);
});

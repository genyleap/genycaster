import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('composer saves or clears only ordinary FID-scoped drafts and never publishes managed tokens',()=>{
  const composer=read('src/presentation/qt/rich_composer_controller.cpp');
  const header=read('src/presentation/qt/rich_composer_controller.hpp');
  assert.match(header,/draftLocallyChanged\(qulonglong fid, bool deleted\)/);
  assert.match(composer,/emit owner\.draftLocallyChanged\(viewerFid, false\)/);
  assert.match(composer,/emit draftLocallyChanged\(m_impl->viewerFid, true\)/);
  assert.match(composer,/if \(changed && viewerFid && !applyingRemote\)/);
  assert.match(composer,/if \(!deleted && \(serialized\.isEmpty\(\)/);
  assert.match(composer,/m_impl->dirty \|\| m_impl->contextDocument/);
  assert.match(composer,/m_impl->applyingRemote=true/);
  assert.match(composer,/m_impl->composerActive[\s\S]*?m_impl->dirty \|\| m_impl->contextDocument/);
  assert.match(composer,/if \(!active && m_impl->contextDocument\) \{/);
  assert.match(composer,/m_impl->contextDocument=false;[\s\S]*?m_impl->newEmpty\(false\)/);
  const drawer=read('ui/components/ComposerDrawer.qml');
  assert.match(drawer,/onOpened: \{\s*richComposer\.setComposerActive\(true\)/);
  assert.match(drawer,/onClosed: \{[\s\S]*?richComposer\.setComposerActive\(false\)/);
  assert.match(composer,/m_impl->settings\.remove\(m_impl->managedDraftKey\(\)\)/);
});

test('account draft replication reuses verified native bearer with durable per-FID queue and revision CAS',()=>{
  const account=read('ui/mobile/openproof_connections_controller.cpp');
  const main=read('ui/mobile/main.cpp');
  assert.match(account,/void OpenProofConnectionsController::recordDraftMutation/);
  assert.match(account,/genycaster\/draft-service\/fid_%1/);
  assert.match(account,/QStringLiteral\("\/dirty"\),true/);
  assert.match(account,/m_draftMutationSerial\+\+/);
  assert.match(account,/m_draftRevision!=remoteRevision/);
  assert.match(account,/m_draftConflict=true/);
  // First read of an existing server draft is not a concurrency conflict
  // merely because an older client left a locally scoped draft behind.
  assert.match(account,/if \(remoteRevision==0 && !seen && hasLocalDraft && !dirty\) \{/);
  assert.match(account,/legacy-device-backup/);
  assert.match(account,/if \(differs && !previous\.isEmpty\(\)\)/);
  assert.match(account,/localDoc\.object\(\)==serverDoc/);
  assert.match(account,/QStringLiteral\("\/dirty"\),false/);
  assert.match(account,/status==409/);
  assert.match(account,/setRawHeader\("Authorization","Bearer "\+m_accountBearer\)/);
  assert.match(account,/cancelNetworkReply\(m_draftRequest, this\)/);
  assert.doesNotMatch(account,/m_draftRequest->abort\(\)/);
  assert.match(account,/generation!=m_grantGeneration \|\| viewer!=m_viewerFid/);
  assert.match(account,/m_draftRemoteRevision!=m_draftRevision/);
  assert.match(main,/RichComposerController::draftLocallyChanged/);
  assert.match(main,/OpenProofConnectionsController::cloudDraftSnapshotReceived/);
  assert.match(main,/richComposerController\.applyAccountDraft\(fid, document, deleted\)/);
  assert.match(main,/openProofConnections\.confirmDraftSnapshot\(fid, revision\)/);
});

test('draft transport transfers serialized document only, never local managed asset authority',()=>{
  const account=read('ui/mobile/openproof_connections_controller.cpp');
  const write=account.slice(account.indexOf('void OpenProofConnectionsController::uploadDraftChange()'),
    account.indexOf('void OpenProofConnectionsController::reconcileLinkedIn()'));
  assert.match(write,/content\.insert\(QStringLiteral\("document"\),doc\.object\(\)\)/);
  assert.doesNotMatch(write,/managedDraftKey|managementToken|richManagedAssetsV1/);
});

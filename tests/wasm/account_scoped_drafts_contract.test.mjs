import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('composer drafts and managed media are stored in the same FID-specific namespace',()=>{
  const cpp=read('src/presentation/qt/rich_composer_controller.cpp');
  const header=read('src/presentation/qt/rich_composer_controller.hpp');
  assert.match(header,/void setViewerFid\(qulonglong fid\)/);
  assert.match(cpp,/composer\/accounts\/fid_%1\/richDraftV1/);
  assert.match(cpp,/composer\/accounts\/fid_%1\/richManagedAssetsV1/);
  assert.match(cpp,/settings\.setValue\(draftKey\(\), serialized\)/);
  assert.match(cpp,/serialized=documentToJson\(document, markdown\)/);
  assert.match(cpp,/settings\.setValue\(managedDraftKey\(\), managedAssetsToJson/);
  assert.match(cpp,/settings\.value\(m_impl->draftKey\(\)\)/);
  assert.match(cpp,/settings\.value\(m_impl->managedDraftKey\(\)\)/);
  assert.match(cpp,/settings\.remove\(m_impl->draftKey\(\)\)/);
  assert.match(cpp,/settings\.remove\(m_impl->managedDraftKey\(\)\)/);
  // Older unassigned drafts are left intact, not silently assigned to a
  // newly signed-in FID (which could be a different account).
  assert.match(cpp,/return viewerFid\s*\?/);
  assert.match(cpp,/: QString::fromLatin1\(DraftKey\)/);
  assert.match(cpp,/: QString::fromLatin1\(DraftManagedAssetsKey\)/);
});

test('account switching persists prior pending edits before clearing the editor',()=>{
  const cpp=read('src/presentation/qt/rich_composer_controller.cpp');
  const main=read('ui/mobile/main.cpp');
  assert.match(cpp,/void RichComposerController::setViewerFid\(const qulonglong fid\)/);
  assert.match(cpp,/if \(m_impl->dirty && !m_impl->contextDocument\)\s*m_impl->saveDraft\(false\)/);
  assert.match(cpp,/m_impl->viewerFid = fid;\s*m_impl->contextDocument = false;\s*m_impl->newEmpty\(false\);/);
  assert.match(cpp,/m_impl->newEmpty\(false\);[\s\S]*?restoreDraft\(\);/);
  assert.match(main,/authenticationStateChanged,[\s\S]*?richComposerController\.setViewerFid\(fid\)/);
  assert.match(main,/richComposerController\.setViewerFid\(initialFid\)/);
});

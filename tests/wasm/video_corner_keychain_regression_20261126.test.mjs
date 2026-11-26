import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
test('video corners use four independent covers above rectangular overlays',()=>{
  const player=read('ui/components/media/GenyVideoPlayer.qml');
  const shape=player.slice(player.indexOf('id: videoCornerMask'),player.indexOf('// Lightweight scene-graph scrim'));
  assert.equal((shape.match(/ShapePath \{/g)||[]).length,4);
  assert.doesNotMatch(shape,/OddEvenFill|PathMove/);
  assert.match(shape,/z: 19/);
  assert.match(shape,/cornerRadius \* \(1 - cubicFactor\)/);
  assert.match(player,/z: 20\s+highQualityEdges: true/);
  assert.match(player,/anchors.leftMargin: root.fullscreen \? 0 : videoCornerMask.cornerRadius/);
  assert.equal((player.match(/\bVideoOutput\s*\{/g)||[]).length,1);
  assert.doesNotMatch(player,/layer\.enabled:\s*true/);
});
test('video cover receives its real parent surface, including bookmarks',()=>{
  assert.match(read('ui/components/profile/BookmarksSurface.qml'),/mediaSurfaceColor: bookmarkCard.fillColor/);
  for(const path of ['feed/CastItem','content/RichCastRenderer','media/CastEmbedsPanel'])
    assert.match(read('ui/components/'+path+'.qml'),/mediaSurfaceColor: root.mediaSurfaceColor/);
  assert.match(read('ui/components/media/EmbedPreview.qml'),/cornerCoverColor: root.mediaSurfaceColor/);
  assert.match(read('ui/components/media/VideoPreview.qml'),/cornerMaskColor: root.cornerCoverColor/);
  assert.match(read('ui/components/media/EmbedCard.qml'),/cornerCoverColor: root.fillColor/);
});
test('device seal consumes a single already-authorized key without caching social signers',()=>{
  const vault=read('src/platform/signer_vault_macos.cpp');
  assert.match(vault,/m_service == "org.genycaster.device-session" && fid.value == 1U/);
  assert.match(vault,/PkeyPtr key = std::move\(m_oneShotDeviceKey\)/);
  assert.match(vault,/if \(!key\) \{\s*auto loaded = loadSeed\(m_service, m_fid\)/);
  assert.match(vault,/OPENSSL_cleanse/);
  const store=read('src/storage/session_store.cpp');
  assert.match(store,/auto key = m_deviceVault->load\(deviceSlot\)[\s\S]{0,400}\(\*key\)->sign\(sessionSealPayload\(session\)\)/);
  assert.match(store,/proof->bytes != stored/);
});
test('denied Keychain authorization is not an automatic restore retry',()=>{
  assert.match(read('src/platform/signer_keychain_boundary.cpp'),/status == errSecUserCanceled \|\| status == errSecAuthFailed/);
  assert.match(read('src/platform/signer_vault_macos.cpp'),/failure.authorizationDenied \? foundation::error::ErrorCode::Unauthorized/);
  const coordinator=read('src/application/authentication.cppm');
  const retry=coordinator.slice(coordinator.indexOf('m_restoreAllowed=state.phase'),coordinator.indexOf('m_restoreDue=now+m_restoreDelay'));
  assert.doesNotMatch(retry,/Code::Unauthorized/);
  assert.match(retry,/Code::Unavailable/);
  assert.match(read('properties/macOS/Configure.cmake'),/MACOSX_BUNDLE_GUI_IDENTIFIER "com.genyleap.genycaster"/);
  assert.match(read('properties/macOS/Configure.cmake'),/GENYCASTER_MACOS_LOCAL_SIGNING_IDENTITY/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=(name)=>readFileSync(name,'utf8');
const controller=read('ui/mobile/openproof_connections_controller.cpp');
const qml=read('ui/components/BottomNavigation.qml');
const gateway=read('ui/mobile/runtime_ui_bindings.cpp');
const capability=read('src/capabilities/auth.cppm');
const session=read('src/application/session.cpp');
const provider=read('src/providers/openproof_auth/auth.cpp');

test('live appearance changes never turn glass off for transient graphics-info states',()=>{
  assert.match(qml,/useBackdropBlur: glassEnabled && blurSourceItem !== null/);
  assert.doesNotMatch(qml,/root\.GraphicsInfo\.api\s*!==/);
  assert.match(qml,/function onDarkChanged\(\) \{ backdropRefresh\.restart\(\) \}/);
  assert.match(qml,/capsuleBackdrop\.scheduleUpdate\(\)/);
});

test('changing native account never discards offline topic suggestions',()=>{
  assert.match(controller,/\[\[nodiscard\]\] QVariantList offlineTopicCatalog\(\)/);
  assert.match(controller,/m_topicCatalog = offlineTopicCatalog\(\)/);
  assert.doesNotMatch(controller,/m_topicCatalog\.clear\(\)/);
  assert.match(controller,/if\(m_topicCatalogRequest \|\| !cloudAuthorizationReady\(\)/);
  assert.match(controller,/authorizeCloudRequest\(request\)/);
});

test('verified native OpenProof login delivers ephemeral grant only through trusted C++',()=>{
  assert.match(capability,/struct VerifiedAccountGrant final/);
  assert.match(provider,/\.accountGrant = verifiedAccountGrant\(\*account\)/);
  assert.match(provider,/if \(!account\.farcasterFid \|\| m_accessToken\.empty\(\)/);
  assert.match(session,/polled->accountGrant->fid == m_impl->current->fid\.value/);
  assert.match(gateway,/sessionService->pollSignIn\(\s*sessionObserver, accountGrantObserver\)/);
  assert.match(gateway,/bridge\.accountFid\(\) == fid/);
  assert.match(gateway,/connections\.acceptAuthenticatedGrant\(/);
  assert.match(gateway,/std::fill\(grant\.bearer\.begin\(\), grant\.bearer\.end\(\), '\\0'\)/);
  assert.doesNotMatch(read('ui/mobile/openproof_connections_controller.hpp'),/Q_PROPERTY\([^\n]*[Bb]earer/);
  assert.doesNotMatch(read('src/application/session.cppm'),/class SessionService[^]*persistAccountGrant/);
});

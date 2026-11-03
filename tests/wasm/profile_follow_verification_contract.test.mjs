import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('a visited profile requests independently verified incoming and outgoing relationships',()=>{
  const runtime=source('ui/mobile/runtime_ui_bindings.cpp');
  const begin=runtime.indexOf('PresentationBridge::profileRequested');
  const end=runtime.indexOf('QMetaObject::invokeMethod(&bridge,',begin);
  assert.ok(begin>0 && end>begin);
  const block=runtime.slice(begin,end);
  assert.match(block,/refreshProfileFollowsMe\(bridge, runtime, \*accountFid, state\.profile->fid\);/);
  assert.match(block,/refreshProfileFollowsMe\(bridge, runtime, \*accountFid, state\.profile->fid,\s*true\);/);
});

test('verified outgoing state reaches active profile, not only search suggestions',()=>{
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const begin=bridge.indexOf('void PresentationBridge::applySearchFollowing');
  const end=bridge.indexOf('void PresentationBridge::applyProfileFollowsMe',begin);
  assert.ok(begin>0 && end>begin);
  const method=bridge.slice(begin,end);
  assert.match(method,/m_activeView == QStringLiteral\("profile"\)/);
  assert.match(method,/m_detail\.insert\(QStringLiteral\("followingByMe"\), following\)/);
  assert.match(method,/m_detail\.insert\(QStringLiteral\("followingKnown"\), true\)/);
  assert.match(method,/m_profileDetailCache\.insert\(fid, m_detail\)/);
  assert.match(method,/emit stateChanged\(\)/);
});

test('Follow remains discoverable with an unknown relationship and never sends a blind protocol write',()=>{
  const qml=source('ui/components/ProfileSurface.qml');
  const begin=qml.indexOf('visible: Number(root.activeDetail.fid || 0) > 0 && !root.isOwnProfile');
  const end=qml.indexOf('root.followingMutationRequested(',begin);
  assert.ok(begin>0 && end>begin);
  const button=qml.slice(begin,end);
  assert.match(button,/enabled: Boolean\(root\.activeDetail\.followingKnown\) && !root\.busy/);
  assert.match(button,/qsTrId\("genycaster.search.relationship_unknown"\)/);
  assert.match(button,/qsTrId\("genycaster.profile.unfollow"\)/);
});

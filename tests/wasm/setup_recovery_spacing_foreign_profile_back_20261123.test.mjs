import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = p=>readFileSync(p,'utf8');
const setup=read('ui/components/setup/InitialSetup.qml');
const shell=read('ui/components/shell/AppShell.qml');
const cpp=read('src/presentation/qt/presentation_bridge.cpp');
const header=read('src/presentation/qt/presentation_bridge.hpp');

test('expanding 24-word recovery removes the entire sign-in action grid and spacer',()=>{
  const stage=setup.slice(setup.indexOf('id: signInStage'),setup.indexOf('id: verifyingStage'));
  assert.match(stage,/Item \{\s*visible: !root\.showRecovery\s*Layout\.preferredHeight: Theme\.space2/);
  const actionGrid=stage.slice(stage.indexOf('// Verified Farcaster sign-in'),stage.indexOf('objectName: "setupExistingConnections"'));
  assert.match(actionGrid,/GridLayout \{\s*\/\/ Hide the entire action grid while recovery is expanded\.[\s\S]*?visible: !root\.showRecovery && !root\.authenticated\s*&& root\.existingProtocolSelected/);
  assert.match(actionGrid,/objectName: "setupExistingRecoveryPhrase"/);
  const recovery=stage.slice(stage.indexOf('objectName: "setupExistingConnections"'));
  assert.match(recovery,/ColumnLayout \{\s*visible: root\.showRecovery && \(root\.existingProtocolSelected \|\| root\.resumingRegistration\)/);
  assert.match(recovery,/objectName: "nativeRecoveryWordGrid"/);
  assert.match(recovery,/Layout\.topMargin: Theme\.space2/);
});

test('clearing shell-route history blocks re-adding the hidden Cast or profile as a breadcrumb',()=>{
  const clear=cpp.split('void PresentationBridge::clearDetailHistory()')[1]
    .split('void PresentationBridge::openCast(')[0];
  assert.match(clear,/m_detailHistory\.clear\(\)/);
  assert.match(clear,/m_discardNextDetailOrigin = true/);
  assert.match(header,/bool m_discardNextDetailOrigin\{false\}/);
  const remember=cpp.split('void PresentationBridge::rememberDetailLocation()')[1]
    .split('void PresentationBridge::clearDetailHistory()')[0];
  assert.match(remember,/if \(m_restoringDetailHistory\) return/);
  assert.match(remember,/std::exchange\(m_discardNextDetailOrigin, false\)/);
  assert.ok(remember.indexOf('std::exchange(m_discardNextDetailOrigin, false)')<
            remember.indexOf('DetailLocation location{}'));
});

test('direct notification-to-foreign-profile Back restores Notifications, never hidden prior Cast',()=>{
  const notifications=shell.slice(shell.indexOf('NotificationSurface {'),shell.indexOf('Behavior on opacity',shell.indexOf('NotificationSurface {')));
  assert.match(notifications,/onProfileRequested: fid => \{\s*root\.rememberShellRouteForReturn\(\)\s*presentationBridge\.clearDetailHistory\(\)\s*root\.shellRoute = ""/);
  assert.match(notifications,/root\.openProfileFromContent\(fid\)/);
  assert.match(shell,/function returnFromProfileSurface\(\) \{\s*presentationBridge\.returnFromDetail\(\)\s*root\.returnToPendingShellRoute\(\)/);
  const back=cpp.split('void PresentationBridge::returnFromDetail()')[1]
    .split('void PresentationBridge::loadMore()')[0];
  assert.match(back,/while \(!m_detailHistory\.empty\(\)\)/);
  assert.match(back,/showFeed\(\);/);
  assert.match(shell,/if \(root\.pendingReturnShellRoute\.length === 0\s*\|\| presentationBridge\.activeView !== "feed"\)/);
  assert.match(shell,/root\.shellRoute = root\.pendingReturnShellRoute/);
});

test('notification-to-Cast-to-foreign-profile Back returns Cast before Notifications',()=>{
  const notifications=shell.slice(shell.indexOf('NotificationSurface {'),shell.indexOf('Behavior on opacity',shell.indexOf('NotificationSurface {')));
  assert.match(notifications,/onCastRequested: castId => \{[\s\S]*?presentationBridge\.clearDetailHistory\(\)[\s\S]*?presentationBridge\.openCast\(castId\)/);
  const detail=shell.slice(shell.indexOf('id: castSurfaceLoader'),shell.indexOf('onReplyRequested: cast => root.openComposer(cast)',shell.indexOf('id: castSurfaceLoader')));
  assert.match(detail,/onProfileRequested: fid => \{\s*\+\+root\.profileTopRevision\s*root\.openProfileFromContent\(fid\)/);
  const open=cpp.split('void PresentationBridge::openProfileFid(')[1]
    .split('void PresentationBridge::openContentProfileFid(')[0];
  assert.match(open,/rememberDetailLocation\(\)/);
  const back=cpp.split('void PresentationBridge::returnFromDetail()')[1]
    .split('void PresentationBridge::loadMore()')[0];
  assert.match(back,/openCast\(previous\.castId\)/);
  assert.match(shell,/function returnFromCastDetail\(\) \{\s*presentationBridge\.returnFromDetail\(\)\s*root\.returnToPendingShellRoute\(\)/);
});

test('nested public profiles and an own profile retain independent return stacks',()=>{
  const profile=shell.slice(shell.indexOf('id: profileSurfaceLoader'),shell.indexOf('id: castSurfaceLoader'));
  assert.match(profile,/onBackRequested: root\.returnFromProfileSurface\(\)/);
  assert.match(profile,/onProfileRequested: fid => \{\s*\+\+root\.profileTopRevision\s*root\.openProfileFromContent\(fid\)/);
  const own=cpp.split('void PresentationBridge::openMyProfile()')[1]
    .split('void PresentationBridge::openProfileUsername(')[0];
  assert.match(own,/rememberDetailLocation\(\)/);
  const foreign=cpp.split('void PresentationBridge::openProfileFid(')[1]
    .split('void PresentationBridge::openContentProfileFid(')[0];
  assert.match(foreign,/rememberDetailLocation\(\)/);
  const back=cpp.split('void PresentationBridge::returnFromDetail()')[1]
    .split('void PresentationBridge::loadMore()')[0];
  assert.match(back,/previous\.ownProfile/);
  assert.match(back,/openMyProfile\(\)/);
  assert.match(back,/openProfileFid\(previous\.profileFid\)/);
});

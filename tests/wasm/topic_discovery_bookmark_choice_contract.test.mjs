import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = p => readFileSync(new URL('../../'+p,import.meta.url),'utf8');
const shell=read('ui/components/AppShell.qml');
const controller=read('ui/mobile/openproof_connections_controller.cpp');
const header=read('ui/mobile/openproof_connections_controller.hpp');
const browse=read('ui/components/TopicBrowseSurface.qml');
const picker=read('ui/components/TopicFilterPicker.qml');
const profile=read('ui/components/ProfileSurface.qml');
const chooser=read('ui/components/BookmarkCollectionChooser.qml');
const engagement=read('ui/components/EngagementBar.qml');

test('Explorer and Profile use authenticated server-indexed topic cast retrieval',()=>{
  assert.match(controller,/void OpenProofConnectionsController::selectCastTopic/);
  assert.match(controller,/authorizeCloudRequest\(request\)/);
  assert.match(controller,/\/api\/v1\/cloud\/topic_casts/);
  assert.match(controller,/QStringLiteral\("author_fid"\)/);
  assert.match(controller,/QStringLiteral\("offset"\)/);
  assert.match(header,/Q_PROPERTY\(QVariantList topicCastItems/);
  assert.match(shell,/onTopicRequested: \(slug,fid\) => root\.openTopicBrowser\(slug,"profile",fid\)/);
  assert.match(shell,/onTopicRequested: slug => root\.openTopicBrowser\(slug,"explorer",0\)/);
  assert.match(profile,/root\.topicRequested\(slug,Number\(root\.activeDetail\.fid \|\| 0\)\)/);
  assert.match(browse,/model: root\.items \|\| \[\]/);
  assert.match(browse,/onClicked: root\.castRequested\(row\.hash\)/);
});
test('topic requests are protected from stale owner and stale topic results',()=>{
  assert.match(controller,/requestGeneration!=m_topicCastsGeneration/);
  assert.match(controller,/viewer!=m_viewerFid/);
  assert.match(controller,/currentTopic!=m_activeCastTopic/);
  assert.match(controller,/m_topicCastsRequest->abort\(\)/);
  assert.match(controller,/body\.size\(\)>64\*1024/);
  assert.match(controller,/m_topicCastsHasMore=rows\.size\(\)==pageSize/);
  assert.match(controller,/clearCastTopic\(\);[\s\S]*?m_grantExpiry\.stop\(\)/);
});
test('topic picker uses catalog slugs and known translated topic labels',()=>{
  for(const slug of ['art','development','photography','design','technology','music','writing','science','business','gaming'])
    assert.ok(picker.includes('genycaster.topic.'+slug));
  assert.match(picker,/root\.topicRequested\(entry\.slug\)/);
});
test('bookmark action opens a nonblocking collection chooser only when adding',()=>{
  assert.match(engagement,/const adding = !root\.isBookmarked/);
  assert.match(engagement,/presentationBridge\.toggleBookmark\(root\.castData\)/);
  assert.match(engagement,/if \(adding\)[\s\S]*?requestBookmarkCollectionChoice/);
  assert.match(shell,/onBookmarkCollectionChoiceRequested\(hash\)/);
  assert.match(shell,/openProofConnections\.bookmarkPendingCount===0/);
  assert.match(shell,/Number\(presentationBridge\.accountFid\)!==root\.bookmarkChoiceOwner/);
  assert.match(shell,/openProofConnections\.setBookmarkCollectionMembership/);
  assert.match(chooser,/signal unfiledRequested\(\)/);
  assert.match(chooser,/signal createRequested\(string name\)/);
});

test('each new visual surface is registered in the Qt QML module',()=>{
  const cmake=read('ui/CMakeLists.txt');
  for(const component of ['BookmarkCollectionChooser','TopicBrowseSurface','TopicFilterPicker'])
    assert.ok(cmake.includes('components/'+component+'.qml'));
});

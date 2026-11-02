import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = f => readFileSync(new URL('../../'+f,import.meta.url),'utf8');
test('composer keeps topic selection separate from protocol parent and channel',()=>{
 const q=read('ui/components/ComposerDrawer.qml');
 assert.match(q,/property var selectedTopics: \[\]/);
 assert.match(q,/readonly property int maxCastTopics: 3/);
 assert.match(q,/objectName: "composerTopicDropdown"/);
 assert.match(q,/objectName: "composerTopicSearch"/);
 assert.match(q,/model: openProofConnections \\? openProofConnections.topicCatalog : \\[\\]/);
 assert.match(q,/onClicked: root.toggleTopic\(topicChip.slug\)/);
 assert.match(q,/root\.selectedTopics = \[\]/);
 assert.match(q,/onClicked: topicPicker.open\(\)/);
 assert.match(q,/selectedParentUrl/);
 assert.doesNotMatch(q,/publishPreparedCast\([^\n]+selectedTopics/);
});
test('client saves topics only after the protocol confirms a cast hash',()=>{
 const shell=read('ui/components/AppShell.qml');
 const controller=read('ui/mobile/openproof_connections_controller.cpp');
 assert.match(shell,/presentationBridge\.writePhase === "confirmed"[\s\S]*?openProofConnections\.saveCastTopics\(castId,root.pendingCastTopics\)/);
 assert.match(shell,/root.pendingTopicsOwner === Number\(presentationBridge.accountFid\)/);
 assert.match(controller,/void OpenProofConnectionsController::pumpTopicWrites/);
 assert.match(controller,/topicOutboxKey\(\)/);
 assert.match(controller,/\/api\/v1\/cloud\/cast_topics/);
});
test('all built-in topic labels are localized without schema translation tables',()=>{
 const fa=read('ui/i18n/qml_fa_IR.ts');
 const composer=read('ui/components/ComposerDrawer.qml');
 for(const slug of ['art','development','photography','design','technology','music','writing','science','business','gaming']) {
  assert.ok(fa.includes('id="genycaster.topic.'+slug+'"'),slug);
  assert.ok(composer.includes('qsTrId("genycaster.topic.'+slug+'")'),slug);
 }
 const sql=read('services/account-sync/migrations/006_cast_topics.sql');
 assert.doesNotMatch(sql,/CREATE TABLE IF NOT EXISTS topic_translations|\bchannel_id\s+(text|varchar)|\bparent_url\s+(text|varchar)/i);
});

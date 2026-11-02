import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=f=>readFileSync(new URL('../../'+f,import.meta.url),'utf8');

test('composer topic chooser is dynamic, searchable, capped, and separate from Farcaster channels',()=>{
 const q=read('ui/components/ComposerDrawer.qml');
 assert.match(q,/objectName: "composerTopicDropdown"/);
 assert.match(q,/model: openProofConnections \? openProofConnections\.topicCatalog : \[\]/);
 assert.match(q,/id: topicSearch/);
 assert.match(q,/root.selectedTopics.length < root.maxCastTopics/);
 assert.match(q,/readonly property int maxCastTopics: 3/);
 assert.match(q,/onClicked: topicPicker.open\(\)/);
 assert.match(q,/objectName: "desktopCastTopics"/);
 assert.match(q,/property string selectedParentUrl:/);
 assert.match(q,/property var selectedTopics:/);
 assert.match(q,/openProofConnections.fetchTopics\(\)/);
});
test('server-backed topics write only after a confirmed Farcaster cast hash',()=>{
 const shell=read('ui/components/AppShell.qml');
 assert.match(shell,/root.pendingCastTopics = composerDrawer.selectedTopics.slice\(\)/);
 assert.match(shell,/presentationBridge.writePhase === "confirmed"[\s\S]*?openProofConnections.saveCastTopics\(castId,root.pendingCastTopics\)/);
 assert.match(shell,/presentationBridge.writePhase === "failed"/);
 const cpp=read('ui/mobile/openproof_connections_controller.cpp');
 assert.match(cpp,/QStringLiteral\("\/api\/v1\/cloud\/topics"\)/);
 assert.match(cpp,/QStringLiteral\("\/api\/v1\/cloud\/cast_topics"\)/);
 assert.match(cpp,/QNetworkRequest::ManualRedirectPolicy/);
 assert.match(cpp,/QJsonArray::fromStringList/);
 assert.match(cpp,/topicOutboxKey\(\)/);
 assert.match(cpp,/m_settings->sync\(\)/);
});
test('topic labels have explicit localized names for Persian and all supported locales',()=>{
 const q=read('ui/components/ComposerDrawer.qml');
 const keys=['art','development','photography','design','technology','music',
 'writing','science','business','gaming'];
 for(const slug of keys)assert.ok(q.includes('genycaster.topic.'+slug),slug);
 for(const locale of ['fa_IR','en_US','ar_SA','de_DE','fr_FR','es_ES','hi_IN',
    'ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
   const ts=read('ui/i18n/qml_'+locale+'.ts');
   for(const slug of keys) assert.ok(ts.includes('id="genycaster.topic.'+slug+'"'),locale+':'+slug);
 }
 const fa=read('ui/i18n/qml_fa_IR.ts');
 assert.match(fa,/<source>Art<\/source>\s*<translation>هنر<\/translation>/);
 assert.match(fa,/<source>Photography<\/source>\s*<translation>عکاسی<\/translation>/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const qml=path=>read('ui/components/'+path+'.qml');

test('phone form dialogs are genuinely full screen with no bogus drag handle',()=>{
  const adaptive=qml('GenyAdaptiveDialog');
  assert.match(adaptive,/property bool mobileFullScreen: false/);
  // Short confirmations should keep their contextual sheet treatment.
  const shell=qml('AppShell');
  assert.match(shell,/GenyAdaptiveDialog \{[\s\S]*?id: signOutConfirm/);
  assert.match(qml('RenderingSettingsPopup'),/mobileFullScreen: true/);
  assert.match(qml('AccentColorStudio'),/mobileFullScreen: true/);
  assert.match(adaptive,/fullScreen: control.mobileFullScreen/);
  assert.match(adaptive,/interactive: !control.mobileFullScreen/);
  assert.match(adaptive,/showHandle: !control.mobileFullScreen && control.showHandle/);
  assert.match(adaptive,/height: control.mobileFullScreen[\s\S]*?parent\.height/);
  assert.match(qml('FeedbackDialog'),/mobileFullScreen: true/);
});
test('mobile feedback action sits by bell and opens real full-screen form',()=>{
  const header=qml('AppHeader');
  const shell=qml('AppShell');
  assert.match(header,/signal feedbackRequested\(\)/);
  assert.match(header,/visible: root.compact[\s\S]*?iconGlyph: Theme.iconFeedback[\s\S]*?onClicked: root.feedbackRequested\(\)/);
  assert.match(shell,/onFeedbackRequested: feedbackDialog.open\(\)/);
  assert.match(qml('FeedbackDialog'),/objectName: "feedbackFixedActions"/);
  assert.match(qml('FeedbackDialog'),/objectName: "feedbackFormScroll"/);
});
test('feedback form performs authenticated server submission and tracks real status',()=>{
  const ui=qml('FeedbackDialog');
  const controller=read('ui/mobile/openproof_connections_controller.cpp');
  const header=read('ui/mobile/openproof_connections_controller.hpp');
  assert.match(ui,/openProofConnections\.submitFeedback\(root\.category, root\.sentiment/);
  assert.match(ui,/openProofConnections\.fetchFeedback\(\)/);
  assert.match(ui,/openProofConnections\.feedbackStats\.accepted/);
  assert.match(ui,/openProofConnections\.feedbackStats\.resolved/);
  assert.doesNotMatch(ui,/backend_pending|copyText\(/);
  assert.match(header,/Q_INVOKABLE void submitFeedback/);
  assert.match(controller,/m_network\.post\(request, QJsonDocument\{payload\}/);
  assert.match(controller,/m_feedbackSubmitted = status == 201/);
  assert.match(controller,/if \(viewer != m_viewerFid \|\| generation != m_grantGeneration\) return;/);
  assert.match(controller,/reply->isReadable\(\)/);
});
test('feedback text is translated in every supported locale',()=>{
  const dialog=qml('FeedbackDialog');
  const ids=[...new Set([...dialog.matchAll(/qsTrId\("([^"]+)"\)/g)].map(m=>m[1]))];
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                        'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']){
    const ts=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of ids)
      assert.ok(ts.includes('message id="'+id+'"'),locale+' missing '+id);
  }
  const catalog=read('ui/i18n/catalog.cpp');
  for(const id of ids)
    assert.ok(catalog.includes('qtTrId("'+id+'")'),'missing catalog ID: '+id);
});

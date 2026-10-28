import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const root=process.cwd();
const qml=readFileSync(root+'/ui/components/ProfileSurface.qml','utf8');
const activity=readFileSync(root+'/ui/components/GenyPulsePanel.qml','utf8');
const controller=readFileSync(root+'/ui/mobile/openproof_connections_controller.cpp','utf8');
const header=readFileSync(root+'/ui/mobile/openproof_connections_controller.hpp','utf8');
test('own mobile profile alone omits redundant back-to-feed toolbar',()=>{
  assert.match(qml,/visible: !\(root\.compact && root\.isOwnProfile\)/);
  assert.match(qml,/text: qsTrId\("genycaster\.profile\.back_back_to_feed"\)/);
});
test('professional label matches compact business identity style',()=>{
  assert.match(qml,/objectName: "profileProfessionalCategory"/);
  assert.match(qml,/text: Theme\.iconBusiness/);
  assert.match(qml,/Layout\.topMargin: Theme\.space1/);
  assert.match(qml,/Layout\.alignment: Qt\.AlignVCenter/);
});
test('activity explore button is background-free rather than a third filled CTA',()=>{
  const start=activity.indexOf('objectName: "genyPulseExpandButton"');
  assert.ok(start>=0);
  assert.match(activity.slice(start,start+250),/quiet: true/);
  assert.match(activity.slice(start,start+250),/elevated: false/);
});
test('status editor gives clear, private and user-defined modes with authenticated writes',()=>{
  assert.match(qml,/objectName: "profileStatusEditor"/);
  assert.match(qml,/objectName: "profileStatusSetting"/);
  assert.match(qml,/objectName: "profileMoodSummary"/);
  for(const name of ['available','busy','idle','away','focusing','creating','dnd','custom','none'])
    assert.ok(qml.includes("id:'"+name+"'"),name);
  assert.match(qml,/moodLabel/);
  assert.match(qml,/profileStatusEditor\.publicMood/);
  assert.match(qml,/cloudAuthorizationReady/);
  assert.match(qml,/saveProfileStatus/);
  assert.match(header,/Q_PROPERTY\(QVariantMap profileStatus/);
  assert.match(controller,/authorizeCloudRequest\(request\)/);
  assert.match(controller,/api\/v1\/cloud\/profile_status/);
});
test('status choices are localized in every bundled UI language',()=>{
  const ids=['set','hint','available','busy','idle','away','focusing','creating',
   'dnd','custom','none','note','placeholder','visible','save','edit'];
  for(const lang of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'pt_BR','ru_RU','hi_IN','ja_JP','ko_KR','zh_CN']){
    const xml=readFileSync(root+'/ui/i18n/qml_'+lang+'.ts','utf8');
    for(const id of ids)
      assert.ok(xml.includes('id="genycaster.status.'+id+'"'),lang+' '+id);
  }
});

test('own profile status row is the only inline edit affordance',()=>{
  const start=qml.indexOf('objectName: "profileMoodSummary"');
  const end=qml.indexOf('objectName: "profileSaveStatus"',start);
  assert.ok(start>=0 && end>start);
  const row=qml.slice(start,end);
  assert.match(row,/TapHandler \{\s*enabled: root\.isOwnProfile\s*onTapped: profileStatusEditor\.open\(\)/);
  assert.match(row,/Accessible\.role: root\.isOwnProfile \? Accessible\.Button/);
  assert.match(row,/Keys\.onReturnPressed: if \(root\.isOwnProfile\)/);
  assert.doesNotMatch(row,/ActionButton \{/);
});
test('status note remains reachable in a resizing mobile sheet with pinned save',()=>{
  const start=qml.indexOf('id: profileStatusEditor');
  const end=qml.indexOf('id: profileLinksDrawer',start);
  const editor=qml.slice(start,end);
  assert.match(editor,/Math\.min\(parent\.height - Theme\.space2, 710\)/);
  assert.match(editor,/function revealNoteEditor\(\)/);
  assert.match(editor,/id: statusFormScroll/);
  assert.match(editor,/onHeightChanged: profileStatusEditor\.revealNoteEditor\(\)/);
  assert.match(editor,/onActiveFocusChanged: if \(activeFocus\)\s*profileStatusEditor\.revealNoteEditor\(\)/);
  assert.ok(editor.indexOf('id: statusFormScroll') < editor.indexOf('objectName: "profileStatusSave"'));
});
test('iOS visual viewport panning keeps the Qt canvas above the keyboard',()=>{
  const loader=readFileSync(root+'/ui/wasm/genycaster-loader.js','utf8');
  const definition=loader.match(/function fit\(\) \{[\s\S]*?\n\}/);
  assert.ok(definition,'viewport fitting function');
  assert.match(loader,/window\.visualViewport\?\.addEventListener\("scroll",fit\)/);
  const context={
    window:{visualViewport:{height:420,pageTop:180,pageLeft:0},
      innerHeight:800,scrollY:0,scrollX:0},
    screen:{style:{}},Math
  };
  const fit=vm.runInNewContext('('+definition[0]+')',context);
  fit();
  assert.equal(context.screen.style.height,'420px');
  assert.equal(context.screen.style.top,'180px');
  context.window.visualViewport.height=780;
  context.window.visualViewport.pageTop=0;
  fit();
  assert.equal(context.screen.style.height,'780px');
  assert.equal(context.screen.style.top,'0px');
  context.window.visualViewport=null;
  context.window.scrollY=24;
  fit();
  assert.equal(context.screen.style.top,'24px');
});

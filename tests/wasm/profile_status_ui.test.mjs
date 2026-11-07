import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const root=process.cwd();
const qml=readFileSync(root+'/ui/components/ProfileSurface.qml','utf8');
const activity=readFileSync(root+'/ui/components/GenyPulsePanel.qml','utf8');
const controller=readFileSync(root+'/ui/app/openproof/connections.cpp','utf8');
const header=readFileSync(root+'/ui/app/openproof/connections.hpp','utf8');
test('own mobile profile alone omits redundant back-to-feed toolbar',()=>{
  assert.match(qml,/visible: !\(root\.compact && root\.isOwnProfile\)/);
  assert.match(qml,/text: qsTrId\("genycaster\.setup\.back"\)/);
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
  assert.match(editor,/height: parent \? parent\.height : 690/);
  assert.match(editor,/fullScreen: mobilePresentation/);
  assert.match(editor,/showHandle: false/);
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

test('status save closes only on server-confirmed completion and preserves conflict errors',()=>{
  const editor=qml.slice(qml.indexOf('id: profileStatusEditor'),qml.indexOf('id: profileLinksDrawer'));
  assert.match(header,/void profileStatusSaveFinished\(bool saved, const QString& message\)/);
  assert.match(controller,/emit profileStatusSaveFinished\(false,m_profileStatusError\)/);
  assert.match(controller,/emit profileStatusSaveFinished\(saved,m_profileStatusError\)/);
  assert.match(editor,/function onProfileStatusSaveFinished\(saved,message\)/);
  assert.match(editor,/if\(saved\) \{[\s\S]*?profileStatusEditor\.close\(\)/);
  assert.match(editor,/profileStatusEditor\.saveError=message/);
  assert.match(editor,/objectName: "profileStatusSaveError"/);
  assert.doesNotMatch(editor,/function onProfileStatusChanged\(\) \{[\s\S]*?if\(profileStatusEditor\.awaitingSave\s*&& !openProofConnections\.profileStatusBusy\)/);
  assert.match(controller,/if\(!saved && code==409\)/);
  assert.match(controller,/if\(m_profileStatusFid!=profileFid\) \{\s*m_profileStatus\.clear/);
  assert.match(controller,/\|\| m_profileStatusWrite\) return/);
});
test('profile summary and picker carry semantic icon/color for each mood',()=>{
  const row=qml.slice(qml.indexOf('objectName: "profileMoodSummary"'),qml.indexOf('objectName: "profileSaveStatus"'));
  const picker=qml.slice(qml.indexOf('id: profileStatusEditor'),qml.indexOf('id: profileLinksDrawer'));
  for(const mood of ['available','busy','idle','away','focusing','creating','dnd','custom','none'])
    assert.match(qml,new RegExp("id:'"+mood+"',icon:Theme\\.icon[A-Za-z]+,color:Theme\\.[A-Za-z]+"));
  assert.match(row,/root\.moodAppearance\(root\.displayedMood\.mood\)\.icon/);
  assert.match(row,/root\.moodAppearance\(root\.displayedMood\.mood\)\.color/);
  assert.match(picker,/customSelectedColor: modelData\.color/);
  assert.match(picker,/customSelectedSurface: Qt\.rgba/);
  const button=readFileSync(root+'/ui/components/ActionButton.qml','utf8');
  assert.match(button,/customSelectedColor/);
  assert.match(button,/customSelectedSurface/);
});
test('periodic status fetch never races an open editor or pending save',()=>{
  const timer=qml.slice(qml.indexOf('id: profileStatusFetch'),qml.indexOf('interval: 60000'));
  assert.match(timer,/profileStatusEditor.visible \|\| profileStatusEditor.awaitingSave/);
  assert.match(controller,/if\(m_profileStatusRead!=reply\) \{/);
  assert.match(controller,/if\s*\(!profileFid \|\| !trustedHttpsEndpoint\(m_cloudUrl\) \|\| m_profileStatusWrite\) return/);
});

test('status icon family is semantic, bundled and independent of audio controls',()=>{
  const theme=readFileSync(root+'/ui/theme/Theme.qml','utf8');
  const mappings={
    available:'iconStatusAvailable',busy:'iconStatusBusy',idle:'iconStatusIdle',
    away:'iconStatusAway',focusing:'iconStatusFocused',
    creating:'iconStatusCreating',dnd:'iconStatusDoNotDisturb',
    custom:'iconStatusCustom',none:'iconStatusClear'
  };
  for(const [status,semanticName] of Object.entries(mappings)){
    assert.match(theme,new RegExp('readonly property string '+semanticName+
      ': "status/'+status+'"'));
    assert.match(qml,new RegExp("id:'"+status+"',icon:Theme\\."+semanticName+",color:Theme\\."));
    const svg=readFileSync(root+'/ui/assets/icons/genyleap/icons/status/'+status+'.svg','utf8');
    assert.match(svg,/<svg [^>]*viewBox="0 0 24 24"/);
    assert.match(svg,/stroke="currentColor"/);
    assert.match(svg,/<\/svg>/);
    assert.doesNotMatch(svg,/<(?:text|image|script)\b/i);
  }
  const dnd=readFileSync(root+'/ui/assets/icons/genyleap/icons/status/dnd.svg','utf8');
  assert.match(dnd,/<circle[^>]*r="9"/);
  assert.match(dnd,/<path[^>]*18\.36 5\.64/);
  assert.doesNotMatch(qml,/id:'dnd',icon:Theme\.iconVolumeMuted/);
  const away=readFileSync(root+'/ui/assets/icons/genyleap/icons/status/away.svg','utf8');
  const idle=readFileSync(root+'/ui/assets/icons/genyleap/icons/status/idle.svg','utf8');
  assert.notEqual(away,idle);
});

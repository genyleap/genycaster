import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(p,'utf8');
const setup=read('ui/components/setup/InitialSetup.qml');
test('welcome links open the in-app legal reader without recording consent',()=>{
 const note=setup.slice(setup.indexOf('id: welcomeLegalNote'),setup.indexOf('id: welcomeStage'));
 assert.match(note,/visible: root.currentStep === 0 && !root.reviewMode/);
 assert.match(note,/href="terms"/);
 assert.match(note,/href="privacy"/);
 assert.match(note,/onLinkActivated: link => root.openLegalDocument\(link\)/);
 assert.doesNotMatch(note,/acceptLegalConsent|consentChecked\s*=/);
 assert.match(setup,/if \(kind !== "terms" && kind !== "privacy"\)/);
 assert.match(setup,/legalNoteSpace: welcomeLegalNote.visible\s*\? welcomeLegalNote.implicitHeight \+ Theme.space2 : 0/);
 assert.match(setup,/stageAction.height\) \+ Theme.space3 \* 2 \+ setupFooterContent.legalNoteSpace/);
});
test('account routes require explicit current consent and recorded consent',()=>{
 assert.match(setup,/accountAccessAllowed: consentChecked && legalConsentAccepted/);
 assert.match(setup,/!root.accountAccessAllowed && root.pendingStep > 2/);
 assert.match(setup,/!accountAccessAllowed && target > 2/);
 const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
 for(const name of ['setupCreateAccount','setupUseExistingAccount']){
  const block=account.slice(account.indexOf('objectName: "'+name+'"'));
  assert.match(block,/enabled: root.accountAccessAllowed/);
  assert.match(block,/onClicked: \{\s*if \(!root.accountAccessAllowed\) return/);
 }
 assert.match(setup,/id: signInStage\s+ColumnLayout \{\s+enabled: root.accountAccessAllowed/);
 assert.match(setup,/function startNativeAccountCreation\(\) \{\s*if \(!accountAccessAllowed/);
 assert.match(setup,/if \(!browserReadOnly \|\| !runtimeReady \|\| !accountAccessAllowed/);
 assert.equal((account.match(/accountShowcase: !root.phone/g)||[]).length,2);
});
test('all twelve locales preserve both legal link placeholders',()=>{
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const ts=read('ui/i18n/qml_'+locale+'.ts');
  const msg=ts.match(/<message id="genycaster.setup.welcome_legal_note">[\s\S]*?<translation>([\s\S]*?)<\/translation>/)?.[1];
  assert.ok(msg,locale);
  assert.equal((msg.match(/%1/g)||[]).length,1,locale);
  assert.equal((msg.match(/%2/g)||[]).length,1,locale);
 }
});

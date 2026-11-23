import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path,'utf8');
const setup=read('ui/components/setup/InitialSetup.qml');
const card=read('ui/components/setup/SetupActionCard.qml');
const check=read('ui/components/controls/GenyCheckRow.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');
const bridge=read('src/presentation/qt/presentation_bridge.cpp');
const qml=read('ui/components/shell/AppShell.qml');
const gallery=read('ui/components/media/GalleryLayout.qml');

test('recovery phrase submission enables the signer without a redundant checkbox',()=>{
  assert.doesNotMatch(setup,/id: deviceSignerConsent|nativeRecoveryDeviceSignerConsent/);
  assert.match(setup,/objectName: "nativeRecoveryAutomaticSignerNotice"/);
  assert.match(setup,/genycaster\.setup\.recovery_automatic_signer_notice/);
  const submit=setup.slice(setup.indexOf('let words = nativeRecoveryGrid.phrase()'),
    setup.indexOf('root.switchStep(5)',setup.indexOf('let words = nativeRecoveryGrid.phrase()')));
  assert.match(submit,/const authorizeDeviceSigner = true/);
  assert.match(submit,/nativeRecoverFarcasterAccount\(words, authorizeDeviceSigner\)/);
  assert.match(submit,/nativeRecoverHyperSnapAccount\(words, authorizeDeviceSigner\)/);
  assert.match(setup,/objectName: "setupExistingRecoveryPhrase"\s+visible: !root\.showRecovery/);
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
    const ts=read('ui/i18n/qml_'+locale+'.ts');
    assert.match(ts,/<message id="genycaster\.setup\.recovery_automatic_signer_notice">/);
  }
});

test('legal acknowledgment always starts empty and requires an explicit user click',()=>{
  assert.match(setup,/property bool consentChecked: false/);
  assert.match(setup,/onVisibleChanged: \{\s*if \(visible\)\s*consentChecked = false/);
  assert.match(setup,/showRecovery = false\s*[\s\S]*?consentChecked = false/);
  assert.doesNotMatch(setup,/consentChecked = legalConsentAccepted/);
  assert.match(setup,/if \(!reviewMode && !consentChecked && target > 2\)/);
  assert.match(setup,/if \(currentStep === 2\) return consentChecked/);
  assert.match(setup,/if \(!consentChecked\) return/);
  assert.match(setup,/Accessible\.role: Accessible\.CheckBox/);
  assert.match(setup,/Accessible\.checked: root\.consentChecked/);
  assert.match(setup,/onClicked: root\.consentChecked = !root\.consentChecked/);
});

test('onboarding cards and checkbox descriptions never show selection Copy overlay',()=>{
  assert.ok((card.match(/selectableText: false/g)||[]).length>=4);
  assert.match(check,/selectableText: false/);
  assert.match(setup,/id: consentLabel[\s\S]*?selectableText: false/);
});

test('profile avatar always supplies a QString when identity data is missing',()=>{
  const part=profile.slice(profile.indexOf('id: profileAvatar'),
    profile.indexOf('TapHandler {\n                        parent: profileAvatar'));
  assert.match(part,/displayName: String\(root\.activeDetail\.displayName/);
  assert.match(part,/\|\| ""\)\s*$/m);
  assert.match(bridge,/sameProtocol \|\| fromOwnNotifications/);
  assert.match(qml,/root\.pendingReturnShellRoute === "notifications"/);
  assert.match(gallery,/if \(tileLoader && tileLoader\.status === Loader\.Loading\)\s*tileLoader\.active = false/);
});

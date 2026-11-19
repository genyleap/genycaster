import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const setup=readFileSync(new URL('../../ui/components/setup/InitialSetup.qml',import.meta.url),'utf8');
const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
const existing=setup.slice(setup.indexOf('id: signInStage'),setup.indexOf('id: verifyingStage'));

test('both desktop and browser begin with exactly New / Existing before protocol selection',()=>{
  const choice=account.slice(account.indexOf('objectName: "setupCreateAccount"')-150,
                             account.indexOf('visible: root.choosingRegistrationProvider'));
  assert.match(choice,/objectName: "setupCreateAccount"/);
  assert.match(account,/objectName: "setupCreateAccount"/);
  assert.match(account,/genycaster.setup.design_existing_account/);
  assert.equal((account.match(/objectName: "setupRegisterHyperSnap"/g)||[]).length,1);
  assert.equal((account.match(/objectName: "setupRegisterNeynar"/g)||[]).length,1);
  assert.doesNotMatch(account,/setupBrowserAccountProtocols|setupBrowserHyperSnapChoice/);
  assert.match(account,/root.accountIntent = "new"[\s\S]*?root.choosingRegistrationProvider = true/);
  assert.match(account,/root.accountIntent = "existing"[\s\S]*?root.existingProtocolSelected = false[\s\S]*?root.switchStep\(4\)/);
});

test('Existing protocol picker renders once and gates every subsequent action',()=>{
  assert.equal((existing.match(/objectName: "setupExistingProtocolChoice"/g)||[]).length,1);
  assert.equal((existing.match(/objectName: "setupExistingFarcaster"/g)||[]).length,1);
  assert.equal((existing.match(/objectName: "setupExistingHyperSnap"/g)||[]).length,1);
  assert.doesNotMatch(existing,/setupBrowserRecoveryProtocolChoice|setupRecoverHyperSnap|setupRecoverFarcaster/);
  assert.match(existing,/visible: !root.authenticated && !root.existingProtocolSelected\s*&& !root.resumingRegistration/);
  assert.match(existing,/root.recoveryProtocol = "farcaster-snapchain"[\s\S]*?root.existingProtocolSelected = true/);
  assert.match(existing,/root.recoveryProtocol = "hypersnap-v2"[\s\S]*?root.existingProtocolSelected = true/);
  assert.match(existing,/visible: root.existingProtocolSelected && !root.authenticated/);
  assert.match(existing,/visible: !root.authenticated && root.existingProtocolSelected/);
  assert.match(existing,/visible: root.recoveryProtocol === "hypersnap-v2"/);
  assert.match(existing,/visible: root.recoveryProtocol === "farcaster-snapchain"/);
});

test('Back returns from phrase editor to protocol choice then New/Existing',()=>{
  const back=setup.slice(setup.indexOf('function previousStep()'),setup.indexOf('function actionText()'));
  assert.match(back,/if \(currentStep === 4 && showRecovery\)[\s\S]*?showRecovery = false/);
  assert.match(back,/if \(currentStep === 4 && existingProtocolSelected\)[\s\S]*?existingProtocolSelected = false/);
  assert.match(back,/if \(currentStep === 3 && choosingRegistrationProvider\)/);
  assert.match(back,/if \(currentStep === 4 && resumingRegistration\)[\s\S]*?switchStep\(3\)/);
  assert.match(existing,/root.showRecovery && \(root.existingProtocolSelected \|\| root.resumingRegistration\)/);
});

test('browser registration prevents dead stub wallet while offering verified identity providers',()=>{
  assert.match(account,/objectName: "setupBrowserRegistrationPath"/);
  assert.match(account,/visible: root.browserReadOnly && root.registrationProviderId.length > 0/);
  assert.match(account,/objectName: "setupBrowserExternalHyperSnap"/);
  assert.match(account,/objectName: "setupBrowserExternalFarcaster"/);
  assert.match(account,/objectName: "setupBrowserExternalHyperSnap"[\s\S]*?root.accountIntent = "existing"[\s\S]*?root.continueWithGenyleap\(\)/);
  assert.match(account,/objectName: "setupBrowserExternalFarcaster"[\s\S]*?root.accountIntent = "existing"[\s\S]*?root.continueWithFarcaster\(\)/);
  assert.match(account,/visible: !root.browserReadOnly && root.registrationProviderId.length > 0/);
  for (const name of ['nativeAccountLookupPanel','nativeRegistrationProofPanel','nativeRegistrationBroadcastPanel','neynarFarcasterRegistration']) {
    assert.match(account,new RegExp('objectName: "'+name+'"[^]*?visible: !root.browserReadOnly'));
  }
  assert.match(existing,/visible: root.existingProtocolSelected && !root.authenticated\s*&& \(!root.browserReadOnly \|\| root.recoveryProtocol === "farcaster-snapchain"\)/);
  assert.match(existing,/genycaster.setup.browser_hypersnap_recovery_notice/);
  assert.match(account,/genycaster.setup.browser_native_registration_notice/);
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translation=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts',import.meta.url),'utf8');
    assert.ok(translation.includes('id="genycaster.setup.browser_native_registration_notice"'),locale);
    assert.ok(translation.includes('id="genycaster.setup.browser_hypersnap_recovery_notice"'),locale);
  }
});

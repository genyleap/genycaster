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

test('shared setup shows advisory recovery and sign-in checks on both account paths', () => {
  assert.match(account, /SetupConnectionPanel \{\s*objectName: "setupRegistrationConnections"/);
  assert.match(existing, /SetupConnectionPanel \{\s*objectName: "setupExistingConnections"/);
  assert.match(setup, /onShowsSetupConnectivityChanged:[^]*?checkSetupConnections\(\)/);
  for (const stage of [account, existing]) {
    assert.match(stage, /connectionState: root.protocolConnectionState\(false\)/);
    assert.match(stage, /connectionState: root.protocolConnectionState\(true\)/);
  }
  assert.match(existing, /protocol: root.existingProtocolSelected \? root.recoveryProtocol : ""/);
  for (const service of ['hypersnapRecovery', 'farcasterRecovery', 'farcasterSignIn']) {
    assert.ok(setup.includes('root.setupConnections.' + service), service);
  }
  assert.match(existing, /enabled: !root.authenticationInProgress && !root.recoverySubmitPending/);
  assert.doesNotMatch(account + existing, /enabled:[^\n]*(setupConnections|showsSetupConnectivity)/);
  const bindings = readFileSync(new URL('../../ui/app/runtime/bindings.cpp', import.meta.url), 'utf8');
  assert.match(bindings, /probeAddress\{"0x0+1"\}/);
  assert.match(bindings, /"\/v1\/nativeAccount" \+ custodyQuery/);
  assert.match(bindings, /"\/v1\/browser\/hypersnap\/native-account" \+ custodyQuery/);
  assert.match(bindings, /"eth_call"/);
  assert.match(bindings, /"challenge_providers"/);
  assert.match(bindings, /"\/v1\/browser\/openproof\/providers"/);
  const signer = readFileSync(new URL('../../ui/wasm/browser_protocol_signer.mjs', import.meta.url), 'utf8');
  const rpcUrl = signer.match(/const optimismRpcUrl = '([^']+)'/)?.[1];
  assert.ok(rpcUrl && bindings.includes('optimismRpc{"' + rpcUrl + '"}'));
});

test('connection details start collapsed and a manual check stays available when expanded', () => {
  const panel = setup.slice(setup.indexOf('component SetupConnectionPanel:'), setup.indexOf('function greetingIndexForTag'));
  assert.match(panel, /property bool expanded: false/);
  assert.match(panel, /onClicked: details.expanded = !details.expanded/);
  assert.match(panel, /model: details.expanded/);
  assert.match(panel, /SetupConnectionRefresh \{ visible: details.expanded \}/);
});

test('browser registration prevents dead stub wallet while offering verified identity providers',()=>{
  assert.match(account,/objectName: "setupBrowserRegistrationPath"/);
  assert.match(account,/visible: root.browserReadOnly && root.registrationProviderId.length > 0/);
  assert.match(account,/objectName: "setupBrowserExternalHyperSnap"/);
  assert.match(account,/objectName: "setupBrowserExternalFarcaster"/);
  assert.match(account,/objectName: "setupBrowserExternalHyperSnap"[\s\S]*?root.accountIntent = "existing"[\s\S]*?root.continueWithGenyleap\(\)/);
  assert.match(account,/objectName: "setupBrowserExternalFarcaster"[\s\S]*?root.accountIntent = "existing"[\s\S]*?root.continueWithFarcaster\(\)/);
  assert.match(account,/visible: !root.browserReadOnly && root.registrationProviderId.length > 0/);
  for (const name of ['nativeRegistrationBroadcastPanel','setupNativeAwaitingConfirmation','setupNativeTechnicalDetailsToggle','neynarFarcasterRegistration']) {
    assert.match(account,new RegExp('objectName: "'+name+'"'));
  }
  assert.match(existing,/visible: !root.authenticated && root.existingProtocolSelected/);
  assert.match(account,/objectName: "setupBrowserCreateHyperSnap"/);
  assert.match(account,/objectName: "setupBrowserCreateFarcaster"/);
  assert.match(account,/root.browserCreateAccount\("hypersnap-v2"\)/);
  assert.match(account,/root.browserCreateAccount\("farcaster-snapchain"\)/);
  assert.match(existing,/root.recoverAccount\(root.recoveryProtocol\)/);
  assert.doesNotMatch(existing,/objectName: "setupBrowserHyperSnapRecoveryNotice"/);
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translation=readFileSync(new URL('../../ui/i18n/qml_'+locale+'.ts',import.meta.url),'utf8');
    assert.ok(translation.includes('id="genycaster.setup.browser_native_registration_notice"'),locale);
    assert.ok(translation.includes('id="genycaster.setup.browser_hypersnap_recovery_notice"'),locale);
  }
});

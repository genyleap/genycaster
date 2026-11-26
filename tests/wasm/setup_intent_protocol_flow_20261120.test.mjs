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
  assert.match(existing,/visible: !root.showRecovery && !root.authenticated\s*&& root.existingProtocolSelected/);
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
  assert.match(account, /connectionState: nativeRegistrationStatus.shardZeroBlockDelay > 120/);
  assert.match(existing, /connectionState: root.protocolConnectionState\(false\)/);
  for (const stage of [account, existing]) {
    assert.match(stage, /connectionState: root.protocolConnectionState\(true\)/);
  }
  assert.match(existing, /protocol: root.existingProtocolSelected \? root.recoveryProtocol : ""/);
  for (const service of ['hypersnapRecovery', 'farcasterRecovery', 'farcasterSignIn']) {
    assert.ok(setup.includes('setupConnections.' + service), service);
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
  assert.match(panel, /onClicked:\s*\{\s*details.expanded = !details.expanded\s*if \(details.expanded\) providerProfiles.refreshDirectory\(\)/);
  assert.match(panel, /model: details.expanded/);
  assert.match(panel, /SetupConnectionRefresh \{ visible: true \}/);
  assert.match(panel, /readonly property int latency: root\.protocolLatencyMs\(farcaster\)/);
  assert.match(panel, /route\.statusColor/);
  assert.match(setup, /function protocolLatencyMs\(farcaster\)/);
  for(const card of ['setupRegisterHyperSnap','setupRegisterNeynar','setupExistingFarcaster','setupExistingHyperSnap']){
    const start=setup.indexOf('objectName: "'+card+'"');
    assert.ok(start>=0,card);
    assert.match(setup.slice(start,start+1200),/latencyMs: root\.protocolLatencyMs\((?:false|true)\)/,card);
  }
});

test('browser registration uses the shared QML wallet and provider flow',()=>{
  assert.doesNotMatch(account,/setupBrowserRegistrationPath|setupBrowserCreateHyperSnap|setupBrowserCreateFarcaster/);
  assert.match(account,/nativeCustodyWallet.beginBrowserWallet\(root.registrationProviderId, ""\)/);
  assert.match(account,/objectName: "setupGenerateCustodyWallet"/);
  assert.match(account,/objectName: "nativeRecoveryWordsDisplay"/);
  for(const name of ['nativeRegistrationBroadcastPanel','setupNativeAwaitingConfirmation','setupNativeTechnicalDetailsToggle','neynarFarcasterRegistration']){
    assert.match(account,new RegExp('objectName: "'+name+'"'));
  }
  assert.match(existing,/nativeCustodyWallet.stageBrowserRecovery\(root.recoveryProtocol, words\)/);
  assert.match(existing,/root.recoverAccount\(root.recoveryProtocol\)/);
  assert.doesNotMatch(existing,/objectName: "setupBrowserHyperSnapRecoveryNotice"/);
});

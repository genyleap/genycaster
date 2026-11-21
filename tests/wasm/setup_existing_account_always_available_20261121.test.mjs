import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const setup=readFileSync(new URL('../../ui/components/setup/InitialSetup.qml',import.meta.url),'utf8');
const create=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
const existing=setup.slice(setup.indexOf('id: signInStage'),setup.indexOf('id: verifyingStage'));
const button=create.slice(create.indexOf('title: qsTrId("genycaster.setup.design_existing_account")'),
                          create.indexOf('root.switchStep(4)',create.indexOf('title: qsTrId("genycaster.setup.design_existing_account")')));

test('Use existing remains selectable while account registration or sync is pending',()=>{
  assert.match(button,/enabled: true/);
  assert.doesNotMatch(button,/enabled:\s*[^\n]*(nativeRegistrationPublisher\.pending|neynarRegistration\.pending|syncedForRegistration|registrationNodeUsable|showsSetupConnectivity)/);
  assert.match(button,/root\.accountIntent = "existing"/);
  assert.match(create,/root\.switchStep\(4\)/);
});

test('Existing sign in does not discard custody state or reset pending registration',()=>{
  assert.match(button,/if \(!nativeRegistrationPublisher\.pending\s*&& !neynarRegistration\.pending\s*&& neynarRegistration\.phase !== "reserved"\)/);
  const guard=button.slice(button.indexOf('if (!nativeRegistrationPublisher.pending'),button.indexOf('root.accountIntent = "existing"'));
  assert.match(guard,/neynarRegistration\.reset\(\)/);
  assert.match(guard,/nativeRegistrationPublisher\.clear\(\)/);
  assert.match(guard,/nativeCustodyWallet\.discard\(\)/);
  assert.match(guard,/nativeRegistrationStatus\.clearExistingLookup\(\)/);
});
test('Recovery choices are not disabled by network sync lag or node ping',()=>{
  const picker=existing.slice(existing.indexOf('objectName: "setupExistingProtocolChoice"'),existing.indexOf('objectName: "setupExistingConnections"'));
  for(const id of ['setupExistingFarcaster','setupExistingHyperSnap']) {
    const start=picker.indexOf('objectName: "'+id+'"');
    assert.ok(start>0,id);
    const end=picker.indexOf('onClicked:',start);
    assert.doesNotMatch(picker.slice(start,end),/enabled:[^\n]*(unavailable|syncedForRegistration|showsSetupConnectivity|registrationNodeUsable)/);
  }
  assert.match(setup,/connectionState: nativeRegistrationStatus\.shardZeroBlockDelay > 120/);
  assert.match(existing,/objectName: "setupExistingHyperSnap"/);
  assert.match(create,/objectName: "setupFarcasterExistingAccount"[\s\S]*?visible: true/);
});

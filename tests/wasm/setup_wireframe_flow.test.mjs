import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = path => readFileSync(path, 'utf8');
const setup = source('ui/components/InitialSetup.qml');
const shell = source('ui/components/AppShell.qml');

test('approved wireframe has eight distinct, ordered stages', () => {
  assert.match(setup, /wizardSteps: \[0,1,2,3,4,5,6,7\]/);
  const ids=['welcomeStage','languageStage','termsStage','accountStage',
             'signInStage','verifyingStage','appearanceStage','completeStage'];
  for (const id of ids) assert.ok(setup.includes('id: '+id));
  for (let i=0;i<ids.length-1;i++)
    assert.ok(setup.indexOf('id: '+ids[i]) < setup.indexOf('id: '+ids[i+1]));
  assert.match(setup, /sourceComponent: root.currentStep === 0 \? welcomeStage/);
});

test('Setup allows return and review without resetting the account', () => {
  assert.match(setup, /function previousStep\(\)/);
  assert.match(setup, /function switchStep\(step\)/);
  assert.match(setup, /property bool reviewMode: false/);
  assert.match(setup, /signal closeReviewRequested\(\)/);
  assert.match(shell, /property bool setupReviewOpen: false/);
  assert.match(shell, /function openSetupReview\(step\)/);
  assert.match(shell, /visible: presentationBridge.initialSetupRequired \|\| root.setupReviewOpen/);
  assert.match(shell, /reviewMode: root.setupReviewOpen && !presentationBridge.initialSetupRequired/);
  assert.doesNotMatch(setup, /resetInitialSetupProgress\(/);
});

test('existing authentication and secure recovery stay connected to the bridge', () => {
  for(const s of ['root.continueWithGenyleap()', 'root.continueWithFarcaster()',
                  'root.nativeRecoverAccount(words)','root.openApproval()',
                  'root.cancelAuthentication()','recoveryField.clear()'])
    assert.ok(setup.includes(s),s);
  assert.match(setup, /Qt.ImhSensitiveData/);
  assert.match(shell, /presentationBridge.beginNativeRecoverySetup\(recoveryWords\)/);
});

test('legal consent and guest access are respected', () => {
  assert.match(setup, /root.legalConsentAccepted \|\| root.consentChecked/);
  assert.match(setup, /acceptLegalConsent\(true, true, true\)/);
  assert.match(setup, /if \(currentStep === 2\) return legalConsentAccepted \|\| consentChecked/);
  assert.match(setup, /root.guestSelected = true/);
  assert.match(setup, /visible: root.browserReadOnly/);
  assert.match(setup, /finishSetup\(\)/);
});

test('unimplemented account creation uses registration handoff only', () => {
  assert.match(setup, /root.accountIntent = "new"/);
  assert.match(setup, /genycaster.setup.design_create_account/);
  assert.match(setup, /genycaster.setup.new_profile_body/);
  assert.match(setup, /root.openRegistration\(\)/);
});

test('shared theme, native project icons and all three theme modes are present', () => {
  assert.match(setup, /qrc:\/qt\/qml\/Farcaster\/Client\/wasm\/genycaster-icon\.svg/);
  for(const token of ['Theme.accent','Theme.accentInk','Theme.ink',
                       'appearanceController.setThemeMode','Theme.ink'])
    assert.ok(setup.includes(token));
  assert.match(setup, /model: root.greetings/);
});

test('mobile has a bottom CTA, desktop a two-pane welcome', () => {
  assert.match(setup, /id: stageScroll/);
  assert.match(setup, /Layout.preferredHeight: visible \? \(root.phone \? 82 : 86\) : 0/);
  assert.match(setup, /visible: !root.phone && root.width >= 1100 && root.currentStep === 0/);
  assert.match(setup, /Layout.fillWidth: root.phone/);
  assert.match(setup, /onClicked: root.previousStep\(\)/);
});

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

test('new-account registration stays disabled until backend registration exists', () => {
  const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
  assert.match(account, /genycaster.setup.design_create_account/);
  assert.match(account, /genycaster.shell.coming_soon/);
  assert.match(account, /enabled: false/);
  assert.doesNotMatch(account, /onClicked:[\s\S]*?root.openRegistration\(\)/);
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

test('recovery phrase stays in the correct platform UI until verification really starts', () => {
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const dialog=source('ui/wasm/recovery-phrase-dialog.mjs');
  assert.match(setup,/if \(root\.browserReadOnly\) \{\s*\/\/ Browser-only DOM phrase dialog/);
  assert.match(setup,/root\.recoverAccount\(\)/);
  assert.match(setup,/authenticationRoute === "recovery" && browserReadOnly && !authenticated/);
  assert.match(setup,/root\.nativeRecoverAccount\(words\)/);
  assert.match(setup,/recoveryField\.clear\(\)/);
  assert.match(setup,/running: root\.verifyingBusy \|\| root\.recoverySubmitPending/);
  assert.match(setup,/onAuthenticationErrorTextChanged:/);
  assert.match(dialog,/id='genycaster-farcaster-recovery'/);
  assert.match(dialog,/browser\.isSecureContext!==true/);
  assert.match(bridge,/closeGenycasterSetupApproval\(\)/);
});

test('verification success has an explicit next-step action without sign-in popup dependency', () => {
  const stage=setup.slice(setup.indexOf('id: verifyingStage'),setup.indexOf('id: appearanceStage'));
  assert.match(stage,/visible: root\.authenticated/);
  assert.match(stage,/text: qsTrId\("genycaster\.setup\.continue"\)/);
  assert.match(stage,/onClicked: root\.switchStep\(6\)/);
  assert.match(stage,/root\.authenticationApprovalAvailable/);
});

test('terms and privacy open internal scrollable reader with reliable whole-row consent control', () => {
  assert.match(setup,/id: legalReader/);
  assert.match(setup,/id: termsStage/);
  assert.match(setup,/root\.openLegalDocument\("terms"\)/);
  assert.match(setup,/root\.openLegalDocument\("privacy"\)/);
  assert.match(setup,/genycaster\.setup\.terms_body/);
  assert.match(setup,/genycaster\.setup\.privacy_body/);
  assert.match(setup,/onClicked: root\.consentChecked = !root\.consentChecked/);
  assert.doesNotMatch(setup,/Qt\.openUrlExternally/);
});

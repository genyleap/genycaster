import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source = path => readFileSync(path, 'utf8');
const setup = source('ui/components/setup/InitialSetup.qml');
const shell = source('ui/components/shell/AppShell.qml');

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
  assert.match(shell, /visible: root\.setupReviewOpen \|\| \(presentationBridge\.authenticationStateObserved\s*&& presentationBridge\.initialSetupRequired\)/);
  assert.match(shell, /reviewMode: root.setupReviewOpen && !presentationBridge.initialSetupRequired/);
  assert.doesNotMatch(setup, /resetInitialSetupProgress\(/);
});

test('existing authentication and secure recovery stay connected to the bridge', () => {
  for(const s of ['root.continueWithGenyleap()', 'root.continueWithFarcaster()',
                  'root.nativeRecoverFarcasterAccount(words, authorizeDeviceSigner)','root.openApproval()',
                  'root.cancelAuthentication()','nativeRecoveryGrid.clearAll()'])
    assert.ok(setup.includes(s),s);
  assert.match(setup, /Qt.ImhSensitiveData/);
  assert.match(shell, /presentationBridge.beginNativeRecoverySetup\(recoveryWords\)/);
});

test('legal consent and authenticated Setup are enforced', () => {
  assert.match(setup, /if \(!consentChecked\) return/);
  assert.match(setup, /acceptLegalConsent\(true, true, true\)/);
  assert.match(setup, /if \(currentStep === 2\) return consentChecked/);
  assert.doesNotMatch(setup, /guestSelected|continue_as_guest/);
  assert.match(setup, /if \(currentStep === 7\) return reviewMode \|\| \(legalConsentAccepted\s*&& authenticated && \(accountIntent !== "new" \|\| signerReady\)\)/);
  assert.match(setup, /if \(currentStep === 5\) return reviewMode \|\| \(authenticated\s*&& \(accountIntent !== "new" \|\| signerReady\)\)/);
  assert.match(setup, /finishSetup\(\)/);
});

test('registration keeps protocols isolated; a single explicit click authorizes exactly one native account', () => {
  const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
  const create=account.slice(account.indexOf('objectName: "setupCreateAccount"'),
                             account.indexOf('title: qsTrId("genycaster.setup.design_existing_account")'));
  assert.match(create,/root.accountIntent = "new"/);
  assert.match(create,/root.choosingRegistrationProvider = true/);
  assert.match(account,/objectName: "setupRegisterHyperSnap"/);
  assert.match(account,/objectName: "setupRegisterNeynar"/);
  assert.match(account,/objectName: "setupBeginRegistration"[\s\S]*?onClicked: root\.startNativeAccountCreation\(\)/);
  assert.match(setup,/function startNativeAccountCreation\(\)/);
  assert.match(setup,/nativeCreationRequested = true/);
  assert.match(setup,/function submitPreparedNativeAccount\(\)/);
  assert.match(setup,/nativeCreationRequested = false[\s\S]*?nativeRegistrationPublisher\.submitPrepared\(\)/);
  assert.match(setup,/!nativeRegistrationStatus\.accountLookupChecked/);
  assert.match(setup,/!nativeRegistrationStatus\.accountLookupFailed/);
  assert.doesNotMatch(account,/objectName: "setupPrepareNativeProof"/);
  assert.doesNotMatch(account,/objectName: "setupNativeRegistrationConsent"/);
  assert.doesNotMatch(account,/objectName: "setupRegistrationCompleted"/);
  assert.doesNotMatch(source('ui/components/shell/AppShell.qml'),/farcaster.xyz\/~\/signup/);
});

test('signup text inputs use the registered themed QML component', () => {
  const cmake=source('ui/CMakeLists.txt');
  assert.match(cmake,/QT_RESOURCE_ALIAS "components\/DirectionAwareTextField\.qml"/);
  const ids=['walletChallengeFirst','walletChallengeSecond',
             'neynarEnrollmentVoucher','neynarRequestedUsername','wordField'];
  for(const id of ids) {
    assert.match(setup,new RegExp(
      '(?:delegate:\\s*)?DirectionAwareTextField \\{\\s*id: '+id+'\\s*forceLeftToRight: true'),
      id+' must use the theme-aware Latin-script field');
  }
  assert.doesNotMatch(setup,/\bBidiTextField\s*\{/);
  for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                       'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const t=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of ['neynar_voucher_explanation','neynar_voucher_placeholder',
                      'neynar_voucher_ready']) {
      const m=t.match(new RegExp(
        '<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>'));
      assert.ok(m,locale+': '+key);
      assert.match(m[1],/<translation>[^<]+<\/translation>/);
    }
  }
  assert.doesNotMatch(source('ui/i18n/qml_en_US.ts'),/Enter paid registration code/);
  assert.doesNotMatch(source('ui/i18n/qml_en_US.ts'),/Verify enrollment code/);
});

test('registration network status is protocol-scoped, actionable and localized', () => {
  const readiness = setup.slice(
    setup.indexOf('function registrationReadinessText()'),
    setup.indexOf('property bool showRecovery: false'));
  assert.match(readiness, /registrationProviderId === "hypersnap-v2"/);
  assert.match(readiness, /nativeRegistrationStatus\.checking/);
  assert.match(readiness, /!nativeRegistrationStatus\.active/);
  assert.match(readiness, /!runtimeReady/);
  assert.match(readiness, /!nativeRegistrationPublisher\.registrationSupported/);
  assert.match(readiness, /!neynarRegistration\.registrationSupported/);
  assert.match(readiness, /!neynarRegistration\.available/);
  assert.match(readiness, /native_registration_node_ready/);
  assert.match(readiness, /native_registration_node_unavailable/);
  assert.match(readiness, /native_registration_signing_unavailable/);
  assert.match(readiness, /neynar_registration_requirements/);
  assert.match(setup, /objectName: "setupRegistrationReadiness"/);
  assert.match(setup, /text: root\.registrationReadinessText\(\)/);
  assert.match(setup, /!nativeRegistrationPublisher\.pending/);
  assert.match(setup, /!nativeRegistrationPublisher\.confirmed/);
  assert.match(setup, /neynarRegistration\.phase === "idle"/);
  assert.doesNotMatch(setup, /genycaster\.setup\.native_registration_pending/);
  assert.doesNotMatch(setup, /genycaster\.setup\.neynar_registration_pending/);

  const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  const required=[
    'registration_choose_provider', 'registration_shared_fid',
    'registration_neynar_label', 'registration_runtime_initializing',
    'native_registration_node_ready', 'native_registration_node_unavailable',
    'native_registration_signing_unavailable',
    'neynar_registration_requirements', 'native_proof_prepared'];
  for (const locale of locales) {
    const translations=source('ui/i18n/qml_'+locale+'.ts');
    for (const key of required) {
      const id='genycaster.setup.'+key;
      const message=translations.match(
        new RegExp('<message id="'+id.replaceAll('.', '\\.')+'">([\\s\\S]*?)<\\/message>'));
      assert.ok(message, locale+': '+key+' is missing');
      assert.match(message[1], /<translation>[^<]+<\/translation>/,
                   locale+': '+key+' has no translation');
    }
    const registrationCta = translations.match(
      /<message id="genycaster\.setup\.registration_start">([\s\S]*?)<\/message>/);
    assert.ok(registrationCta, locale+': native registration CTA missing');
    assert.match(registrationCta[1], /<source>Submit HyperSnap registration<\/source>/);
    assert.doesNotMatch(registrationCta[1], /Continue to Farcaster registration/);
    assert.doesNotMatch(translations, /Secure wallet onboarding is not connected in this build\./);
    assert.doesNotMatch(translations, /Account creation is disabled until these steps are complete\./);
    assert.doesNotMatch(translations, /No account will be created in this build\./);
  }
});

test('Back never loses an already-submitted HyperSnap account or resubmits it', () => {
  const back=setup.slice(setup.indexOf('function previousStep()'),
                         setup.indexOf('function actionText()'));
  assert.match(back,/if \(!nativeRegistrationPublisher\.pending\) \{[\s\S]*?nativeRegistrationPublisher\.clear\(\)/);
  assert.doesNotMatch(back,/nativeRegistrationPublisher\.stopPolling\(\)/);
  assert.match(back,/nativeCreationRequested = false/);
  assert.match(back,/nativeRegistrationPreparation\.cancel\(\)/);
  assert.match(back,/registrationError = ""/);
  assert.match(back,/choosingRegistrationProvider = false/);
  assert.doesNotMatch(back,/nativeRegistrationPublisher\.submitPrepared\(\)/);
  assert.doesNotMatch(back,/nativeCustodyWallet\.discard\(\)/);
  const publisher=source('ui/app/identity/native_publish.cpp');
  assert.match(publisher,/if \(m_submitted && !confirmed\(\)\) \{/);
  assert.match(publisher,/loadRetryJournal\(\)/);
  assert.match(publisher,/saveRetryJournal\(\)/);
  assert.match(publisher,/eraseRetryJournal\(\)/);
});

test('replacing a verified wallet requires confirmation and preserves old pending journal', () => {
  const account=setup.slice(setup.indexOf('id: accountStage'),
                            setup.indexOf('id: signInStage'));
  assert.match(account,/objectName: "setupWalletReplacement"/);
  assert.match(account,/objectName: "setupChooseDifferentWallet"/);
  assert.match(account,/root\.confirmWalletReplacement = true/);
  assert.match(account,/objectName: "setupWalletReplacementWarning"/);
  assert.match(account,/objectName: "setupConfirmDifferentWallet"/);
  const confirmation=account.slice(account.indexOf('objectName: "setupConfirmDifferentWallet"'),
                                   account.indexOf('objectName: "setupRestoreInterruptedWallet"'));
  assert.match(confirmation,/nativeRegistrationPublisher\.detachWalletForNewRegistration\(\)/);
  assert.match(confirmation,/nativeCustodyWallet\.discard\(\)/);
  assert.doesNotMatch(confirmation,/nativeRegistrationPublisher\.submitPrepared\(\)/);
  const header=source('ui/app/identity/native_publish.hpp');
  const publisher=source('ui/app/identity/native_publish.cpp');
  assert.match(header,/Q_INVOKABLE bool detachWalletForNewRegistration\(\)/);
  assert.match(publisher,/bool HyperSnapRegistrationPublisher::detachWalletForNewRegistration\(\)/);
  assert.match(publisher,/if \(m_submitted && !m_address\.isEmpty\(\)\)\s*saveRetryJournal\(\)/);
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const xml=source('ui/i18n/qml_'+locale+'.ts');
    for(const id of ['wallet_switch_action','wallet_switch_warning','wallet_switch_confirm'])
      assert.ok(xml.includes('id="genycaster.setup.'+id+'"'),locale+' '+id);
  }
});

test('HyperSnap pending is a single minimal read-only state, technical diagnostics are optional', () => {
  const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
  const broadcast=account.slice(account.indexOf('objectName: "nativeRegistrationBroadcastPanel"'));
  const cta=broadcast.slice(broadcast.indexOf('objectName: "setupBeginRegistration"'),
                             broadcast.indexOf('objectName: "setupRetryNativeAccountLookup"'));
  assert.match(cta,/visible: !nativeRegistrationPublisher\.pending/);
  assert.match(cta,/onClicked: root\.startNativeAccountCreation\(\)/);
  assert.doesNotMatch(cta,/\.submitPrepared\(\)/);
  assert.match(broadcast,/objectName: "setupNativeAwaitingConfirmation"/);
  assert.match(broadcast,/objectName: "setupNativePendingDescription"/);
  assert.match(broadcast,/objectName: "setupNativeTechnicalDetailsToggle"/);
  assert.match(broadcast,/objectName: "setupNativeTechnicalDetails"[\s\S]*?visible: root\.nativeDetailsExpanded/);
  assert.match(broadcast,/objectName: "setupCheckNativeRegistrationConfirmation"/);
  assert.match(broadcast,/objectName: "setupNativeRetryWindow"/);
  assert.match(broadcast,/objectName: "setupReleaseExpiredNativeAttempt"/);
  assert.match(broadcast,/nativeRegistrationPublisher\.releaseExpiredAttempt\(\)/);
  assert.match(setup,/function resumeNativePendingChecks\(\)/);
  assert.match(setup,/nativeRegistrationPublisher\.checkConfirmation\(\)/);
  assert.match(setup,/nativeRegistrationPublisher\.retryReady/);
  const publisher=source('ui/app/identity/native_publish.cpp');
  assert.match(publisher,/m_lastSubmissionHttpStatus = result\.httpStatus/);
  assert.match(publisher,/Never clear m_pending \/ m_submitted/);
  assert.match(publisher,/m_pending && m_checks < 240/);
  assert.doesNotMatch(publisher,/m_pending = false;\s*\/\/.*[Tt]imeout/);
});

test('wallet recovery phrase copy is explicit, guarded and never exported through QML', () => {
  const header=source('ui/app/identity/native_wallet.hpp');
  const wallet=source('ui/app/identity/native_wallet.cpp');
  const account=setup.slice(setup.indexOf('id: accountStage'),
                            setup.indexOf('id: signInStage'));
  assert.match(header, /Q_INVOKABLE bool copyRecoveryWords\(\)/);
  assert.match(account, /objectName: "setupCopyRecoveryWords"/);
  assert.match(account, /visible: nativeCustodyWallet\.recoveryVisible/);
  assert.match(account, /nativeCustodyWallet\.copyRecoveryWords\(\)/);
  assert.match(account, /wallet_copy_warning/);
  assert.match(wallet, /if \(!recoveryVisible\(\)/);
  assert.match(wallet, /clipboard->setText\(text\)/);
  assert.match(wallet, /QTimer::singleShot\(60'000, this/);
  assert.match(wallet, /if \(actual == expected\) clipboard->clear\(\)/);
  assert.match(wallet, /void NativeCustodyWalletController::discard\(\)\s*\{\s*clearCopiedRecoveryWords\(\)/);
  assert.doesNotMatch(header, /Q_PROPERTY\([^)]*(?:mnemonic|recoveryWords|phrase)/i);
  const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const locale of locales) {
    const catalog=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of ['wallet_copy_label','wallet_copy_success','wallet_copy_warning']) {
      const message=catalog.match(new RegExp(
        '<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>'));
      assert.ok(message,locale+': missing '+key);
      assert.match(message[1],/<translation>[^<]+<\/translation>/,
                   locale+': untranslated '+key);
    }
  }
});

test('native recovery authorizes a device signer only with explicit user consent', () => {
  const qml=source('ui/components/setup/InitialSetup.qml');
  const runtime=source('ui/app/runtime/bindings.cpp');
  const platform=source('src/platform/custody_recovery.cpp');
  const app=source('src/application/signer.cpp');
  assert.match(qml,/objectName: "nativeRecoveryDeviceSignerConsent"/);
  assert.match(qml,/deviceSignerConsent\.checked/);
  assert.match(qml,/GenyCheckRow \{[\s\S]*?id: deviceSignerConsent/);
  assert.match(source('ui/components/controls/GenyCheckRow.qml'),/Accessible\.role: Accessible\.CheckBox/);
  assert.match(qml,/genycaster.setup.recovery_signer_consent/);
  assert.match(runtime,/makeOneShotRecoveryAuthorizer\(mnemonic,/);
  assert.match(platform,/wordCount != 24/);
  assert.match(platform,/request.fid.value != m_verifiedFid/);
  assert.match(platform,/std::chrono::minutes\{3\}/);
  assert.match(runtime,/completeRecoveryChallenge\([\s\S]*?makeOneShotRecoveryAuthorizer\(/);
  assert.match(runtime,/acceptRecoveryGrant\([\s\S]*?signerService->provision\(/);
  assert.match(runtime,/nativeRecoveryGeneration->load\(std::memory_order_acquire\)/);
  assert.match(app,/oneShotAuthorizer\.reset\(\)/);
  assert.match(platform,/if \(m_consumed\)/);
  assert.match(platform,/OPENSSL_cleanse\(key.bytes.data\(\), key.bytes.size\(\)\)/);
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translations=source('ui/i18n/qml_'+locale+'.ts');
    assert.match(translations,/<message id="genycaster.setup.recovery_signer_consent">/);
    assert.match(translations,/<translation>[^<]+<\/translation>/);
  }
});

test('native desktop and mobile recovery use 24 styled secret word slots', () => {
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  assert.match(setup,/id: nativeRecoveryGrid/);
  assert.match(setup,/objectName: "nativeRecoveryWordGrid"/);
  assert.match(setup,/id: recoveryWordRepeater\s+model: 24/);
  assert.match(setup,/columns: root.phone \? 2 : 4/);
  assert.match(setup,/nativeRecoveryGrid\.completeWordCount === 24/);
  assert.match(setup,/property int completeWordCount: 0/);
  assert.match(setup,/onTextChanged: nativeRecoveryGrid\.refreshCompletion\(\)/);
  assert.match(setup,/function refreshCompletion\(\)/);
  assert.match(setup,/nativeRecoveryGrid\.distribute\(text, index\)/);
  assert.match(setup,/id: revealWords\s+objectName: "nativeRecoveryRevealWords"/);
  assert.match(setup,/GenyCheckRow \{[\s\S]*?id: revealWords/);
  assert.match(source('ui/components/controls/GenyCheckRow.qml'),/Accessible\.role: Accessible\.CheckBox/);
  assert.match(source('ui/components/controls/GenyCheckRow.qml'),/fillColor: control\.checked \? Theme\.accent : Theme\.surface/);
  assert.doesNotMatch(setup,/id: recoveryField|CheckBox \{/);
  assert.match(bridge,/bool exactly24Words\(const QString& phrase\)/);
  assert.match(bridge,/!exactly24Words\(recoveryWords\)/);
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                         'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translation=source('ui/i18n/qml_'+locale+'.ts');
    assert.match(translation,/<message id="genycaster.setup.native_recovery_placeholder">/);
    assert.match(translation,/<source>Enter your 24 recovery words in order<\/source>/);
  }
});

test('shared theme, native project icons and all three theme modes are present', () => {
  assert.match(setup, /qrc:\/qt\/qml\/Farcaster\/Client\/wasm\/genycaster-icon\.svg/);
  // Setup uses semantic light/dark surface ink and the contrast resolver;
  // the former Theme.accentInk alias is no longer a valid Setup token.
  for(const token of ['Theme.accent', 'Theme.accentSurfaceInk',
                       'Theme.contrastInk(Theme.accent)', 'Theme.ink',
                       'appearanceController.setThemeMode'])
    assert.ok(setup.includes(token));
  const languageStage = setup.slice(setup.indexOf('id: languageStage'), setup.indexOf('id: termsStage'));
  assert.match(languageStage, /objectName: "setupLanguageGrid"\s*visible: !root\.phone/);
  assert.match(languageStage, /model: root\.phone \? \[\] : root\.greetings/);
  assert.match(languageStage, /onClicked: localizationController\.setLanguageTag\(modelData\.tag\)/);
  assert.match(languageStage, /SetupLanguageSelector \{\s*id: languageWheel\s*visible: root\.phone/);
});

test('stage CTA is adjacent to the active form, desktop retains a two-pane welcome', () => {
  assert.match(setup, /id: stageScroll/);
  assert.match(setup, /id: stageAction/);
  assert.match(setup, /id: desktopBrandPane/);
  assert.match(setup, /width: root\.currentStep === 0 \? Math\.min\(parent\.width \* 0\.35, 560\)/);
  assert.match(setup, /anchors.top: stageLoader.bottom/);
  assert.match(setup, /anchors.topMargin: Theme.space5/);
  assert.match(setup, /onClicked: root.previousStep\(\)/);
});

test('recovery phrase stays in the correct platform UI until verification really starts', () => {
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const dialog=source('ui/wasm/recovery-phrase-dialog.mjs');
  assert.match(setup,/if \(root\.browserReadOnly\) \{/);
  assert.match(setup,/root\.recoverAccount\(root\.recoveryProtocol\)/);
  assert.match(setup,/authenticationRoute === "browser-protocol-recovery"/);
  assert.match(setup,/authenticationRoute === "browser-protocol-create"/);
  assert.match(setup,/root\.nativeRecoverFarcasterAccount\(words, authorizeDeviceSigner\)/);
  assert.match(setup,/nativeRecoveryGrid\.clearAll\(\)/);
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
  assert.match(stage,/enabled: !root\.recoverySubmitPending[\s\S]*?&& !root\.authenticationInProgress/);
  assert.match(stage,/if \(root\.accountIntent !== "new" \|\| root\.signerReady\)\s*root\.switchStep\(6\)/);
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
  // Legal content stays in-app; the independent company copyright link is external.
  const legal = setup.slice(setup.indexOf('id: legalReader'), setup.indexOf('id: setupCanvas'))
    + setup.slice(setup.indexOf('id: termsStage'), setup.indexOf('id: accountStage'));
  assert.doesNotMatch(legal,/Qt\.openUrlExternally/);
});

test('full-window setup removes shell chrome and has no fake desktop header or footer', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const app=source('ui/components/shell/AppShell.qml');
  assert.match(s,/id: setupCanvas/);
  assert.match(s,/id: desktopBrandPane/);
  assert.match(s,/width: root\.currentStep === 0 \? Math\.min\(parent\.width \* 0\.35, 560\)/);
  assert.match(s,/id: stageAction/);
  assert.match(s,/anchors\.bottom: parent\.bottom/);
  assert.doesNotMatch(s,/id: setupFooter/);
  assert.match(app,/id: shellContent[\s\S]*?visible: presentationBridge\.authenticationStateObserved && !root\.initialSetupOpen/);
  assert.match(app,/id: globalStatus[\s\S]*?visible: !root\.mobile && presentationBridge\.authenticationStateObserved\s*&& !root\.initialSetupOpen/);
});

test('welcome uses the same real eight-stage indicator as the other setup stages', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  assert.match(s,/readonly property int progressCount: wizardSteps\.length/);
  const welcome=s.slice(s.indexOf('id: welcomeStage'),s.indexOf('id: languageStage'));
  const header=s.slice(s.indexOf('id: setupProgress'),s.indexOf('        ActionButton {\n            z: 20'));
  assert.match(header,/stepCount: root\.progressCount/);
  assert.match(header,/currentStep: root\.currentStep/);
  assert.match(welcome,/SetupStepIndicator \{/);
  assert.match(welcome,/stepCount: root\.progressCount/);
  assert.match(welcome,/currentStep: root\.currentStep/);
  assert.equal((s.match(/SetupStepIndicator \{/g) || []).length, 2);
  assert.doesNotMatch(welcome,/model: 3/);
  assert.match(s,/id: stageTransitionTimer/);
  assert.match(s,/if \(!visible \|\| Theme\.reduceMotion\)/);
});

test('mobile legal reader is a bottom sheet, desktop reader stays centered', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  assert.match(s,/id: legalReader/);
  assert.match(s,/id: legalSurface/);
  assert.match(s,/x: root\.phone \? 0/);
  assert.match(s,/y: root\.phone \? legalReader\.height - height/);
  assert.match(s,/Behavior on offset/);
  assert.match(s,/id: legalBodyScroll/);
  assert.match(s,/onClicked: root\.consentChecked = !root\.consentChecked/);
  assert.match(s,/property bool consentChecked: false/);
});

test('desktop brand visual persists across all stages without blocking RTL title icons', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const header=source('ui/components/setup/SetupPageHeader.qml');
  const card=source('ui/components/setup/SetupActionCard.qml');
  assert.match(s,/id: desktopBrandPane[\s\S]*?visible: !root.phone && root.width >= 980/);
  assert.doesNotMatch(s,/visible: !root.phone && root.width >= 980 && root.currentStep === 0/);
  assert.match(s,/id: desktopBrandArt[\s\S]*?fragmentShader: "qrc:\/qt\/qml\/Farcaster\/Client\/shaders\/setup_glass_g\.frag\.qsb"/);
  assert.match(s,/ShaderEffect \{/);
  assert.match(s,/property vector2d paneSize/);
  assert.match(s,/property real darkMode: Theme\.dark \? 1\.0 : 0\.0/);
  assert.match(s,/id: desktopBrandMetadata/);
  assert.match(s,/anchors\.bottom: parent\.bottom/);
  assert.match(s,/Qt\.application\.version \? "v" \+ LocaleFormat\.localizeDigits\(Qt\.application\.version\)/);
  assert.match(s,/new Date\(\)\.getFullYear\(\)/);
  assert.doesNotMatch(s,/source: .*setup-motif\.svg/);
  assert.match(s,/root\.currentStep >= 3 && root\.currentStep <= 6 \? 810 : 680/);
  assert.match(header,/Theme.rtl \? Qt.AlignRight : Qt.AlignLeft/);
  assert.match(s,/iconGlyph: Theme.iconPalette/);
  assert.ok(s.includes('brandImageSource: (Qt.platform.os === "android" ? "image://genycaster-brand/mark"'));
  assert.match(s,/qrc:\/qt\/qml\/Farcaster\/Client\/wasm\/genycaster-icon.svg/);
  assert.match(card,/property url brandImageSource/);
});

test('signout returns immediately to Welcome and legal consent accepts a deliberate tap', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  assert.match(s,/Sign-out always restores Welcome immediately/);
  assert.match(s,/currentStep = 0/);
  assert.match(s,/accountIntent = ""/);
  assert.match(s,/consentChecked = legalConsentAccepted/);
  assert.match(s,/enabled: true\s+focusPolicy: Qt.StrongFocus/);
  assert.match(s,/Accessible.checked: root.consentChecked/);
  assert.match(s,/if \(!consentChecked\) return/);
  assert.match(s,/function openLegalDocument\(kind\)/);
  assert.match(s,/legalReader\.open\(\)/);
  assert.match(s,/onClicked: root.openLegalDocument\("privacy"\)/);
});

test('RTL back and close do not occupy the same top corner', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const nav=s.slice(s.indexOf('        ActionButton {\n            z: 20'),s.indexOf('        Flickable {\n            id: stageScroll'));
  assert.match(nav,/objectName: "setupBack"[\s\S]*?Theme\.rtl \? stagePanel\.x \+ stagePanel\.width - width : stagePanel\.x/);
  assert.match(nav,/objectName: "setupCloseReview"[\s\S]*?Theme\.rtl \? stagePanel\.x : stagePanel\.x \+ stagePanel\.width - width/);
  const hero=s.slice(s.indexOf('id: desktopBrandPane'),s.indexOf('id: desktopSetupNav'));
  assert.match(hero,/x: Theme\.rtl \? setupCanvas\.width - width : 0/);
  assert.doesNotMatch(hero,/anchors\.(?:left|right): Theme\.rtl/);
  assert.match(nav,/iconGlyph: Theme\.rtl \? Theme\.iconArrowRight : Theme\.iconArrowLeft/);
});

test('acceptance toggle is borderless unless keyboard focused', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const start=s.indexOf('id: consentCheck');
  const end=s.indexOf('id: accountStage');
  const terms=s.slice(start,end);
  assert.match(terms,/borderWidth: Theme\.keyboardFocusVisible\(consentCheck\) \? 2 : Theme\.strokeHairline/);
  assert.match(terms,/fillColor: consentCheck\.down \? Theme\.surfacePressed/);
  assert.match(terms,/onClicked: root\.consentChecked = !root\.consentChecked/);
});

test('desktop account keeps editorial cards while enabling registration provider choice', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const card=source('ui/components/setup/SetupActionCard.qml');
  const account=s.slice(s.indexOf('id: accountStage'),s.indexOf('id: signInStage'));
  assert.match(account,/accountShowcase: true/);
  assert.match(account,/objectName: "setupCreateAccount"/);
  assert.match(account,/iconGlyph: Theme\.iconUser/);
  assert.match(account,/root\.choosingRegistrationProvider = true/);
  assert.match(account,/objectName: "setupRegisterHyperSnap"/);
  assert.match(account,/objectName: "setupRegisterNeynar"/);
  assert.doesNotMatch(account,/setupEmphasis: true/);
  assert.match(account,/onClicked: \{\s*neynarRegistration\.reset\(\)\s*nativeRegistrationPublisher\.clear\(\)\s*nativeRegistrationPreparation\.cancel\(\)\s*nativeRegistrationStatus\.clearExistingLookup\(\)\s*nativeCustodyWallet\.discard\(\)\s*root\.accountIntent = "existing"/);
  assert.match(account,/iconGlyph: Theme\.iconLink/);
  assert.match(card,/property bool accountShowcase: false/);
  assert.match(card,/property bool setupEmphasis: false/);
  assert.match(card,/id: editorialAccountCard/);
  assert.match(card,/property bool networkShowcase: false/);
  assert.match(card,/preferredSize: 36/);
  assert.match(card,/preferredSize: root.connectionText \? 24 : 40/);
  assert.match(card,/id: networkCardRow/);
  assert.match(account,/networkShowcase: true/);
  assert.match(account,/iconGlyph: "quilibrium\/hypersnap"/);
  assert.match(account,/iconGlyph: "brands\/farcaster"/);
});

test('Setup returns through nested choices without losing custody and adapts recovery layout', () => {
  const setup=source('ui/components/setup/InitialSetup.qml');
  const back=setup.slice(setup.indexOf('function previousStep()'),setup.indexOf('function actionText()'));
  assert.match(back,/if \(currentStep === 3 && choosingRegistrationProvider\)/);
  assert.match(back,/choosingRegistrationProvider = false/);
  assert.match(back,/stageScroll\.contentY = 0/);
  assert.doesNotMatch(back,/nativeCustodyWallet\.discard\(\)/);
  assert.match(back,/if \(currentStep === 4 && showRecovery\)/);
  assert.match(setup,/objectName: "setupExistingProtocolChoice"/);
  assert.match(setup,/columns: root\.phone \? 2 : 4/);
  assert.match(setup,/String\(wordField\.index \+ 1\)\.padStart\(2, "0"\)/);
  assert.match(setup,/nativeRecoveryGrid\.completeWordCount/);
  const card=source('ui/components/setup/SetupActionCard.qml');
  assert.match(card,/layoutDirection: Theme\.uiDirection/);
  assert.match(card,/id: networkCardRow/);
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translation=source('ui/i18n/qml_'+locale+'.ts');
    assert.match(translation,/<source>Create a new identity with a wallet you control\.<\/source>/);
    assert.match(translation,/<source>Native FID · self-custody wallet<\/source>/);
    assert.match(translation,/<source>FID on Optimism · registered via Neynar<\/source>/);
  }
});

test('shared marker geometry maintains a common vertical center for pill and eight dots', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const component=source('ui/components/setup/SetupStepIndicator.qml');
  const manifest=source('ui/CMakeLists.txt');
  assert.match(manifest,/components\/setup\/SetupStepIndicator\.qml/);
  assert.match(s,/id: setupProgress/);
  assert.match(s,/readonly property int progressCount: wizardSteps\.length/);
  assert.match(component,/model: root\.safeCount/);
  assert.match(component,/readonly property int safeCount: Math\.max\(1, stepCount\)/);
  assert.match(component,/readonly property int activeIndex: Math\.max\(0, Math\.min\(safeCount - 1, currentStep\)\)/);
  assert.match(component,/anchors\.centerIn: parent/);
  assert.match(component,/height: 16/);
  assert.match(component,/width: marker\.active \? 26 : 7/);
  assert.match(component,/height: marker\.active \? 5 : 7/);
  assert.match(component,/Behavior on fillColor/);
  assert.match(component,/Theme\.reduceMotion/);
});

test('browser recovery occupies the native Setup pane without exposing secrets to QML', () => {
  const dialog=source('ui/wasm/recovery-phrase-dialog.mjs');
  assert.match(dialog,/accountMode\?'0':'min\(29vw,430px\)'/);
  assert.match(dialog,/accountMode\?'dialog':'region'/);
  assert.match(dialog,/grid-template-columns:repeat\(4,minmax\(0,1fr\)\)/);
  assert.match(dialog,/grid-template-columns:repeat\(2,minmax\(0,1fr\)\)/);
  assert.match(dialog,/genycaster-recovery-reveal:focus-visible/);
  assert.match(dialog,/if\(accountMode\)card.setAttribute\('aria-modal','true'\)/);
  assert.doesNotMatch(dialog,/backdrop-filter:blur\(7px\)/);
  assert.match(dialog,/word\.autocomplete='off'/);
  assert.doesNotMatch(dialog,/localStorage\.setItem/);
});

test('completion badge is a flat accent check without glow or shadow', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const complete=s.slice(s.indexOf('id: completeStage'));
  assert.match(complete,/id: completionBadge/);
  assert.match(complete,/fillColor: Theme\.accent/);
  assert.doesNotMatch(complete,/layer\.effect:|shadowEnabled:|shadowColor:|shadowBlur:|MultiEffect/);
  // The studio rail owns one color-only pass; the completion badge stays flat.
  const studio=s.slice(s.indexOf('id: desktopBrandPane'),s.indexOf('id: setupProgress'));
  assert.equal((studio.match(/layer\.effect: MultiEffect/g) || []).length, 1);
  assert.doesNotMatch(studio,/blurEnabled: true|shadowEnabled: true/);
});

test('recovery appears before Genyleap and Farcaster, stays usable in review mode', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const stage=s.slice(s.indexOf('id: signInStage'),s.indexOf('id: verifyingStage'));
  const recovery=stage.indexOf('genycaster.setup.design_recovery_title');
  const genyleap=stage.indexOf('genycaster.shell.continue_with_genyleap');
  const farcaster=stage.indexOf('genycaster.shell.sign_in_with_farcaster');
  assert.ok(recovery>=0 && recovery<genyleap && genyleap<farcaster);
  assert.match(stage,/setupEmphasis: true/);
  assert.match(stage,/root\.recoverAccount\(root\.recoveryProtocol\)/);
  assert.match(stage,/root\.showRecovery = !root\.showRecovery/);
  const chooser=stage.indexOf('objectName: "setupExistingProtocolChoice"');
  const nativeRecovery=stage.indexOf('visible: root.showRecovery && (root.existingProtocolSelected');
  assert.ok(chooser>=0 && chooser<recovery && recovery<nativeRecovery
    && farcaster<nativeRecovery, 'Protocol selection precedes the action group and inline recovery form');
  assert.doesNotMatch(stage,/objectName: "setupBrowserRecoveryProtocolChoice"/);
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const handler=bridge.slice(bridge.indexOf('void PresentationBridge::beginRecoverySetup()'),
     bridge.indexOf('void PresentationBridge::beginNativeRecoverySetup('));
  assert.doesNotMatch(handler,/!initialSetupRequired\(\)/);
  assert.match(handler,/signInRequested\(QStringLiteral\("recovery"\)\)/);
  assert.match(handler,/applyAuthenticationError/);
});

test('copyright and Beta version remain readable at the bottom of the desktop left sidebar', () => {
  const s=source('ui/components/setup/InitialSetup.qml');
  const pane=s.slice(s.indexOf('id: desktopBrandPane'), s.indexOf('id: setupProgress'));
  assert.match(pane,/visible: !root\.phone && root\.width >= 980/);
  assert.match(pane,/clip: true\s+\/\/ Keep this pane[\s\S]*?z: 2/);
  assert.match(pane,/id: desktopBrandMetadata/);
  assert.match(pane,/anchors\.bottom: parent\.bottom/);
  assert.match(pane,/anchors\.left: parent\.left/);
  assert.match(pane,/anchors\.right: parent\.right/);
  assert.match(pane,/visible: desktopBrandPane\.visible/);
  assert.match(pane,/new Date\(\)\.getFullYear\(\)/);
  assert.match(pane,/Qt\.application\.version \? "v" \+ LocaleFormat\.localizeDigits/);
  assert.match(pane,/Qt\.application\.version/);
  const theme=source('ui/theme/Theme.qml');
  assert.match(pane,/color: Theme\.setupFooterCopyrightInk/);
  assert.match(pane,/color: Theme\.setupFooterVersionInk/);
  assert.match(pane,/font\.pixelSize: Theme\.typeCaptionSize/);
  assert.match(theme,/readonly property color setupFooterCopyrightInk: dark \? "#E2EAF4" : "#DFE8F4"/);
  assert.match(pane,/id: genyleapCopyrightLink/);
  assert.match(pane,/text: "© " \+ new Date\(\)\.getFullYear\(\) \+ " Genyleap"/);
  assert.match(pane,/Accessible\.role: Accessible\.Link/);
  assert.match(pane,/activeFocusOnTab: true/);
  assert.match(pane,/Keys\.onReturnPressed: Qt\.openUrlExternally\(\(deploymentEndpoints\.companyOrigin \+ "\/"\)\)/);
  assert.match(pane,/cursorShape: Qt\.PointingHandCursor/);
  assert.match(pane,/onClicked: Qt\.openUrlExternally\(\(deploymentEndpoints\.companyOrigin \+ "\/"\)\)/);
  assert.doesNotMatch(pane,/" GenyCaster"/);
});

test('Setup GPU-rendered volumetric G is cross-platform, theme-aware and interactive',()=>{
  const s=source('ui/components/setup/InitialSetup.qml');
  const pane=s.slice(s.indexOf('id: desktopBrandPane'),s.indexOf('id: setupProgress'));
  const cmake=source('ui/CMakeLists.txt');
  const shader=source('ui/shaders/setup_glass_g.frag');
  assert.match(pane,/ShaderEffect \{/);
  assert.match(pane,/id: desktopBrandArt/);
  assert.match(pane,/paneSize: Qt\.vector2d\(width, height\)/);
  assert.match(pane,/fragmentShader: "qrc:\/qt\/qml\/Farcaster\/Client\/shaders\/setup_glass_g\.frag\.qsb"/);
  assert.match(pane,/Theme\.dark \? 1\.0 : 0\.0/);
  assert.match(pane,/id: brandInteraction/);
  assert.match(pane,/hoverEnabled: true/);
  assert.match(pane,/onPositionChanged: function\(mouse\)/);
  assert.match(pane,/brandAccentTap\.restart\(\)/);
  assert.match(pane,/Keys\.onReturnPressed:/);
  assert.match(pane,/!Theme\.reduceMotion/);
  assert.match(pane,/id: genyleapCopyrightLink/);
  assert.match(pane,/root\.currentStep/);
  assert.doesNotMatch(pane,/desktopMist|mistMote|setup-mist\.svg|Animation\.Infinite/);
  assert.match(cmake,/shaders\/setup_glass_g\.frag\.qsb/);
  assert.match(shader,/vec2 canonicalDistance\(/);
  assert.match(pane,/logoDistanceMap: Image/);
  assert.match(cmake,/shaders\/genycaster_logo_sdf\.png/);
  assert.match(shader,/vec2 logo\(/);
  assert.match(shader,/vec2 scene\(/);
  assert.match(shader,/for\(int i=0;i<116;i\+\+\)/);
  assert.match(pane,/id: automaticRotation/);
  assert.match(pane,/id: rotationCooldown/);
  assert.match(pane,/onPressed: function\(mouse\)/);
  assert.match(pane,/onReleased:/);
  assert.match(pane,/Keys\.onLeftPressed:/);
  assert.match(pane,/property real orbitYaw: desktopBrandPane\.orbitYaw/);
  assert.match(shader,/float orbitYaw/);
  assert.match(shader,/float dragYaw/);
  assert.match(shader,/float dragPitch/);
  assert.match(shader,/normalAt/);
  assert.doesNotMatch(pane,/WebEngineView|WebView|three\.js/);
});

test('new-wallet backup is native, private, verified and never claims premature registration', () => {
  const walletHeader = source('ui/app/identity/native_wallet.hpp');
  const walletImpl = source('ui/app/identity/native_wallet.cpp');
  const main = source('ui/app/main.cpp');
  const registration = source('src/platform/custody_recovery.cpp');
  for (const contract of [
    'Q_PROPERTY(bool hasWallet', 'Q_INVOKABLE bool generate()',
    'Q_INVOKABLE bool verifyBackup', 'Q_INVOKABLE void discard()'
  ]) assert.ok(walletHeader.includes(contract), contract);
  assert.ok(!walletHeader.includes('Q_PROPERTY(QString recoveryWords'));
  assert.ok(!walletHeader.includes('Q_INVOKABLE QString recoveryWords'));
  assert.ok(main.includes('qmlRegisterType<NativeRecoveryWordsItem>'));
  assert.ok(main.includes('{"nativeCustodyWallet", &nativeCustodyWallet}'));
  for (const contract of [
    'objectName: "setupGenerateCustodyWallet"',
    'NativeRecoveryWords {', 'wallet: nativeCustodyWallet',
    'nativeCustodyWallet.startBackupChallenge()',
    'nativeCustodyWallet.verifyBackup(', 'nativeCustodyWallet.discard()',
    'echoMode: TextInput.Password',
    'objectName: "setupBeginRegistration"'
  ]) assert.ok(setup.includes(contract), contract);
  for (const contract of [
    'RAND_priv_bytes', 'CRYPTO_memcmp',
    'm_private->secret.reset()'
  ]) assert.ok(walletImpl.includes(contract), contract);
  assert.ok(registration.includes('solveNativeOnboardPow('));
  assert.ok(registration.includes('hyper.onboard.pow:'));
  assert.ok(registration.includes("maxAttempts > 100'000'000ULL"));
  assert.ok(!shell.includes('farcaster.xyz/~/signup'));
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translation=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of ['wallet_create_label','wallet_recovery_warning',
                      'wallet_challenge_word','wallet_verified']) {
      assert.ok(translation.includes('id="genycaster.setup.'+key+'"'));
    }
  }
});

test('native PoW runs off-thread; one explicit Create click gates a single irreversible submit', () => {
  const native=source('ui/app/identity/native_prepare.cpp');
  const wallet=source('ui/app/identity/native_wallet.cpp');
  const header=source('ui/app/identity/native_prepare.hpp');
  const main=source('ui/app/main.cpp');
  const manifest=source('ui/CMakeLists.txt');
  assert.match(setup,/function startNativeAccountCreation\(\)/);
  assert.match(setup,/nativeCreationRequested = true/);
  assert.match(setup,/nativeRegistrationPreparation\.prepare\(\)/);
  assert.match(setup,/function submitPreparedNativeAccount\(\)/);
  assert.match(setup,/!nativeCreationRequested \|\| !nativeRegistrationPreparation\.prepared/);
  assert.match(setup,/nativeCreationRequested = false/);
  assert.match(setup,/nativeRegistrationPublisher\.submitPrepared\(\)/);
  assert.match(setup,/nativeRegistrationStatus\.findExistingNativeAccount\(/);
  assert.match(setup,/nativeRegistrationStatus\.accountLookupChecked/);
  assert.doesNotMatch(setup,/objectName: "setupPrepareNativeProof"/);
  assert.doesNotMatch(setup,/nativeRegistrationConsent\.checked/);
  assert.match(native,/QtConcurrent::run/);
  assert.match(native,/solveNativeOnboardPow/);
  assert.match(native,/m_ports\.backupVerified\(\)/);
  assert.match(native,/m_ports\.seal\(\*fresh, nonce\)/);
  assert.doesNotMatch(header,/Q_INVOKABLE.*preparedEnvelope/);
  assert.doesNotMatch(header,/Q_PROPERTY.*envelope/);
  assert.match(wallet,/NativeCustodyWalletController::sealNativeOnboard/);
  assert.match(wallet,/if \(!backupVerified\(\)/);
  assert.match(main,/\{"nativeRegistrationPreparation", &nativeRegistrationPreparation\}/);
  assert.match(manifest,/app\/identity\/native_prepare\.cpp/);
});

test('HyperSnap signup, recovery and signer remain distinct guarded flows',()=>{
  const shell=source('ui/components/shell/AppShell.qml');
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const bridgeApi=source('src/presentation/qt/presentation_bridge.hpp');
  const runtime=source('ui/app/runtime/bindings.cpp');
  const publisher=source('ui/app/identity/native_publish.cpp');
  const prep=source('ui/app/identity/native_prepare.cpp');
  for(const required of [
    'objectName: "setupBeginRegistration"',
    'nativeCreationRequested = true',
    'nativeRegistrationPublisher.registrationSupported',
    'nativeRegistrationPublisher.submitPrepared()',
    'nativeRegistrationPublisher.checkConfirmation()',
    'nativeRecoverHyperSnapAccount(words, authorizeDeviceSigner)',
    'root.nativeRecoverFarcasterAccount(words, authorizeDeviceSigner)',
    'objectName: "setupExistingHyperSnap"',
    'objectName: "setupExistingFarcaster"'
  ]) assert.ok(setup.includes(required),required);
  assert.match(shell,/onNativeRecoverHyperSnapAccount/);
  assert.match(bridgeApi,/nativeHyperSnapRecoveryRequested\(QString recoveryWords, bool authorizeSigner\)/);
  assert.match(bridge,/emit nativeHyperSnapRecoveryRequested\(\s*std::move\(recoveryWords\), m_hyperSnapRecoverySignerConsent\)/);
  assert.match(runtime,/nativeHyperSnapRecoveryRequested/);
  assert.match(runtime,/makeOneShotRecoveryAuthorizer/);
  assert.match(runtime,/nativeAccountConfirmed/);
  assert.match(publisher,/m_transport\.submit\(envelope/);
  assert.match(publisher,/scheduleCheck\(generation\)/);
  assert.match(prep,/takeForSubmission/);
  assert.doesNotMatch(setup,/farcaster\.xyz\/~\/signup/);
});

test('Farcaster paid registration uses native wallet and authoritative Optimism confirmation', () => {
  const ui=source('ui/components/setup/InitialSetup.qml');
  const bootstrap=source('ui/app/main.cpp');
  const bindings=source('ui/app/runtime/bindings.cpp');
  const session=source('src/application/session.cpp');
  const signer=source('ui/app/identity/native_wallet.cpp');
  const neynar=source('ui/app/identity/neynar_registration_controller.cpp');
  const header=source('ui/app/identity/neynar_registration_controller.hpp');
  const cmake=source('ui/CMakeLists.txt');
  for(const objectName of [
    'neynarFarcasterRegistration','setupNeynarVoucher',
    'setupUseNeynarVoucher','setupReserveNeynarFid','setupNeynarUsername',
    'setupNeynarTransferConsent','setupSubmitNeynarRegistration',
    'setupCheckNeynarRegistration'
  ]) assert.ok(ui.includes('objectName: "'+objectName+'"'),objectName);
  assert.match(ui,/neynarTransferConsent\.checked/);
  assert.match(ui,/neynarRegistration\.registrationSupported/);
  assert.match(ui,/neynarRegistration\.reserveFid\(\)/);
  assert.match(ui,/neynarRegistration\.submit\(/);
  assert.match(ui,/neynarRegistration\.checkConfirmation\(\)/);
  assert.match(ui,/if \(!nativeRegistrationPublisher\.pending\s*&& !neynarRegistration\.pending/);
  assert.match(bootstrap,/\{"neynarRegistration", &neynarRegistration\}/);
  assert.match(bootstrap,/signReservedFarcasterTransfer\(/);
  assert.match(bindings,/NeynarRegistrationController::farcasterFidConfirmed/);
  assert.match(bindings,/NativeCustodySignerAccess::forConfirmedFarcasterFid/);
  assert.match(bindings,/acceptVerifiedFarcasterCustody/);
  assert.match(bindings,/setPreferredProviderForAccess\(/);
  assert.match(session,/farcaster:optimism-id-registry/);
  assert.match(signer,/NativeCustodyWalletController::signReservedFarcasterTransfer/);
  assert.match(neynar,/m_ports\.signTransfer\(fid, nonce, deadline\)/);
  assert.match(neynar,/m_reservationId = deterministicUuid\(m_voucher, m_wallet\)/);
  assert.match(neynar,/phase == QStringLiteral\("confirmed"\)/);
  assert.match(neynar,/m_voucher\.fill\('\\0'\)/);
  assert.doesNotMatch(header,/Q_INVOKABLE.*signTransfer/);
  assert.doesNotMatch(header,/Q_PROPERTY.*recoveryWords/);
  assert.match(cmake,/app\/identity\/neynar_registration_controller\.cpp/);
  for(const loc of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                     'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const ts=source('ui/i18n/qml_'+loc+'.ts');
    for(const key of ['neynar_voucher_explanation','neynar_reserve_fid',
                      'neynar_transfer_consent','neynar_submit_registration',
                      'neynar_fid_confirmed','neynar_check_again']) {
      assert.ok(ts.includes('id="genycaster.setup.'+key+'"'),loc+':'+key);
    }
  }
});

test('interrupted Farcaster registration can restore offline custody without a second FID',()=>{
  const qml=source('ui/components/setup/InitialSetup.qml');
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const controller=source('ui/app/identity/neynar_registration_controller.cpp');
  const runtime=source('ui/app/runtime/bindings.cpp');
  const wallet=source('ui/app/identity/native_wallet.cpp');
  assert.match(qml,/objectName: "setupRestoreInterruptedWallet"/);
  assert.match(qml,/nativeCustodyWallet\.restoreForRegistration\(words\)/);
  assert.match(qml,/neynarRegistration\.checkConfirmation\(\)/);
  assert.match(controller,/m_reservationId = deterministicUuid\(m_voucher, m_wallet\)/);
  assert.match(controller,/if \(m_phase != QStringLiteral\("ready"\)\) return false/);
  assert.match(wallet,/derivePublicCustodyAddress\(phrase\)/);
  assert.match(bridge,/nativeFarcasterRecoveryRequested\(\s*std::move\(recoveryWords\), m_farcasterRecoverySignerConsent\)/);
  assert.match(runtime,/resolveRecoveryCustodyFid\(\*runtime->transport, \*address/);
  assert.match(runtime,/makeOneShotRecoveryAuthorizer\(/);
  assert.match(runtime,/commitVerifiedIdentity\(fid, address, std::move\(permit\), true, secret\)/);
  assert.doesNotMatch(qml,/farcaster\.xyz\/~\/signup/);
});

test('an existing HyperSnap FID is adopted automatically only after custody-bound verification', () => {
  const ui=source('ui/components/setup/InitialSetup.qml');
  const status=source('ui/app/identity/native_registration.cpp');
  const publisher=source('ui/app/identity/native_publish.cpp');
  const header=source('ui/app/identity/native_publish.hpp');
  const runtime=source('ui/app/runtime/bindings.cpp');
  const adoption=ui.slice(ui.indexOf('function resumeExistingNativeAccount('),
                          ui.indexOf('    Connections {\n        target: nativeCustodyWallet'));
  assert.match(adoption,/nativeRegistrationStatus\.existingNativeFid\.length === 0/);
  assert.match(adoption,/nativeRegistrationPublisher\.confirmExistingAccount\(\)/);
  assert.match(adoption,/nativeExistingResumeRequested = true/);
  assert.doesNotMatch(adoption,/submitPrepared\(\)|takeProof\(/);
  assert.match(status,/verifiedExistingFidForCustody/);
  assert.match(status,/m_lookupAddress == address\.toLower\(\)/);
  assert.match(header,/Q_INVOKABLE bool confirmExistingAccount\(\)/);
  const verifier=publisher.slice(publisher.indexOf('bool HyperSnapRegistrationPublisher::confirmExistingAccount()'),
                                 publisher.indexOf('bool HyperSnapRegistrationPublisher::recoverInterruptedRegistration()'));
  assert.match(verifier,/m_pending \|\| m_submitted/);
  assert.match(verifier,/m_ports\.backupVerified\(\)/);
  assert.match(verifier,/m_ports\.existingFidForCustody\(custody\)/);
  assert.match(verifier,/m_transport\.lookupByCustody\(custody/);
  assert.match(verifier,/result\.fid == expected/);
  assert.match(verifier,/onConfirmed\(result\.fid, generation\)/);
  assert.doesNotMatch(verifier,/submit\(|saveRetryJournal|takeProof/);
  assert.match(runtime,/nativeAccountConfirmed/);
  assert.match(runtime,/NativeCustodySignerAccess::forConfirmedNativeFid/);
});

test('confirmed HyperSnap identity signs in automatically and offers read-only retry on errors', () => {
  const ui=source('ui/components/setup/InitialSetup.qml');
  const publisher=source('ui/app/identity/native_publish.cpp');
  const header=source('ui/app/identity/native_publish.hpp');
  const bridge=source('ui/app/runtime/bindings.cpp');
  const account=ui.slice(ui.indexOf('id: accountStage'),ui.indexOf('id: signInStage'));
  assert.match(account,/objectName: "setupContinueConfirmedNativeAccount"/);
  assert.match(account,/onClicked: nativeRegistrationPublisher\.resumeConfirmedAccount\(\)/);
  assert.match(account,/objectName: "setupNativeAwaitingConfirmation"/);
  assert.match(account,/visible: nativeRegistrationPublisher\.pending/);
  assert.match(account,/objectName: "setupWalletReplacement"[\s\S]*?&& !nativeRegistrationPublisher\.confirmed/);
  assert.match(account,/title: root\.accountFidRegistered[\s\S]*?genycaster\.setup\.account_verified/);
  assert.match(ui,/onAuthenticatedChanged:/);
  assert.match(ui,/if \(visible && authenticated && currentStep >= 3 && currentStep <= 5\)/);
  const retry=publisher.slice(publisher.indexOf('bool HyperSnapRegistrationPublisher::resumeConfirmedAccount()'),
                              publisher.indexOf('bool HyperSnapRegistrationPublisher::recoverInterruptedRegistration()'));
  assert.match(header,/Q_INVOKABLE bool resumeConfirmedAccount\(\)/);
  assert.match(retry,/m_transport\.lookupByCustody\(m_address/);
  assert.match(retry,/result\.fid == expected/);
  assert.match(retry,/onConfirmed\(result\.fid, generation\)/);
  assert.doesNotMatch(retry,/\.submit\(|takeProof|saveRetryJournal/);
  assert.match(bridge,/nativePublisher->setIdentityRuntimeReady\(canAttach\)/);
});

test('HyperSnap native signer authorization is local, one-use and registry-bound',()=>{
  const shell=source('ui/components/shell/AppShell.qml');
  const bridgeHeader=source('src/presentation/qt/presentation_bridge.hpp');
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const runtime=source('ui/app/runtime/bindings.cpp');
  const signerService=source('src/application/signer.cpp');
  const setup=source('ui/components/setup/InitialSetup.qml');
  assert.match(bridgeHeader,/Q_INVOKABLE void authorizeNativeHyperSnapSigner\(QString recoveryWords\)/);
  assert.match(bridgeHeader,/void nativeHyperSnapSignerAuthorizationRequested\(QString recoveryWords\)/);
  assert.match(bridgeHeader,/Q_PROPERTY\(bool nativeCustodySignerAvailable/);
  assert.match(runtime,/updateNativeSignerAvailability/);
  assert.match(shell,/!presentationBridge.nativeCustodySignerAvailable/);
  assert.match(bridge,/if \(!nativeCustodyAccount\(\) \|\| m_accountFid < \(1ULL << 32\)\)/);
  assert.match(bridge,/emit nativeHyperSnapSignerAuthorizationRequested\(std::move\(recoveryWords\)\)/);
  assert.match(shell,/readonly property bool nativeAccount:\s*presentationBridge\.nativeCustodyAccount/);
  assert.match(shell,/objectName: "nativeSignerRecoveryGrid"/);
  assert.match(shell,/id: nativeSignerWordGrid/);
  assert.match(shell,/model: 24/);
  assert.match(shell,/showRecoveryWords[\s\S]*?TextInput.Normal : TextInput.Password/);
  assert.match(shell,/Qt.ImhSensitiveData/);
  assert.match(shell,/presentationBridge.authorizeNativeHyperSnapSigner\(\s*interactionAuthorizationDialog.recoveryPhrase\(\)\)/);
  assert.match(shell,/interactionAuthorizationDialog.clearRecoveryWords\(\)/);
  const native=runtime.slice(runtime.indexOf('PresentationBridge::nativeHyperSnapSignerAuthorizationRequested'),
                             runtime.indexOf('PresentationBridge::signerProvisionRequested'));
  assert.match(native,/identityIssuer != "hypersnap:native-registry"/);
  assert.match(native,/session->fid.value != bridge.accountFid\(\)/);
  assert.match(native,/lookupByCustody\(custody/);
  assert.match(native,/result.fid != fid/);
  assert.match(native,/makeOneShotRecoveryAuthorizer/);
  // A platform recovery enum must not be assigned to the foundation Error DTO.
  assert.doesNotMatch(native,/\.error\s*=\s*std::move\(permit\.error\(\)\)/);
  assert.match(native,/using RecoveryError = farcaster::platform::custody_recovery::Error/);
  assert.match(native,/permit\.error\(\) == RecoveryError::KeyDerivationFailed/);
  assert.match(native,/\.error\s*=\s*foundation::error::Error::make\(/);
  assert.match(native,/runtime->signerService->provision\(\s*current, signerObserver, std::move\(\*permit\)\)/);
  assert.doesNotMatch(native,/resolveRecoveryCustodyFid\(|\bsubmitPrepared\(|\bregisterGasless\(/);
  const direct=runtime.slice(runtime.indexOf('PresentationBridge::signerProvisionRequested'),
                             runtime.indexOf('PresentationBridge::signerRevokeRequested'));
  assert.match(direct,/NativeCustodySignerAccess::forConfirmedNativeFid/);
  assert.match(direct,/without a live custody key/i);
  assert.match(direct,/SignerService/);
  assert.match(direct,/session, signerObserver, std::move\(localPermit\)/);
  assert.match(signerService,/session->identityIssuer == "hypersnap:native-registry"\s*&& !oneShotAuthorizer/);
  assert.match(setup,/readonly property bool accountFidRegistered/);
  assert.match(setup,/readonly property bool farcasterFidRegistered/);
  assert.match(setup,/function maybeLookupNativeFid\(\)/);
  assert.match(setup,/qsTrId\("genycaster.setup.registration_next_device"\)/);
  for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                       'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const translated=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of ['genycaster.shell.native_signer_description',
                      'genycaster.shell.native_signer_use_words',
                      'genycaster.shell.native_signer_words_note',
                      'genycaster.shell.native_signer_authorize',
                      'genycaster.setup.registration_next_device'])
      assert.ok(translated.includes('id="'+key+'"'),locale+' '+key);
  }
});

test('signer parse errors are not reported as a rejected network authorization',()=>{
  const runtime=source('ui/app/runtime/bindings.cpp');
  const begin=runtime.indexOf('[[nodiscard]] QString signerErrorText(');
  const end=runtime.indexOf('void applySignerState(',begin);
  assert.ok(begin >= 0 && end > begin);
  const mapping=runtime.slice(begin,end);
  assert.match(mapping,/case ErrorCode::ProtocolFailure:\s*\n[\s\S]*?genycaster\.runtime\.the_signer_operation_could_not_be_completed_safely/);
  assert.match(mapping,/case ErrorCode::Conflict:\s*\n[\s\S]*?genycaster\.runtime\.snapchain_rejected_or_contradicted_the_signer_authorization_/);
  assert.doesNotMatch(mapping,/case ErrorCode::ProtocolFailure:\s*\n\s*case ErrorCode::Conflict:/);
});

test('HyperSnap interrupted native registration restores the same custody without replay',()=>{
  const ui=source('ui/components/setup/InitialSetup.qml');
  const publisher=source('ui/app/identity/native_publish.cpp');
  const transport=source('ui/app/identity/hypersnap_registration_transport.cpp');
  const runtime=source('ui/app/runtime/bindings.cpp');
  assert.match(ui,/setupRestoreInterruptedWallet/);
  assert.match(ui,/nativeRegistrationPublisher\.recoverInterruptedRegistration\(\)/);
  assert.match(ui,/nativeRegistrationPublisher\.pending/);
  assert.match(ui,/nativeRegistrationPublisher\.checkConfirmation\(\)/);
  assert.match(publisher,/m_submitted = true;\s*m_pending = true;\s*m_unknown = true/);
  assert.match(publisher,/lookup\(false, generation\)/);
  assert.match(publisher,/if \(m_submitted && !confirmed\(\)\)/);
  assert.match(publisher,/nativeAccountConfirmed/);
  assert.match(transport,/\/v1\/nativeAccount/);
  assert.match(transport,/!data\.value\(QStringLiteral\("message"\)\)\.isObject\(\)/);
  assert.match(runtime,/acceptVerifiedNativeCustody/);
});

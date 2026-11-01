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
                  'root.cancelAuthentication()','nativeRecoveryGrid.clearAll()'])
    assert.ok(setup.includes(s),s);
  assert.match(setup, /Qt.ImhSensitiveData/);
  assert.match(shell, /presentationBridge.beginNativeRecoverySetup\(recoveryWords\)/);
});

test('legal consent and guest access are respected', () => {
  assert.match(setup, /if \(!consentChecked\) return/);
  assert.match(setup, /acceptLegalConsent\(true, true, true\)/);
  assert.match(setup, /if \(currentStep === 2\) return consentChecked/);
  assert.match(setup, /root.guestSelected = true/);
  assert.match(setup, /visible: root.browserReadOnly/);
  assert.match(setup, /finishSetup\(\)/);
});

test('new-account registration is muted, clearly unavailable, and never clickable', () => {
  const account=setup.slice(setup.indexOf('id: accountStage'),setup.indexOf('id: signInStage'));
  const create=account.slice(account.indexOf('objectName: "setupCreateAccountUnavailable"'),
                             account.indexOf('title: qsTrId("genycaster.setup.design_existing_account")'));
  const card=source('ui/components/SetupActionCard.qml');
  assert.match(create,/accountShowcase: true/);
  assert.match(create,/unavailable: true/);
  assert.match(create,/enabled: false/);
  assert.doesNotMatch(create,/setupEmphasis: true|onClicked:|root.openRegistration/);
  assert.match(card,/property bool unavailable: false/);
  assert.match(card,/root.unavailable \? \(Theme.dark/);
  assert.match(card,/id: unavailableBadge/);
  assert.match(card,/text: qsTrId\("genycaster.setup.temporarily_unavailable"\)/);
  assert.match(card,/x: Theme.rtl \? 0 : parent.width - width/);
  assert.match(account,/root.accountIntent = "existing"/);
  assert.match(account,/root.switchStep\(4\)/);
  for(const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
    const translation=source('ui/i18n/qml_'+locale+'.ts');
    assert.match(translation,/<message id="genycaster.setup.temporarily_unavailable">/);
    assert.match(translation,/<source>Temporarily unavailable<\/source>\s*<translation>[^<]+<\/translation>/);
  }
  assert.match(source('ui/i18n/qml_fa_IR.ts'),/<translation>فعلاً غیرفعال<\/translation>/);
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
  assert.match(setup,/Accessible\.role: Accessible\.CheckBox/);
  assert.match(setup,/fillColor: revealWords\.checked \? Theme\.ink : Theme\.surface/);
  assert.doesNotMatch(setup,/id: recoveryField|CheckBox \{/);
  assert.match(bridge,/const auto exactly24Words = \[\]/);
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
  for(const token of ['Theme.accent','Theme.accentInk','Theme.ink',
                       'appearanceController.setThemeMode','Theme.ink'])
    assert.ok(setup.includes(token));
  assert.match(setup, /model: root.greetings/);
});

test('mobile has a bottom CTA, desktop a two-pane welcome', () => {
  assert.match(setup, /id: stageScroll/);
  assert.match(setup, /id: stageAction/);
  assert.match(setup, /id: desktopBrandPane/);
  assert.match(setup, /width: Math.min\(parent.width \* 0.29, 430\)/);
  assert.match(setup, /anchors.bottom: parent.bottom/);
  assert.match(setup, /onClicked: root.previousStep\(\)/);
});

test('recovery phrase stays in the correct platform UI until verification really starts', () => {
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const dialog=source('ui/wasm/recovery-phrase-dialog.mjs');
  assert.match(setup,/if \(root\.browserReadOnly\) \{/);
  assert.match(setup,/root\.recoverAccount\(\)/);
  assert.match(setup,/authenticationRoute === "recovery" && browserReadOnly && !authenticated/);
  assert.match(setup,/root\.nativeRecoverAccount\(words\)/);
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
  // Legal content stays in-app; the independent company copyright link is external.
  const legal = setup.slice(setup.indexOf('id: legalReader'), setup.indexOf('id: setupCanvas'))
    + setup.slice(setup.indexOf('id: termsStage'), setup.indexOf('id: accountStage'));
  assert.doesNotMatch(legal,/Qt\.openUrlExternally/);
});

test('full-window setup removes shell chrome and has no fake desktop header or footer', () => {
  const s=source('ui/components/InitialSetup.qml');
  const app=source('ui/components/AppShell.qml');
  assert.match(s,/id: setupCanvas/);
  assert.match(s,/id: desktopBrandPane/);
  assert.match(s,/width: Math\.min\(parent\.width \* 0\.29, 430\)/);
  assert.match(s,/id: stageAction/);
  assert.match(s,/anchors\.bottom: parent\.bottom/);
  assert.doesNotMatch(s,/id: setupFooter/);
  assert.match(app,/id: shellContent[\s\S]*?visible: !root\.initialSetupOpen/);
  assert.match(app,/id: globalStatus[\s\S]*?visible: !root\.mobile && !root\.initialSetupOpen/);
});

test('welcome uses the same real eight-stage indicator as the other setup stages', () => {
  const s=source('ui/components/InitialSetup.qml');
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
  const s=source('ui/components/InitialSetup.qml');
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
  const s=source('ui/components/InitialSetup.qml');
  const header=source('ui/components/SetupPageHeader.qml');
  const card=source('ui/components/SetupActionCard.qml');
  assert.match(s,/id: desktopBrandPane[\s\S]*?visible: !root.phone && root.width >= 980/);
  assert.doesNotMatch(s,/visible: !root.phone && root.width >= 980 && root.currentStep === 0/);
  assert.match(s,/id: desktopBrandArt[\s\S]*?fragmentShader: "qrc:\/qt\/qml\/Farcaster\/Client\/shaders\/setup_glass_g\.frag\.qsb"/);
  assert.match(s,/ShaderEffect \{/);
  assert.match(s,/property vector2d paneSize/);
  assert.match(s,/property real darkMode: Theme\.dark \? 1\.0 : 0\.0/);
  assert.match(s,/id: desktopBrandMetadata/);
  assert.match(s,/anchors\.bottom: parent\.bottom/);
  assert.match(s,/Qt\.application\.version \|\| "1\.0\.0"/);
  assert.match(s,/new Date\(\)\.getFullYear\(\)/);
  assert.doesNotMatch(s,/source: .*setup-motif\.svg/);
  assert.match(s,/root.currentStep === 3 \|\| root.currentStep === 6/);
  assert.match(header,/Theme.rtl \? Qt.AlignRight : Qt.AlignLeft/);
  assert.match(s,/iconGlyph: Theme.iconPalette/);
  assert.match(s,/brandImageSource: "qrc:\/qt\/qml\/Farcaster\/Client\/wasm\/genycaster-icon.svg"/);
  assert.match(card,/property url brandImageSource/);
});

test('signout returns immediately to Welcome and legal consent accepts a deliberate tap', () => {
  const s=source('ui/components/InitialSetup.qml');
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
  const s=source('ui/components/InitialSetup.qml');
  const nav=s.slice(s.indexOf('        ActionButton {\n            z: 20'),s.indexOf('        Flickable {\n            id: stageScroll'));
  assert.match(nav,/x: Theme\.rtl \? root\.width - width -/);
  assert.match(nav,/x: Theme\.rtl \? \(desktopBrandPane\.visible \? desktopBrandPane\.width \+ Theme\.space5/);
  assert.match(nav,/x: Theme\.rtl \? root\.width - width -/);
  assert.match(nav,/iconGlyph: Theme\.rtl \? Theme\.iconArrowRight : Theme\.iconArrowLeft/);
});

test('acceptance toggle is borderless unless keyboard focused', () => {
  const s=source('ui/components/InitialSetup.qml');
  const start=s.indexOf('id: consentCheck');
  const end=s.indexOf('id: accountStage');
  const terms=s.slice(start,end);
  assert.match(terms,/borderWidth: Theme\.keyboardFocusVisible\(consentCheck\) \? 2 : 0/);
  assert.match(terms,/fillColor: consentCheck\.down \? Theme\.surfacePressed/);
  assert.match(terms,/onClicked: root\.consentChecked = !root\.consentChecked/);
});

test('desktop account is a balanced editorial dark/light pair, with no fake new signup', () => {
  const s=source('ui/components/InitialSetup.qml');
  const card=source('ui/components/SetupActionCard.qml');
  const account=s.slice(s.indexOf('id: accountStage'),s.indexOf('id: signInStage'));
  assert.match(account,/accountShowcase: true[\s\S]*?unavailable: true/);
  assert.match(account,/objectName: "setupCreateAccountUnavailable"/);
  assert.match(account,/iconGlyph: Theme\.iconUser/);
  assert.match(account,/enabled: false/);
  assert.doesNotMatch(account,/setupEmphasis: true/);
  assert.match(account,/onClicked: \{\s*root\.accountIntent = "existing"/);
  assert.match(account,/iconGlyph: Theme\.iconLink/);
  assert.match(card,/property bool accountShowcase: false/);
  assert.match(card,/property bool setupEmphasis: false/);
  assert.match(card,/id: editorialAccountCard/);
  assert.match(card,/LayoutMirroring\.enabled: false/);
  assert.match(card,/x: Theme\.rtl \? parent\.width - width : 0/);
});

test('shared marker geometry maintains a common vertical center for pill and eight dots', () => {
  const s=source('ui/components/InitialSetup.qml');
  const component=source('ui/components/SetupStepIndicator.qml');
  const manifest=source('ui/CMakeLists.txt');
  assert.match(manifest,/components\/SetupStepIndicator\.qml/);
  assert.match(s,/id: setupProgress/);
  assert.match(s,/readonly property int progressCount: wizardSteps\.length/);
  assert.match(component,/model: root\.safeCount/);
  assert.match(component,/readonly property int safeCount: Math\.max\(1, stepCount\)/);
  assert.match(component,/readonly property int activeIndex: Math\.max\(0, Math\.min\(safeCount - 1, currentStep\)\)/);
  assert.match(component,/anchors\.centerIn: parent/);
  assert.match(component,/height: 16/);
  assert.match(component,/width: marker\.active \? 22 : 6/);
  assert.match(component,/height: marker\.active \? 4 : 6/);
  assert.match(component,/Behavior on color/);
  assert.match(component,/Theme\.reduceMotion/);
});

test('browser recovery occupies the native Setup pane without exposing secrets to QML', () => {
  const dialog=source('ui/wasm/recovery-phrase-dialog.mjs');
  assert.match(dialog,/left:min\(29vw,430px\)/);
  assert.match(dialog,/role','region'/);
  assert.match(dialog,/grid-template-columns:repeat\(4,minmax\(0,1fr\)\)/);
  assert.match(dialog,/grid-template-columns:repeat\(2,minmax\(0,1fr\)\)/);
  assert.match(dialog,/genycaster-recovery-reveal:focus-visible/);
  assert.doesNotMatch(dialog,/aria-modal|backdrop-filter:blur\(7px\)/);
  assert.match(dialog,/word\.autocomplete='off'/);
  assert.doesNotMatch(dialog,/localStorage\.setItem/);
});

test('completion badge is a flat accent check without glow or shadow', () => {
  const s=source('ui/components/InitialSetup.qml');
  const complete=s.slice(s.indexOf('id: completeStage'));
  assert.match(complete,/id: completionBadge/);
  assert.match(complete,/fillColor: Theme\.accent/);
  assert.doesNotMatch(complete,/layer\.effect:|shadowEnabled:|shadowColor:|shadowBlur:|MultiEffect/);
  assert.doesNotMatch(s,/import QtQuick\.Effects/);
});

test('recovery appears before Genyleap and Farcaster, stays usable in review mode', () => {
  const s=source('ui/components/InitialSetup.qml');
  const stage=s.slice(s.indexOf('id: signInStage'),s.indexOf('id: verifyingStage'));
  const recovery=stage.indexOf('genycaster.setup.design_recovery_title');
  const genyleap=stage.indexOf('genycaster.shell.continue_with_genyleap');
  const farcaster=stage.indexOf('genycaster.shell.sign_in_with_farcaster');
  assert.ok(recovery>=0 && recovery<genyleap && genyleap<farcaster);
  assert.match(stage,/setupEmphasis: true/);
  assert.match(stage,/root\.recoverAccount\(\)/);
  assert.match(stage,/root\.showRecovery = !root\.showRecovery/);
  assert.ok(stage.indexOf('visible: root.showRecovery')<genyleap,
    'Native recovery phrase field must expand immediately after the first card');
  const bridge=source('src/presentation/qt/presentation_bridge.cpp');
  const handler=bridge.slice(bridge.indexOf('void PresentationBridge::beginRecoverySetup()'),
     bridge.indexOf('void PresentationBridge::beginNativeRecoverySetup('));
  assert.doesNotMatch(handler,/!initialSetupRequired\(\)/);
  assert.match(handler,/signInRequested\(QStringLiteral\("recovery"\)\)/);
  assert.match(handler,/applyAuthenticationError/);
});

test('copyright and Beta version remain readable at the bottom of the desktop left sidebar', () => {
  const s=source('ui/components/InitialSetup.qml');
  const pane=s.slice(s.indexOf('id: desktopBrandPane'), s.indexOf('id: setupProgress'));
  assert.match(pane,/visible: !root\.phone && root\.width >= 980/);
  assert.match(pane,/clip: true\s+\/\/ Keep this pane[\s\S]*?z: 2/);
  assert.match(pane,/id: desktopBrandMetadata/);
  assert.match(pane,/anchors\.bottom: parent\.bottom/);
  assert.match(pane,/anchors\.left: parent\.left/);
  assert.match(pane,/anchors\.right: parent\.right/);
  assert.match(pane,/visible: desktopBrandPane\.visible/);
  assert.match(pane,/new Date\(\)\.getFullYear\(\)/);
  assert.match(pane,/Beta 1\.0  ·  v/);
  assert.match(pane,/Qt\.application\.version/);
  assert.match(pane,/Theme\.dark \? "#E2E3F2" : "#4B4E6C"/);
  assert.match(pane,/id: genyleapCopyrightLink/);
  assert.match(pane,/text: "© " \+ new Date\(\)\.getFullYear\(\) \+ " Genyleap"/);
  assert.match(pane,/Accessible\.role: Accessible\.Link/);
  assert.match(pane,/activeFocusOnTab: true/);
  assert.match(pane,/Keys\.onReturnPressed: Qt\.openUrlExternally\("https:\/\/genyleap\.com\/"\)/);
  assert.match(pane,/cursorShape: Qt\.PointingHandCursor/);
  assert.match(pane,/onClicked: Qt\.openUrlExternally\("https:\/\/genyleap\.com\/"\)/);
  assert.doesNotMatch(pane,/" GenyCaster"/);
});

test('Setup GPU-rendered volumetric G is cross-platform, theme-aware and interactive',()=>{
  const s=source('ui/components/InitialSetup.qml');
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

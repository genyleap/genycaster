import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=p=>readFileSync(p,'utf8');
const shell=read('ui/components/shell/AppShell.qml');
const modal=read('ui/components/controls/GenyModal.qml');
const reaction=read('ui/components/feed/EngagementBar.qml');
const button=read('ui/components/feed/EngagementButton.qml');
const embed=read('ui/components/media/EmbedPreview.qml');
const link=read('ui/components/media/LinkPreview.qml');

test('native signers show all 24 slots with no nested scrolling on desktop',()=>{
  const dialog=shell.slice(shell.indexOf('id: interactionAuthorizationDialog'),
    shell.indexOf('GenyProDetailsDrawer {',shell.indexOf('id: interactionAuthorizationDialog')));
  assert.match(dialog,/width: fullScreen \? \(parent \? parent.width : root.width\)\s*: \(parent \? Math.min\(760,/);
  assert.match(dialog,/id: nativeRecoveryScroll\s*objectName: "nativeSignerRecoveryGrid"/);
  assert.match(dialog,/Layout\.preferredHeight: nativeSignerWordColumns\.implicitHeight/);
  assert.match(dialog,/columns: !nativeRecoveryScroll \|\| nativeRecoveryScroll\.width < Theme\.scaleUi\(340\) \? 3 : 4/);
  assert.match(dialog,/model: 24/);
  const grid=dialog.slice(dialog.indexOf('id: nativeRecoveryScroll'),
    dialog.indexOf('ActionButton {\n                visible: (nativeRecoveryScroll'));
  assert.doesNotMatch(grid,/ScrollView \{/);
  assert.doesNotMatch(grid,/Layout\.maximumHeight:/);
  assert.match(grid,/Layout\.preferredHeight: root\.compact \? Theme\.scaleUi\(38\) : Theme\.scaleUi\(44\)/);
});

test('mobile and narrow desktop signer recovery is a full-height bottom entrance',()=>{
  const dialog=shell.slice(shell.indexOf('id: interactionAuthorizationDialog'),
    shell.indexOf('GenyProDetailsDrawer {',shell.indexOf('id: interactionAuthorizationDialog')));
  assert.match(dialog,/fullScreen: root\.compact/);
  assert.match(dialog,/slideFromBottom: fullScreen/);
  assert.match(dialog,/height: fullScreen \? \(parent \? parent.height : root.height\)/);
  assert.match(dialog,/readonly property bool mobileRecoveryInput: Qt\.platform\.os === "android"/);
  assert.match(modal,/property bool slideFromBottom: false/);
  assert.match(modal,/property: "y"[\s\S]*?Theme\.motionStandard/);
  assert.match(modal,/curveDepth: control\.fullScreen \? 0 : Theme\.curveModal/);
});

test('signer authorization always offers an action instead of disabling an empty form',()=>{
  const dialog=shell.slice(shell.indexOf('id: interactionAuthorizationDialog'),
    shell.indexOf('GenyProDetailsDrawer {',shell.indexOf('id: interactionAuthorizationDialog')));
  const action=dialog.slice(dialog.indexOf('objectName: "authorizeSelectedProtocolSigner"'));
  assert.match(action,/enabled: !presentationBridge\.signerBusy/);
  assert.doesNotMatch(action,/enabled: !presentationBridge\.signerBusy\s*&&/);
  assert.match(action,/if \(interactionAuthorizationDialog\.enteringNativeWords\s*&& !interactionAuthorizationDialog\.recoveryReady\(\)\)/);
  assert.match(action,/recoveryValidationVisible = true/);
  assert.match(dialog,/objectName: "signerRecoveryValidationHint"/);
  assert.match(action,/if \(interactionAuthorizationDialog\.authorizationSubmitted\) \{\s*presentationBridge\.refreshSigner\(\)/);
  assert.match(dialog,/objectName: "signerReenterRecoveryWords"/);
  assert.match(dialog,/authorizationSubmitted = false/);
  assert.match(action,/busy: interactionAuthorizationDialog\.authorizationSubmitted\s*&& presentationBridge\.signerBusy/);
  assert.doesNotMatch(dialog,/signerAuthorizationNeonProgress/);
});

test('closing a word editor cannot dereference a destroyed popup from delegate bindings',()=>{
  const grid=shell.slice(shell.indexOf('id: nativeRecoveryScroll'),
    shell.indexOf('objectName: "signerAuthorizationInlineError"'));
  assert.match(grid,/enabled: !interactionAuthorizationDialog\s*\|\| !interactionAuthorizationDialog\.authorizationSubmitted/);
  assert.match(grid,/placeholderText: interactionAuthorizationDialog\s*&& interactionAuthorizationDialog\.authorizationSubmitted/);
  assert.match(grid,/if \(!interactionAuthorizationDialog\) return/);
  assert.match(grid,/if \(!interactionAuthorizationDialog\s*\|\| !interactionAuthorizationDialog\.opened/);
});

test('heart jumps before filling, rebounds, and respects Reduce Motion',()=>{
  assert.match(button,/function playReactionMotion\(kind, activating\)/);
  assert.match(button,/if \(Theme\.reduceMotion \|\| !control\.visible\) return/);
  assert.match(button,/heartFillHeld = true\s*heartHop\.start\(\)/);
  assert.match(button,/filled: control\.iconFilled && !control\.heartFillHeld/);
  const heart=button.slice(button.indexOf('id: heartHop'),button.indexOf('id: subtlePulse'));
  assert.match(heart,/property: "y"; from: 0; to: -7/);
  assert.match(heart,/property: "scale"; from: 1; to: 1\.32/);
  assert.match(heart,/ScriptAction \{ script: control\.heartFillHeld = false \}/);
  assert.match(heart,/property: "scale"; to: 0\.90/);
  assert.match(heart,/property: "scale"; to: 1\.09/);
  assert.match(heart,/property: "scale"; to: 1/);
  assert.match(button,/function resetReactionMotion\(\)/);
  assert.doesNotMatch(button,/onActiveChanged:.*playReactionMotion/);
});

test('animations only follow permitted user clicks, not network hydration or recycled rows',()=>{
  assert.match(reaction,/onReactionCastIdChanged: \{[\s\S]*?likeButton\.resetReactionMotion\(\)/);
  assert.match(reaction,/if \(!root\.likeInteractive\) \{[\s\S]*?root\.authenticationRequested\(\)[\s\S]*?return[\s\S]*?likeButton\.playReactionMotion\("like", next\)/);
  assert.match(reaction,/recastButton\.playReactionMotion\("recast", next\)/);
  assert.match(reaction,/replyButton\.playReactionMotion\("reply", true\)/);
  assert.match(reaction,/quoteButton\.playReactionMotion\("quote", true\)/);
  assert.match(reaction,/bookmarkButton\.playReactionMotion\("bookmark", adding\)/);
  assert.match(reaction,/castShareButton\.playReactionMotion\("share", true\)/);
  assert.match(button,/id: subtlePulse/);
  assert.match(button,/property: "scale"; from: 1; to: 1\.12/);
});

test('nested LinkPreview incubators cannot race destroyed feed delegate contexts',()=>{
  assert.match(embed,/asynchronous: !root\.magazineCard && root\.resolvedKind !== "link"/);
  assert.match(link,/id: webPreviewLoader[\s\S]*?asynchronous: false/);
});

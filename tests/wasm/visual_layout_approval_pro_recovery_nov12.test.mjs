import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = relative => readFileSync(new URL('../../' + relative, import.meta.url), 'utf8');

test('notification groups retain a visible interactive +N when every actor is hydrated', () => {
  const src=read('ui/components/feed/NotificationSurface.qml');
  assert.match(src,/readonly property int maximumPreview: root\.compact \? 4 : 7/);
  assert.match(src,/notificationRow\.groupTotal - 1/);
  assert.match(src,/visible: notificationRow\.groupedReaction/);
  assert.match(src,/readonly property int additionalCount: Math\.max\(0, groupCount - actorStack\.count\)/);
  assert.match(src,/onTapped: root\.inspectActors\(notificationRow\.modelData\)/);
  assert.match(src,/Accessible\.onPressAction: root\.inspectActors\(notificationRow\.modelData\)/);
  assert.match(src,/id: reactionActorsList/);
  assert.match(src,/root\.expandedActorCount > root\.expandedActors\.length/);
});

test('channel management is content sized and scroll content cannot exceed modal width',()=>{
  const surface=read('ui/components/social/ChannelsSurface.qml');
  const panel=read('ui/components/social/ChannelManagementPanel.qml');
  assert.match(surface,/channelManager\.preferredDialogHeight/);
  assert.match(surface,/root\.compact \? 590 : 620/);
  assert.match(panel,/root\.activeTab === "invites" \? 355 : 335/);
  assert.match(panel,/contentWidth: availableWidth/);
  assert.match(panel,/channelDetailsScroll\.availableWidth - Theme\.space2/);
  assert.match(panel,/Math\.min\(260, channelDetailsScroll\.availableWidth - Theme\.space4\)/);
  assert.match(panel,/visible: \(root\.activeTab === "moderate" \|\| root\.activeTab === "invites"\)/);
});

test('Pro authorization recovers a rejected bearer before treating status as unavailable',()=>{
  const cpp=read('ui/app/identity/connections.cpp');
  assert.match(cpp,/void OpenProofConnectionsController::retryProEntitlements\(\)/);
  assert.match(cpp,/!m_cloudSessionVerified && !m_accountBearer\.isEmpty\(\)/);
  assert.match(cpp,/bootstrapAccountSession\(\)/);
  assert.match(cpp,/status == 401 && m_cloudSessionVerified/);
  assert.match(cpp,/m_cloudCsrf\.clear\(\)/);
  assert.match(cpp,/account_signer_unavailable/);
  const drawer=read('ui/components/profile/GenyProDetailsDrawer.qml');
  assert.match(drawer,/signal signerRequested\(\)/);
  assert.match(drawer,/root\.signerRequested\(\)/);
  assert.match(drawer,/objectName: "genyProPreviewBenefits"/);
  assert.match(drawer,/root\.checkoutActive \? parent\.height/);
  assert.match(drawer,/qsTrId\("genycaster\.pro\.check_status"\)/);
});

test('external Farcaster signer approval is explained before it opens',()=>{
  const shell=read('ui/components/shell/AppShell.qml');
  const setup=read('ui/components/setup/InitialSetup.qml');
  assert.match(shell,/objectName: "interactionAuthorizationExplanation"/);
  assert.match(shell,/function requestInteractionAuthorization\(\)/);
  assert.match(shell,/onAuthorizeSigner: root\.requestInteractionAuthorization\(\)/);
  assert.match(shell,/onSignerRequested: \{/);
  assert.match(shell,/interactionAuthorizationDialog\.close\(\)\s*presentationBridge\.provisionSigner\(\)/);
  assert.match(setup,/subtitle: qsTrId\("genycaster\.setup\.recovery_signer_consent"\)/);
});

test('desktop setup reserves a non-scrolling progress header',()=>{
  const setup=read('ui/components/setup/InitialSetup.qml');
  assert.match(setup,/id: setupProgress[\s\S]*?anchors\.topMargin: 44/);
  assert.match(setup,/anchors\.topMargin: !root\.phone && root\.currentStep > 0 \? Theme\.scaleUi\(84\) : 0/);
  assert.match(setup,/y: !root\.phone && root\.currentStep > 0 && root\.currentStep < 7/);
});

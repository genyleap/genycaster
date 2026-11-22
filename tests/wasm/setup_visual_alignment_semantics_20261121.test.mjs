import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');
const setup = read('ui/components/setup/InitialSetup.qml');
const card = read('ui/components/setup/SetupActionCard.qml');
const cta = read('ui/components/setup/SetupPrimaryButton.qml');

test('connected color is semantic, latency is an independent graded metric', () => {
  assert.match(card, /readonly property color connectionStatusColor:/);
  assert.match(card, /root\.connectionState === "reachable" \? Theme\.success : Theme\.inkMuted/);
  assert.match(card, /readonly property color latencyQualityColor:/);
  assert.match(card, /root\.latencyMs <= 300 \? Theme\.success/);
  assert.match(card, /root\.latencyMs <= 800 \? Theme\.warning : Theme\.danger/);
  assert.match(card, /fillColor: root\.connectionStatusColor/);
  assert.match(card, /color: root\.connectionStatusColor/);
  assert.match(card, /fillColor: Qt\.rgba\(root\.latencyQualityColor\.r/);
  assert.match(setup, /nodeState === "reachable" \? Theme\.success : Theme\.inkMuted/);
  assert.match(setup, /readonly property color latencyColor:/);
  assert.match(setup, /color: route\.latencyColor/);
  assert.match(setup, /return state === "reachable" \? qsTrId\("genycaster.connections.status_connected"\)/);
});

test('protocol cards retain the same status anatomy on create and existing screens', () => {
  for (const id of ['setupRegisterHyperSnap','setupRegisterNeynar',
                    'setupExistingFarcaster','setupExistingHyperSnap']) {
    const start=setup.indexOf('objectName: "' + id + '"');
    assert.ok(start >= 0,id);
    assert.match(setup.slice(start,start + 1250),/latencyMs: root\.protocolLatencyMs\((true|false)\)/,id);
  }
  assert.match(setup,/root\.currentStep === 4 \? \(root\.showRecovery \|\| root\.resumingRegistration \? 640 : 760\)/);
  assert.match(setup,/root\.currentStep === 3 && root\.choosingRegistrationProvider \? 760/);
  assert.match(setup,/text: qsTrId\("genycaster.setup.registration_sync_paused"\)/);
});

test('primary CTA reserves equal icon and arrow slots and centers its label', () => {
  assert.match(cta,/contentItem: RowLayout/);
  assert.equal((cta.match(/Layout\.preferredWidth: Theme\.iconSizeSmall/g) || []).length,2);
  assert.match(cta,/Layout\.fillWidth: true\s+Layout\.minimumWidth: 0\s+text: root\.text/);
  assert.match(cta,/horizontalAlignment: Text\.AlignHCenter/);
  assert.match(cta,/visible: root\.showArrow && !root\.busy/);
  assert.match(setup,/readonly property real welcomeRowWidth: root\.phone \? width/);
  assert.match(setup,/: 0\)\s+: Theme\.rtl \? 0 : setupFooterContent\.width - width/);
  assert.match(setup,/anchors\.fill: parent[\s\S]*?anchors\.leftMargin: root\.phone \? Theme\.space4 : root\.setupGutter/);
  assert.match(setup,/id: setupActionFooter/);
  assert.match(setup,/anchors.bottom: setupActionFooter.top/);
  assert.match(setup,/readonly property bool showsPrimaryCta: true/);
  assert.match(setup,/layoutDirection: Theme\.uiDirection[\s\S]*?visible: nativeRegistrationPublisher\.pending/);
});

test('pending status is compact and actions remain safe and usable', () => {
  assert.match(setup,/GridLayout \{\s*Layout\.fillWidth: true\s*columns: root\.phone \? 1 : 3/);
  assert.match(setup,/index === 1 \? Theme\.accent : Theme\.borderMuted/);
  assert.match(setup,/objectName: "setupRefreshPausedRegistration"/);
  assert.match(setup,/nativeRegistrationPublisher\.checkConfirmation\(\)/);
  assert.match(setup,/objectName: "setupCancelPausedRegistration"/);
  assert.match(setup,/objectName: "setupNativeDeveloperDiagnostics"/);
  assert.match(setup,/fillColor: Theme\.surfaceMuted/);
  assert.match(setup,/nativeRegistrationStatus\.syncedForRegistration\s+compact: true\s+quiet: true/);
  assert.doesNotMatch(setup,/#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})\b/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');
const home = read('ui/components/home/HomeDashboard.qml');
const stats = read('ui/components/home/StatsPanel.qml');
const composer = read('ui/components/composer/ComposerDrawer.qml');
const shell = read('ui/components/shell/AppShell.qml');
const pro = read('ui/components/profile/GenyProDetailsDrawer.qml');
const bridge = read('src/presentation/qt/presentation_bridge.cpp');
const browserBridge = read('ui/app/browser/signer_bridge.hpp');

test('mobile composer expands long reports instead of clipping them at a fixed keyboard height', () => {
  assert.match(composer, /mobilePlainEditor\.contentHeight\s*\+ Theme\.space4 \* 2/);
  assert.match(composer, /Math\.max\(300, composerScroll\.height \* 1\.5\)/);
  assert.match(composer, /root\.ensureEditorVisible\(mobilePlainEditor\)/);
});

test('unknown or Basic subscriptions do not fetch premium personal statistics or Market', () => {
  assert.match(home, /Number\(openProofConnections\.activeAccountFid\) === Number\(accountFid\)/);
  assert.match(home, /proMembership\.status === "verified"/);
  assert.match(home, /proMembership\.tier === "pro" && proMembership\.eligible === true/);
  assert.match(home, /!root\.verifiedGenyPro[\s\S]*?statsController\.clear\(\)/);
  assert.equal((home.match(/proUnlocked: root\.verifiedGenyPro/g) || []).length, 2);
  assert.match(stats, /objectName: "statsProUpgradeGate"/);
  assert.match(stats, /objectName: "statsUnlockProButton"/);
  assert.match(stats, /visible: root\.proUnlocked && root\.section === "market"/);
  assert.match(stats, /root\.tabActive && root\.proUnlocked/);
  assert.match(stats, /enabled: root\.proUnlocked && root\.loaded/);
  assert.match(shell, /onProRequested: genyProDetailsDrawer\.open\(\)/);
});

test('Pro checkout refresh is explicit; adaptive sheet gives every screen a close action', () => {
  assert.doesNotMatch(pro, /function onStateChanged\(\)[\s\S]*?fetchProEntitlements\(false\)/);
  assert.match(pro, /objectName: "genyProManualStatusRefresh"/);
  assert.match(pro, /onClicked:[\s\S]*?fetchProEntitlements\(true\)/);
  assert.match(pro, /fullScreen: root\.compact && root\.manualExpanded && root\.checkoutActive/);
  assert.match(pro, /height: parent \? \(root\.compact && !root\.fullScreen/);
  assert.match(pro, /: parent\.height\) : 640/);
  assert.match(pro, /iconGlyph: Theme\.iconClose[\s\S]*?onClicked: root\.cancelPayment\(\)/);
});

test('second recovery tap cannot replace pending browser challenge or produce a false error', () => {
  assert.match(bridge, /if \(authenticationBusy\(\)\) return;[\s\S]*?m_authenticationRoute = QStringLiteral\("recovery"\)/);
  assert.match(browserBridge, /if \(!m_pendingRecoveryId\.isEmpty\(\)\) return;/);
  assert.match(browserBridge, /m_pendingRecoveryId = QUuid::createUuid\(\)/);
});

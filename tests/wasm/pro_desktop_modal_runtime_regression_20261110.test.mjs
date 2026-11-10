import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('headless visual fixture explicitly exposes the actual app shell', () => {
  const fixture = read('src/presentation/qt/presentation_bridge.cpp')
    .split('void PresentationBridge::applyVerificationFixture()')[1]
    .split('void PresentationBridge::', 1)[0];
  assert.match(fixture, /m_authenticationStateObserved\s*=\s*true/);
  assert.match(fixture, /emit authenticationStateChanged\(\)/);
  const shell = read('ui/components/shell/AppShell.qml');
  assert.match(shell, /objectName:\s*"shellContent"/);
  assert.match(shell, /visible:\s*presentationBridge\.authenticationStateObserved\s*&&\s*!root\.initialSetupOpen/);
  const smoke = read('ui/app/diagnostics/smoke.cpp');
  assert.match(smoke, /findChild<QObject\*>\(QStringLiteral\("shellContent"\)\)/);
  assert.match(smoke, /->property\("visible"\)\.toBool\(\)/);
});

test('Pro side drawer must retain explicit Escape, outside and cancel paths without overlapping full-screen pointer grabs', () => {
  const base = read('ui/components/controls/GenyDrawer.qml');
  const backdrop = read('ui/components/visual/DrawerBackdrop.qml');
  const pro = read('ui/components/profile/GenyProDetailsDrawer.qml');
  assert.match(base, /Shortcut\s*\{[\s\S]*?sequence:\s*"Escape"/);
  assert.match(base, /Overlay\.modal:\s*DrawerBackdrop\s*\{/);
  const overlay = base.split('Overlay.modal: DrawerBackdrop {')[1].split('Overlay.modeless:')[0];
  assert.doesNotMatch(overlay, /MouseArea\s*\{/);
  assert.match(overlay, /onBackgroundTapped:\s*control\.close\(\)/);
  assert.match(backdrop, /outsideDismissConsumed/);
  assert.match(backdrop, /function requestOutsideDismiss\(\)/);
  assert.match(pro, /objectName:\s*"genyProCancelCheckout"/);
  assert.match(pro, /onClicked:\s*root\.cancelPayment\(\)/);
  assert.match(pro, /interactive:\s*false/);
  assert.match(pro, /edge:\s*root\.compact\s*\?\s*Qt\.BottomEdge\s*:\s*Qt\.RightEdge/);
  assert.match(pro, /height:\s*parent\s*\?\s*\(!root\.compact\s*\|\|\s*root\.fullScreen\s*\?\s*parent\.height/);
});

test('native visual diagnostic cycles the real Pro drawer without initiating billing', () => {
  const diagnostic = read('ui/app/diagnostics/snapshots.cpp');
  assert.match(diagnostic, /--exercise-pro-drawer/);
  assert.match(diagnostic, /genyProDetailsDrawer/);
  assert.match(diagnostic, /phase == 12/);
  assert.match(diagnostic, /Pro drawer: six open\/close cycles passed/);
  assert.doesNotMatch(diagnostic.split('--exercise-pro-drawer')[1].split('if (isVisualTest && !snapshotPath.isEmpty())')[0], /pay\(\)|requestProPaymentQuote/);
});

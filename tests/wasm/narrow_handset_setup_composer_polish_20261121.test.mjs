import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const profile = read('ui/components/profile/ProfileSurface.qml');
const shell = read('ui/components/shell/AppShell.qml');
const dock = read('ui/components/navigation/BottomNavigation.qml');
const composer = read('ui/components/composer/ComposerDrawer.qml');
const setup = read('ui/components/setup/InitialSetup.qml');
const splash = read('ui/wasm/genycaster-shell.html');

test('320–375dp profile tabs scroll instead of reducing five labels to ellipses', () => {
  assert.match(profile, /distribute: !root\.compact \|\| root\.profileContentWidth >= 530/);
  assert.match(profile, /minimumTabWidth: root\.compact \? Theme\.scaleUi\(64\) : 68/);
  assert.match(profile, /font\.pixelSize: root\.compact && root\.profileContentWidth < 390/);
  assert.match(profile, /root\.compact \? 0\.42 : 0\.48/);
  assert.match(profile, /root\.compact && root\.profileContentWidth < 390\s+\? "\+" \+ LocaleFormat\.formatInteger/);
  const strip = read('ui/components/controls/GenyTabStrip.qml');
  assert.match(strip, /orientation: ListView\.Horizontal/);
  assert.match(strip, /boundsBehavior: Flickable\.StopAtBounds/);
  assert.match(strip, /tabList\.positionViewAtIndex\(tabList\.currentIndex, ListView\.Contain\)/);
});

test('dock sizing uses logical width, avoids system gestures and reserves final-row clearance', () => {
  assert.match(dock, /readonly property bool smallHandset: width < 390/);
  assert.match(dock, /readonly property real gestureInset: Math\.max\(0,/);
  assert.match(dock, /root\.Window\.window\.SafeArea\.margins\.bottom/);
  assert.match(dock, /implicitHeight: \(Qt\.platform\.os === "android"/);
  assert.match(dock, /Layout\.preferredWidth: root\.smallHandset \? 48 : 52/);
  assert.match(shell, /id: mobileBottomNavigation[\s\S]*?height: implicitHeight/);
  for (const source of [profile, read('ui/components/feed/FeedSurface.qml')])
    assert.match(source, /bottomMargin: root\.compact\s+\? Math\.max\(96, root\.bottomOverlayInset \+ Theme\.space2\) : 0/);
});

test('mobile composer resolves full-screen Drawer geometry before opening and keeps footer tools visible', () => {
  assert.match(composer, /bottomSheet: true\s+fullScreen: root\.compact/);
  assert.match(composer, /height: parent \? \(root\.compact \? parent\.height/);
  assert.equal((shell.match(/composerDrawer\.compact = root\.mobile\s+composerDrawer\.open\(\)/g) || []).length, 2);
  assert.match(composer, /id: mobileComposerFooter[\s\S]*?Layout\.minimumHeight: implicitHeight/);
  assert.match(composer, /objectName: "mobileComposerMediaBar"/);
  assert.match(composer, /objectName: "mobileComposerGifAction"/);
  assert.match(composer, /qsTrId\("genycaster\.composer\.add_cast"\)/);
  assert.match(composer, /id: composerScroll[\s\S]*?Layout\.minimumHeight: 0/);
});

test('Setup Continue is inert until signer authorization completes, and BETA hugs wordmark corner', () => {
  assert.match(setup, /function nextStep\(\) \{[\s\S]*?signerBusy \|\| recoverySubmitPending \|\| authenticationInProgress/);
  assert.match(setup, /function actionEnabled\(\) \{[\s\S]*?signerBusy \|\| recoverySubmitPending \|\| authenticationInProgress/);
  assert.match(setup, /enabled: !root\.signerBusy && !root\.recoverySubmitPending\s+&& !root\.authenticationInProgress/);
  assert.match(setup, /if \(!root\.signerBusy && !root\.recoverySubmitPending/);
  assert.match(splash, /\.boot-wordmark-wrap\{position:relative/);
  assert.match(splash, /\.boot-beta\{position:absolute;top:-15px;right:0/);
  assert.match(splash, /<span class="boot-wordmark-wrap"><img class="boot-wordmark"/);
});

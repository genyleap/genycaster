import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const setup = read('ui/components/setup/InitialSetup.qml');
const indicator = read('ui/components/setup/SetupStepIndicator.qml');

function sectionAfter(anchor, end, file=setup) {
  const start=file.indexOf(anchor);
  assert.ok(start>=0, 'missing '+anchor);
  const stop=file.indexOf(end,start);
  assert.ok(stop>start,'missing closing region '+end);
  return file.slice(start,stop);
}

test('CTA is centered on mobile and fixed to the logical trailing footer corner on desktop',()=>{
  const footer=sectionAfter('id: setupActionFooter','id: welcomeStage');
  assert.match(footer,/anchors\.fill: parent\s+anchors\.leftMargin: root\.setupGutter\s+anchors\.rightMargin: root\.setupGutter/);
  assert.doesNotMatch(footer,/x: stagePanel\.x - setupActionFooter\.x/);
  assert.match(footer,/width: root\.phone\s+\? Math\.min\(Theme\.scaleUi\(264\), setupFooterContent\.width\)/);
  assert.match(footer,/x: root\.phone \? \(setupFooterContent\.width - width\) \/ 2\s+: Theme\.rtl \? 0 : setupFooterContent\.width - width/);
  assert.match(footer,/x: root\.phone \? \(setupFooterContent\.width - width\) \/ 2\s+: Theme\.rtl\s+\? stageAction\.x \+ stageAction\.width \+ Theme\.space2/);
  assert.match(footer,/y: root\.phone \? stageAction\.y \+ stageAction\.height \+ Theme\.space2/);
  assert.match(footer,/root\.phone && root\.welcomeNativeAppsVisible\s+\? stageAction\.implicitHeight \+ setupWelcomeNativeApps\.implicitHeight/);
  assert.match(footer,/anchors.bottom: parent.bottom/);
  assert.match(setup,/anchors.bottom: setupActionFooter.top/);
  assert.match(footer,/neutralPrimary: false/);
  assert.match(footer,/enabled: !root\.stageTransitioning && root\.actionEnabled\(\)/);
});

test('consent keeps label hierarchy and accessible tap target at narrow widths',()=>{
  const consent=sectionAfter('id: consentCheck','id: accountStage');
  assert.match(consent,/implicitHeight: Math\.max\(Theme\.touchTarget/);
  assert.match(consent,/font\.pixelSize: Theme\.typeLabelSize/);
  assert.doesNotMatch(consent,/font\.pixelSize: Theme\.typeBodySize/);
  assert.match(consent,/Layout\.preferredWidth: Theme\.scaleUi\(20\)/);
  assert.match(consent,/horizontalAlignment: Theme\.textStartAlignment/);
  assert.match(consent,/layoutDirection: Theme\.uiDirection/);
  assert.match(consent,/Accessible\.checked: root\.consentChecked/);
  assert.match(consent,/onClicked: root\.consentChecked = !root\.consentChecked/);
});

test('progress dots, consent, recovery and pending timeline mirror correctly',()=>{
  assert.match(indicator,/id: markers[\s\S]*?layoutDirection: Theme\.uiDirection/);
  assert.match(setup,/objectName: "setupBack"[\s\S]*?Theme\.rtl \? root\.width - width - Theme\.space2/);
  assert.match(setup,/iconGlyph: Theme\.rtl \? Theme\.iconArrowRight : Theme\.iconArrowLeft/);
  assert.match(setup,/columns: root\.phone \? 1 : 3\s+layoutDirection: Theme\.uiDirection/);
  assert.match(setup,/required property string modelData\s+Layout\.fillWidth: true\s+layoutDirection: Theme\.uiDirection/);
  assert.match(setup,/readonly property int setupGutter: phone \? 24/);
});

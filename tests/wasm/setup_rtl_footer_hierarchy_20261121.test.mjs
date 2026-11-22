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

test('CTA uses the full mobile footer and logical trailing desktop corner',()=>{
  const footer=sectionAfter('id: setupActionFooter','id: welcomeStage');
  assert.match(footer,/anchors\.fill: parent[\s\S]*?anchors\.leftMargin: root\.phone \? 0 : root\.setupGutter\s+anchors\.rightMargin: root\.phone \? 0 : root\.setupGutter/);
  assert.doesNotMatch(footer,/x: stagePanel\.x - setupActionFooter\.x/);
  for (const required of [
    'readonly property real welcomeRowWidth:',
    'setupFooterContent.welcomeRowWidth - Theme.space2',
    'setupFooterContent.welcomeRowWidth - width',
    'stageAction.x + stageAction.width + Theme.space2',
    'stageAction.x - Theme.space2 - width',
    'y: stageAction.y',
    'Math.max(stageAction.height, setupWelcomeNativeApps.implicitHeight)'
  ]) assert.ok(footer.includes(required),required);
  assert.ok(!footer.includes('stageAction.y + stageAction.height + Theme.space2'),
    'mobile welcome must stay in one horizontal row');
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

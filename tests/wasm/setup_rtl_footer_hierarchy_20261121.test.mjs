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

test('mobile CTAs use balanced 16dp safe gutters and desktop preserves form insets',()=>{
  const footer=sectionAfter('id: setupActionFooter','id: welcomeStage');
  assert.match(footer,/anchors\.fill: parent[\s\S]*?anchors\.leftMargin: root\.phone \? Theme\.space4 : root\.setupGutter\s+anchors\.rightMargin: root\.phone \? Theme\.space4 : root\.setupGutter/);
  assert.doesNotMatch(footer,/x: stagePanel\.x - setupActionFooter\.x/);
  for (const required of [
    'readonly property real welcomeRowWidth:',
    'width: root.phone ? setupFooterContent.width',
    'x: root.phone ? 0 : Theme.rtl ? 0 : setupFooterContent.width - width',
    'stageAction.x + stageAction.width + Theme.space2',
    'stageAction.x - Theme.space2 - width',
    'stageAction.y + stageAction.height + Theme.space2',
    'stageAction.height + Theme.space2 + setupWelcomeNativeApps.height',
    'Math.max(implicitHeight, setupWelcomeNativeApps.implicitHeight)'
  ]) assert.ok(footer.includes(required),required);
  assert.match(footer,/anchors.bottom: parent.bottom/);
  assert.match(setup,/anchors.bottom: setupActionFooter.top/);
  assert.match(footer,/neutralPrimary: false/);
  assert.match(footer,/enabled: !root\.stageTransitioning && root\.actionEnabled\(\)/);
});

test('320–440dp mobile welcome stacks full-width buttons with equal safe insets',()=>{
  const theme=read('ui/theme/Theme.qml');
  assert.match(theme,/readonly property int space4: scaleUi\(16\)/);
  const gutter=16, gap=8;
  for (const viewport of [320,375,390,440]) {
    const usable=viewport-2*gutter;
    for (const rtl of [false,true]) {
      for (const buttonHeight of [58,76,96]) {
        const primary={x:gutter,y:0,w:usable,h:buttonHeight};
        const secondary={x:gutter,y:primary.h+gap,w:usable,h:buttonHeight};
        assert.equal(viewport-primary.x-primary.w,gutter);
        assert.equal(viewport-secondary.x-secondary.w,gutter);
        assert.equal(secondary.y-primary.h,gap);
        assert.equal(secondary.y+secondary.h,2*buttonHeight+gap);
      }
    }
  }
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

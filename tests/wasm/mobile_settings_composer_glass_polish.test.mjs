import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml=name=>readFileSync('ui/components/'+name+'.qml','utf8');

test('settings keeps a compact mobile bottom sheet with scrollable sections',()=>{
  const settings=qml('LocaleSettingsPopup');
  assert.match(settings,/bottomSheet: mobilePresentation/);
  assert.match(settings,/interactive: false/);
  assert.match(settings,/height: parent \? \(mobilePresentation\s*\? Math\.min\(parent\.height \* 0\.91, Theme\.scaleUi\(790\)\)/);
  assert.doesNotMatch(settings,/height: parent \? parent\.height : 820/);
});

test('composer topic loading uses the GenyCaster spinner rather than native Qt busy control',()=>{
  const composer=qml('ComposerDrawer');
  const topic=composer.slice(composer.indexOf('id: topicPicker'),composer.indexOf('id: mediaFileDialog'));
  assert.match(topic,/GenySpinner \{\s*Layout\.alignment: Qt\.AlignHCenter\s*visible: openProofConnections\.topicsBusy\s*running: visible/);
  assert.doesNotMatch(topic,/BusyIndicator\s*\{/);
  assert.match(topic,/onClicked: root\.toggleTopic\(topicChip\.slug\)/);
});

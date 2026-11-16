import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const qml=name=>readFileSync('ui/components/' + componentPath(name)+'.qml','utf8');

test('mobile settings use fullscreen navigation and scrollable sections',()=>{
  const settings=qml('LocaleSettingsPopup');
  assert.match(settings,/GenyAdaptiveDialog \{/);
  assert.match(settings,/mobileFullScreen: true/);
  assert.match(settings,/height: parent \? \(mobilePresentation \? parent\.height/);
  const adaptive=qml('GenyAdaptiveDialog');
  assert.match(adaptive,/bottomSheet: true/);
  assert.match(adaptive,/interactive: !control\.mobileFullScreen/);
  assert.match(adaptive,/onOpened:[\s\S]*?control\.didOpen\(\)/);
});

test('composer topic loading uses the GenyCaster spinner rather than native Qt busy control',()=>{
  const composer=qml('ComposerDrawer');
  const topic=composer.slice(composer.indexOf('id: topicPicker'),composer.indexOf('id: mediaFileDialog'));
  assert.match(topic,/GenySpinner \{\s*Layout\.alignment: Qt\.AlignHCenter\s*visible: openProofConnections \? openProofConnections\.topicsBusy : false\s*running: visible/);
  assert.doesNotMatch(topic,/BusyIndicator\s*\{/);
  assert.match(topic,/onClicked: root\.toggleTopic\(topicChip\.slug\)/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const field=read('ui/components/controls/GenyMultiSelect.qml');
const profile=read('ui/components/profile/ProfileSurface.qml');
const manifest=read('ui/CMakeLists.txt');

test('profile roles and secondary categories use one multi-select field each',()=>{
  assert.match(manifest,/components\/controls\/GenyMultiSelect\.qml/);
  assert.match(profile,/id: secondaryPicker[\s\S]{0,800}maximumSelection: 3/);
  assert.match(profile,/id: professionEditor[\s\S]{0,420}maximumSelection: 5/);
  assert.match(profile,/selectedValues: profileExtrasEditor\.secondarySelection/);
  assert.match(profile,/selectedValues: profileExtrasEditor\.roleSelection/);
  assert.match(profile,/onSelectionEdited: nextValues => \{\s*profileExtrasEditor\.secondarySelection = nextValues/);
  assert.match(profile,/onSelectionEdited: nextValues => \{\s*profileExtrasEditor\.roleSelection = nextValues/);
  assert.doesNotMatch(profile,/profileExtrasEditor\.roleSelection\.concat/);
});

test('selections are visually distinct accent chips inside the field with overflow counter',()=>{
  assert.match(field,/readonly property var visibleValues/);
  assert.match(field,/readonly property int hiddenCount/);
  assert.match(field,/model: control\.visibleValues/);
  assert.match(field,/fillColor: Theme\.primarySurface/);
  assert.match(field,/borderColor: Theme\.accent/);
  assert.match(field,/text: "\+" \+ control\.hiddenCount/);
  assert.match(field,/onClicked: control\.toggleOption\(String\(modelData\)\)/);
});

test('touch-first choices have no search editor and never open the virtual keyboard',()=>{
  assert.doesNotMatch(field,/searchField|DirectionAwareTextField|forceActiveFocus|Qt\.Imh/);
  assert.match(field,/function openChoices\(\) \{[\s\S]*?picker\.open\(\)/);
  assert.match(field,/readonly property var availableOptions: options \|\| \[\]/);
  assert.match(field,/model: control\.availableOptions/);
  assert.match(field,/GridView \{/);
  assert.match(field,/cellWidth: Math\.max\(1, width \/ 2\)/);
  assert.match(field,/readonly property bool selected: control\.isSelected/);
  assert.match(field,/enabled: selected\s*\|\| control\.selectedValues\.length < control\.maximumSelection/);
  assert.match(field,/Accessible\.checked: selected/);
  assert.match(field,/onClicked: control\.toggleOption\(canonicalValue\)/);
  assert.match(field,/if \(existing >= 0\)\s*next\.splice\(existing, 1\)/);
  assert.match(field,/else if \(next\.length < maximumSelection\)\s*next\.push\(canonical\)/);
});

test('profile editor owns vertical touch scrolling instead of drawer drag gestures',()=>{
  const start=profile.indexOf('id: profileExtrasEditor');
  const end=profile.indexOf('id: profileLinksDrawer',start);
  const editor=profile.slice(start,end);
  assert.match(editor,/bottomSheet: mobilePresentation/);
  assert.match(editor,/interactive: false/);
  assert.match(editor,/id: profileEditorScroll/);
  assert.match(editor,/interactive: true/);
  assert.match(editor,/flickableDirection: Flickable\.VerticalFlick/);
  assert.match(editor,/contentHeight: formColumn\.implicitHeight \+ Theme\.space4/);
  assert.match(editor,/ScrollBar\.vertical: GenyScrollBar \{\}/);
  assert.match(editor,/onCloseRequested: profileExtrasEditor\.close\(\)/);
});

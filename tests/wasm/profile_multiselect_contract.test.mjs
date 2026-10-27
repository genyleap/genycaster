import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const field=read('ui/components/GenyMultiSelect.qml');
const profile=read('ui/components/ProfileSurface.qml');
const manifest=read('ui/CMakeLists.txt');

test('profile roles and secondary categories use one multi-select field each',()=>{
  assert.match(manifest,/components\/GenyMultiSelect\.qml/);
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

test('searchable two-column choices reflect selected status and remain removable at limit',()=>{
  assert.match(field,/id: searchField/);
  assert.match(field,/readonly property var filteredOptions/);
  assert.match(field,/toLocaleLowerCase\(\)/);
  assert.match(field,/GridView \{/);
  assert.match(field,/cellWidth: Math\.max\(1, width \/ 2\)/);
  assert.match(field,/readonly property bool selected: control\.isSelected/);
  assert.match(field,/enabled: selected\s*\|\| control\.selectedValues\.length < control\.maximumSelection/);
  assert.match(field,/Accessible\.checked: selected/);
  assert.match(field,/onClicked: control\.toggleOption\(canonicalValue\)/);
  assert.match(field,/if \(existing >= 0\)\s*next\.splice\(existing, 1\)/);
  assert.match(field,/else if \(next\.length < maximumSelection\)\s*next\.push\(canonical\)/);
});

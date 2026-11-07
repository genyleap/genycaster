import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const search=readFileSync(new URL('../../ui/components/navigation/SearchPalette.qml',import.meta.url),'utf8');
test('search input stays under heading; unused drawer height sits below the input',()=>{
  const start=search.indexOf('contentItem: ColumnLayout {');
  const header=search.indexOf('objectName: "closeSearchPalette"',start);
  const input=search.indexOf('id: searchField',start);
  const results=search.indexOf('id: resultViewport',start);
  const suggestions=search.indexOf('id: suggestionViewport',start);
  const spacer=search.indexOf('objectName: "searchEmptyBottomSpace"',start);
  assert.ok(start>=0 && header>start && input>header && results>input
            && suggestions>results && spacer>suggestions);
  const footer=search.slice(spacer);
  assert.match(footer,/Layout\.fillHeight: visible/);
  assert.match(footer,/visible: !resultViewport\.visible && !suggestionViewport\.visible/);
  assert.match(search,/root\.filteredCount > 0 \? Math\.min\(540, 224 \+ root\.filteredCount \* 86\)/);
  assert.match(search,/searchField\.text\.trim\(\)\.length < 2/);
});
test('search continues to use scope-aware results and keyboard shortcuts',()=>{
  assert.match(search,/readonly property string searchScope:/);
  assert.match(search,/onAccepted: root\.submit\(text\)/);
  assert.match(search,/signal suggestionsRequested\(string query\)/);
});

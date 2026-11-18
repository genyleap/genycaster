import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(path,'utf8');

test('VectorImage retains final visible frame and only pauses unfinished playback',()=>{
  const qml=read('ui/components/media/AnimatedEmojiLottie.qml');
  assert.match(qml,/animations\.loops: 1/);
  assert.match(qml,/animations\.paused: !root\.animationActive && !root\.playbackCompleted/);
  assert.doesNotMatch(qml,/animations\.restart\(\)/);
  assert.match(qml,/root\.timeline\.endFrame - 1/);
  assert.match(qml,/root\.timeline\.frameCounter = lastVisible/);
  assert.match(qml,/onSourceChanged: root\.cycleFinished = false/);
  for(const [active,completed,expected] of [
    [false,false,true], [true,false,false],
    [false,true,false], [true,true,false],
  ]) assert.equal(!active && !completed,expected);
});

test('UI and Explorer strict gates agree on bounded sparse prefetch budgets',()=>{
  const feed=read('ui/components/feed/FeedSurface.qml');
  const uiGate=read('ci/check_ui_design.py');
  const explorerGate=read('ci/check_explorer_semantic_contract.py');
  for(const [name,count] of [['filterPrefetchLimit',8],['automaticPrefetchLimit',16]]) {
    const definition=`readonly property int ${name}: ${count}`;
    assert.ok(feed.includes(definition),`FeedSurface ${name} drift`);
    assert.ok(uiGate.includes(definition),`UI gate ${name} drift`);
    assert.ok(explorerGate.includes(definition),`Explorer gate ${name} drift`);
  }
  assert.match(feed,/root\.filterPrefetchPages >= root\.filterPrefetchLimit/);
  assert.match(feed,/root\.automaticPrefetchPages >= root\.automaticPrefetchLimit/);
  assert.match(feed,/root\.resetCurrentFilterPrefetchBudget\(\)/);
  assert.match(feed,/root\.resetAutomaticPageBudget\(\)/);
  assert.match(feed,/readonly property int automaticNoProgressLimit: 2/);
});

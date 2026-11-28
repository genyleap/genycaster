import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const wall=read('ui/components/feed/FeedMasonryView.qml');
const cast=read('ui/components/feed/CastItem.qml');
const semantic=read('ui/components/feed/SemanticFeedItem.qml');
const surface=read('ui/components/feed/FeedSurface.qml');

test('Magazine measures the final painted engagement row through every Loader boundary',()=>{
  assert.match(cast,/readonly property real magazinePaintHeight:[\s\S]*?messageRow\.y \+ messageSurface\.y \+ messageContent\.y[\s\S]*?magazineActions\.y \+ magazineActions\.height/);
  assert.match(cast,/id: magazineActions\s+visible: !root\.mediaFirst \|\| root\.magazineCard/);
  assert.match(semantic,/magazinePaintHeight: root\.magazineCard && loader\.item[\s\S]*?loader\.item\.magazinePaintHeight/);
  assert.match(surface,/readonly property real resolvedHeight: item[\s\S]*?Math\.max\(0, Number\(item\.implicitHeight \|\| 0\),[\s\S]*?Number\(item\.magazinePaintHeight \|\| 0\)/);
});

test('Like/recast cannot immediately shrink a committed masonry lane during Qt Layout rearrangement',()=>{
  const match=wall.match(/            function recordHeight\(\) \{([\s\S]*?)\n            \}\n            Component\.onCompleted:/);
  assert.ok(match);
  const events=[];
  const context=vm.createContext({
    cardLoader: {item: {incubating:false,resolvedHeight:220}},
    measuredHeight: 0,cardHeight:520,revisionSettling:true,
    stableKey:'cast:liked',revisionKey:'reaction:liked',width:340,
    view:{retainHeight:(...args)=>events.push(args)}
  });
  vm.runInContext('function recordHeight(){'+match[1]+'\n}',context);
  vm.runInContext('recordHeight()',context);
  assert.equal(events.length,0,'transient short layout cannot make the next cast overlap');
  context.cardLoader.item.resolvedHeight=610;
  vm.runInContext('recordHeight()',context);
  assert.equal(events.at(-1)[3],610,'new rich content must grow the lane immediately');
  context.cardHeight=610;
  context.cardLoader.item.resolvedHeight=540;
  vm.runInContext('recordHeight()',context);
  assert.equal(events.at(-1)[3],610,'transient shrink cannot discard the grown card');
  context.revisionSettling=false;
  vm.runInContext('recordHeight()',context);
  assert.equal(events.at(-1)[3],540,'settled real shrink is eventually committed');
  assert.match(wall,/id: revisionSettleTimer\s+interval: 48[\s\S]*?slot\.revisionSettling = false\s+slot\.recordHeight\(\)/);
  assert.match(wall,/onRevisionKeyChanged: \{\s+revisionSettling = true\s+revisionSettleTimer\.restart\(\)/);
});

test('A delayed tall embed cannot paint over another card before native reflow catches up',()=>{
  assert.match(wall,/id: slot[\s\S]*?height: Math\.max\(1, cardHeight\)[\s\S]*?clip: true/);
  assert.match(wall,/function onResolvedHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall,/function onImplicitHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall,/if \(layoutModel\.retainHeight\(key, revision, width, height, true\)\)\s+scheduleLayout\(\)/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const wall = source('ui/components/feed/FeedMasonryView.qml');
const cast = source('ui/components/feed/CastItem.qml');
const toolbar = source('ui/components/feed/ExplorerLayoutControls.qml');
const explorer = source('ui/components/home/HomeFeedHeader.qml');
const profile = source('ui/components/profile/ProfileSurface.qml');

test('late media height changes shift only following cards in their stable lane', () => {
  const body = wall.match(/    function forceLayout\(\) \{([\s\S]*?)\n    \}\n\n    function positionViewAtBeginning/);
  assert.ok(body, 'forceLayout is present');
  const heights = [300, 120, 150, 420, 160, 180];
  const items = heights.map(height => ({
    height, assignedColumn: -1, assignmentColumns: 0,
    placed: false, x: 0, y: 0
  }));
  const context = vm.createContext({
    arranging: false, positioned: false, moving: false,
    width: 720, height: 640, columns: 2, gutter: 12,
    cardWidth: 342, leadingInset: 12, contentY: 0,
    contentHeight: 4000, rowsBottom: 0, relayoutPending: false,
    headerLoader: {height: 0}, layoutTimer: {stop() {}},
    slots: {count: items.length, itemAt: i => items[i]},
    layoutStarted() {}, layoutFinished() {}, scheduleAdmission() {}, scheduleLayout() {},
    indexAt: () => -1, Qt: {RightToLeft: 1}, layoutDirection: 0
  });
  vm.runInContext('function forceLayout() {' + body[1] + '\n}', context);

  const verify = () => {
    const perColumn = [[], []];
    for (const item of items) {
      assert.ok(item.placed, 'every slot is positioned');
      assert.ok(item.height > 0, 'slot has a positive reserved height');
      assert.ok(item.assignedColumn >= 0 && item.assignedColumn < 2);
      perColumn[item.assignedColumn].push(item);
    }
    for (const column of perColumn) {
      column.sort((a, b) => a.y - b.y);
      for (let i = 1; i < column.length; i++) {
        assert.ok(Math.abs(column[i].y - (column[i - 1].y + column[i - 1].height + 12)) < 0.01,
          'same-column cards stack exactly after prior measured height');
      }
    }
  };
  vm.runInContext('forceLayout()', context);
  verify();
  const before = items.map(item => item.assignedColumn);
  const belowFirst = items.find((item, i) => i > 0 && item.assignedColumn === before[0]);
  const originalY = belowFirst.y;
  items[0].height += 270;
  vm.runInContext('forceLayout()', context);
  verify();
  assert.deepEqual(items.map(item => item.assignedColumn), before,
    'late image hydration must not move cards sideways');
  assert.equal(belowFirst.y, originalY + 270, 'new media height moves lower slot, never overlaps it');
  assert.match(wall, /if \(relayoutPending\) \{\s*relayoutPending = false\s*scheduleLayout\(\)/);
  assert.match(wall, /onHeightChanged: view\.scheduleLayout\(\)/);
  assert.match(wall, /clip: true\s+onHeightChanged: view\.scheduleLayout\(\)/);
});

test('measured QML implicit height replaces oversized provisional Loader height', () => {
  const body = wall.match(/            function recordHeight\(\) \{([\s\S]*?)\n            \}\n\n            Loader/);
  assert.ok(body);
  const stored = [];
  const ctx = vm.createContext({
    cardLoader: {item: {height: 850, implicitHeight: 210}},
    measuredHeight: 850, stableKey: 'cast-1', revisionKey: 'rev-3', width: 330,
    view: {retainHeight(...args) {stored.push(args);}}
  });
  vm.runInContext('function recordHeight(){' + body[1] + '\n}', ctx);
  vm.runInContext('recordHeight()', ctx);
  assert.equal(ctx.measuredHeight, 210, 'a resolved cast releases its old 850px reservation');
  assert.equal(stored[0][3], 210);
  ctx.cardLoader.item = {height: 360, implicitHeight: 0};
  vm.runInContext('recordHeight()', ctx);
  assert.equal(ctx.measuredHeight, 360, 'height-only delegates retain a valid fallback');
  assert.match(wall, /onImplicitHeightChanged\(\) \{ slot\.recordHeight\(\) \}/);
});

test('Magazine view/filter toolbar matches one segmented surface across Explorer and Profile', () => {
  assert.match(toolbar, /id: segments[\s\S]*?fillColor: Theme\.surfaceMuted/);
  assert.match(toolbar, /id: segmentChoices[\s\S]*?anchors\.margins: 4/);
  assert.match(toolbar, /customSelectedSurface: Theme\.surfaceRaised/);
  assert.match(toolbar, /objectName: "explorerMagazineChoice"/);
  assert.match(toolbar, /objectName: "explorerTimelineChoice"/);
  assert.match(toolbar, /objectName: "magazineToolbarFilter"/);
  assert.match(toolbar, /onClicked: root\.filterRequested\(\)/);
  assert.match(explorer, /filterAvailable: root\.filterAvailable[\s\S]*?onFilterRequested: root\.filterRequested\(\)/);
  assert.match(explorer, /objectName: "explorerFilterTrigger"\s+\/\/ Desktop owns this action[\s\S]*?visible: root\.compact && root\.filterAvailable/);
  assert.match(profile, /filterLabel: qsTrId\("genycaster\.filters\.profile"\)/);
  assert.match(profile, /onFilterRequested: profileFilterSheet\.open\(\)/);
  assert.match(profile, /objectName: "mobileProfileFilterTrigger"[\s\S]*?visible: root\.compact/);
});

test('Magazine timestamp stays alongside the overflow action in the top metadata row', () => {
  const start = cast.indexOf('                RowLayout {\n                    Layout.fillWidth: true');
  const timestamp = cast.indexOf('objectName: "magazineCastTime"');
  const more = cast.indexOf('id: moreButton');
  const lowerStatus = cast.indexOf('visible: root.hasWriteState', more);
  assert.ok(start >= 0 && timestamp > start && more > timestamp && lowerStatus > more);
  const between = cast.slice(timestamp, more);
  assert.match(between, /Layout\.alignment: Qt\.AlignVCenter/);
  assert.match(cast.slice(more, lowerStatus), /Layout\.alignment: root\.magazineCard \? Qt\.AlignVCenter : Qt\.AlignTop/);
  assert.equal((cast.match(/objectName: "magazineCastTime"/g) || []).length, 1);
  assert.match(cast, /Layout\.minimumWidth: 0[\s\S]*?objectName: "magazineCastTime"/);
});

test('Profile Magazine estimates multiline text using actual compact column width', () => {
  assert.match(profile, /const compactCard = root\.compact \|\| root\.profileMagazineEnabled/);
  assert.match(profile, /Math\.max\(compactCard \? 260 : 480, Number\(availableWidth \|\| 0\)\)/);
  assert.match(profile, /root\.profileMagazineEnabled \? 1200 : 600/);
  assert.match(profile, /root\.profileMagazineEnabled \? 28 : 8/);
});

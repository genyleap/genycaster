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

test('measured media heights are routed to native stable-lane geometry instead of repainting offscreen delegates', () => {
  // Per-lane card offsets are computed by MasonryWindowModel in native C++;
  // attempting to simulate the deleted QML placement algorithm silently
  // tested code that no longer ships. A real Qt runtime test checks mixed
  // heights, geometry change, no overlap, and reverse viewport admission.
  const native = source('ui/app/social/feed_rows.cpp');
  const fixture = source('tests/ui/feed_rows_model_tests.cpp');
  assert.match(wall, /layoutModel\.relayout\(columns, committedCardWidth, gutter,/);
  assert.match(wall, /onHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall, /onImplicitHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall, /view\.retainHeight\(stableKey, revisionKey, width, resolved\)/);
  assert.match(native, /bottoms\[slot\.column\] \+= slot\.height \+ m_gap/);
  assert.match(native, /slot\.y = bottoms\[slot\.column\]/);
  assert.match(fixture, /grown\.value\(QStringLiteral\("y"\)\)\.toDouble\(\) > previousLaneY \+ 600\.0/);
  assert.match(fixture, /magazine\.setViewport\(8000\.0/);
  assert.match(wall, /if \(relayoutPending\) \{\s*relayoutPending = false\s*scheduleLayout\(\)/);
});

test('Magazine never mistakes a provisional Loader reservation for a real painted height', () => {
  const body = wall.match(/            function recordHeight\(\) \{([\s\S]*?)\n            \}\n            Component\.onCompleted:/);
  assert.ok(body);
  const stored = [];
  const ctx = vm.createContext({
    cardLoader: {item: {height: 850, implicitHeight: 850, resolvedHeight: 0, incubating: true}},
    measuredHeight: 0, cardHeight: 850, revisionSettling: false,
    stableKey: 'cast-1', revisionKey: 'rev-3', width: 330,
    view: {retainHeight(...args) {stored.push(args);}}
  });
  vm.runInContext('function recordHeight(){' + body[1] + '\n}', ctx);
  vm.runInContext('recordHeight()', ctx);
  assert.equal(stored.length, 0, 'do not store an estimate while the nested Loader incubates');

  ctx.cardLoader.item.incubating = false;
  vm.runInContext('recordHeight()', ctx);
  assert.equal(stored.length, 0, 'zero resolved height still means an unfinished card');

  ctx.cardLoader.item.resolvedHeight = 210;
  vm.runInContext('recordHeight()', ctx);
  assert.equal(ctx.measuredHeight, 210, 'painted cast releases an 850px reservation');
  assert.equal(stored.at(-1)[3], 210, 'native model receives only actual paint height');

  ctx.cardLoader.item.resolvedHeight = 360;
  vm.runInContext('recordHeight()', ctx);
  assert.equal(stored.at(-1)[3], 360, 'later rich-preview height changes reflow the next card');

  // Generic QML card delegates do not expose resolvedHeight; continue to
  // support their intrinsic height without entering a zero-height state.
  ctx.cardLoader.item = {height: 400, implicitHeight: 280};
  vm.runInContext('recordHeight()', ctx);
  assert.equal(ctx.measuredHeight, 280, 'ordinary implicit-height delegates remain supported');
  ctx.cardLoader.item = {height: 360, implicitHeight: 0};
  vm.runInContext('recordHeight()', ctx);
  assert.equal(ctx.measuredHeight, 360, 'height-only delegates retain a valid fallback');
  assert.match(wall, /onResolvedHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall, /onIncubatingChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  assert.match(wall, /onImplicitHeightChanged\(\) \{ slot\.scheduleHeightMeasurement\(\) \}/);
  const surface = source('ui/components/feed/FeedSurface.qml');
  assert.match(surface, /width: magazineRow && parent \? parent\.width : readList\.width/);
});

test('Magazine reveals committed slots and never repacks admitted cards during hydration', () => {
  const cpp = source('ui/app/social/feed_rows.cpp');
  const header = source('ui/app/social/feed_rows.hpp');
  assert.match(header, /Q_INVOKABLE void requestRepack\(\) noexcept;/);
  assert.match(cpp, /void MasonryWindowModel::requestRepack\(\) noexcept[\s\S]*?m_forceRepack = true;/);
  assert.doesNotMatch(wall, /scheduleRebalance|requestRepack|rebalanceTimer/);
  assert.match(cpp, /if \(slot\.laneCommitted\)\s+previousColumns\.insert/);
  assert.match(cpp, /desired\.laneCommitted = true/);
  assert.match(wall, /readonly property bool contentReady: cardLoader\.status === Loader\.Ready[\s\S]*?paintedHeight > 0/);
  assert.match(wall, /contentReady && Math\.abs\(Math\.ceil\(paintedHeight\) - cardHeight\) <= 1/);
  assert.match(wall, /id: visiblePlaceholder[\s\S]*?opacity: slot\.cardReady \? 0 : 1/);
  assert.match(wall, /id: cardLoader[\s\S]*?opacity: slot\.cardReady \? 1 : 0/);
  assert.ok((wall.match(/duration: Theme\.reduceMotion \|\| view\.moving \? 0 : Theme\.motionStandard/g) || []).length >= 2,
    'both sides of the skeleton handoff honor reduced motion and scroll');
  assert.doesNotMatch(wall, /Behavior on (?:height|cardHeight)/,
    'native geometry corrections must not be delayed by decorative size animations');
});

test('artwork reveals with opacity only after decode; miniature avatars avoid extra animations', () => {
  const image = source('ui/components/visual/SuperellipseImage.qml');
  const tile = source('ui/components/media/GalleryTile.qml');
  assert.match(image, /property bool smoothReveal: false/);
  assert.match(image, /visible: root\.ready \|\| \(root\.retainPreviousWhileLoading && imageSource\.status === Image\.Loading\)/);
  assert.match(image, /opacity: root\.ready \|\| \(root\.retainPreviousWhileLoading[\s\S]*?\? 1 : 0\s*Behavior on opacity/);
  assert.match(image, /enabled: root\.smoothReveal && !Theme\.reduceMotion && !root\.scrollMoving/);
  assert.match(tile, /fillColor: !preview\.ready \? Theme\.surfaceMuted/);
  assert.match(tile, /smoothReveal: true/);
});

test('Magazine view/filter toolbar uses background-free tabs in Explorer and Profile', () => {
  assert.match(toolbar, /Item \{\s*id: segments/);
  assert.match(toolbar, /id: segmentChoices[\s\S]*?spacing: Theme\.space2/);
  assert.match(toolbar, /showSelectionIndicator: false/);
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
  assert.match(cast.slice(more, lowerStatus), /Layout\.alignment: Qt\.AlignVCenter/);
  assert.match(cast, /id: castTrailingMetadata[\s\S]*?objectName: "magazineCastTime"[\s\S]*?id: moreButton/);
  assert.equal((cast.match(/objectName: "magazineCastTime"/g) || []).length, 1);
  assert.match(cast, /Layout\.minimumWidth: 0[\s\S]*?objectName: "magazineCastTime"/);
});

test('Profile Magazine estimates multiline text using actual compact column width', () => {
  assert.match(profile, /const compactCard = root\.compact \|\| root\.profileMagazineEnabled/);
  assert.match(profile, /Math\.max\(compactCard \? 260 : 480, Number\(availableWidth \|\| 0\)\)/);
  assert.match(profile, /root\.profileMagazineEnabled \? 1200 : 600/);
  assert.match(profile, /root\.profileMagazineEnabled \? 28 : 8/);
});

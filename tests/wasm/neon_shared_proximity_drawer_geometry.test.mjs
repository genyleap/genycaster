import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, statSync} from 'node:fs';
import vm from 'node:vm';

const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');
const geomSource = read('ui/components/visual/NeonGeometry.js');
const scope = vm.createContext({Math, Number, Infinity});
vm.runInContext(geomSource.replace(/^\.pragma library\s*/, ''), scope);

test('attached square drawer corners project to actual straight edges instead of an imaginary arc', () => {
  const w = 320, h = 620, r = 34;
  const tlSquare = scope.nearestBorderPoint(-5, -7, w, h, r, 4,
    false, true, true, false);
  assert.equal(tlSquare.x, 0);
  assert.equal(tlSquare.y, 0);
  assert.ok(Math.abs(tlSquare.distance - Math.hypot(5,7)) < 1e-7);
  const rounded = scope.nearestBorderPoint(-5, -7, w, h, r, 4);
  assert.ok(rounded.distance > tlSquare.distance + 4,
    'rounding must shorten the straight edge; square edge reaches (0,0)');
  const brSquare = scope.nearestBorderPoint(w + 4, h + 9, w, h, r, 4,
    true, false, false, true);
  assert.equal(brSquare.x, w);
  assert.equal(brSquare.y, h);
});

test('the shader carries four independent corner flags matching the Superellipse QSG renderer', () => {
  const shader = read('ui/shaders/neon.frag');
  const halo = read('ui/components/visual/NeonHalo.qml');
  const material = read('ui/components/visual/NeonSurface.qml');
  const drawer = read('ui/components/controls/GenyDrawer.qml');
  assert.match(shader,/vec4 cornerFlags;/);
  assert.match(shader,/roundedCorners\.x : roundedCorners\.y/);
  assert.match(shader,/roundedCorners\.z : roundedCorners\.w/);
  assert.match(shader,/clamp\(radius \* enabledCorner/);
  assert.match(shader,/surfaceParameters\.z,\s*surfaceParameters\.w, cornerFlags/);
  assert.match(halo,/root\.curveTopLeft \? 1 : 0/);
  assert.match(halo,/root\.curveBottomLeft \? 1 : 0/);
  for (const corner of ['TopLeft','TopRight','BottomLeft','BottomRight'])
    assert.match(material,new RegExp('curve'+corner+': material\\.curve'+corner));
  assert.match(drawer,/curveBottomRight: !control\.fullScreen && !control\.bottomSheet/);
  assert.doesNotMatch(drawer,/gradient: Gradient/);
  const precompiled = statSync(new URL('../../ui/shaders/neon.frag.qsb', import.meta.url));
  assert.ok(precompiled.size > 5000, 'new cross-platform QSB must include the per-corner uniform');
});

test('one grid pointer in a 12px gutter illuminates BOTH adjacent cards',()=>{
  // This is the same bounding-box falloff as the provided Neon Kit v0.4.x.
  const reach=85;
  const proximity=(x,y,w,h,px,py)=>{
    const dx=Math.max(0,x-px,px-(x+w));
    const dy=Math.max(0,y-py,py-(y+h));
    return Math.max(0,1-Math.hypot(dx,dy)/reach);
  };
  const px=106,py=52;
  assert.ok(proximity(0,0,100,100,px,py) > 0.9);
  assert.ok(proximity(112,0,100,100,px,py) > 0.9);
  // Four-card intersection: all four react without receiving hover events.
  for(const x of [0,112])
    for(const y of [0,112])
      assert.ok(proximity(x,y,100,100,106,106) > 0.9);
  assert.equal(proximity(300,300,100,100,106,106),0);
});

test('the shared non-blocking pointer is projected into all real dashboard and market cards',()=>{
  const material=read('ui/components/visual/NeonSurface.qml');
  const home=read('ui/components/home/HomeDashboard.qml');
  const stats=read('ui/components/home/StatsPanel.qml');
  const market=read('ui/components/home/MarketPanel.qml');
  const rail=read('ui/components/navigation/ContextRail.qml');
  assert.match(material,/property bool proximityPointerActive: false/);
  assert.match(material,/readonly property real proximityStrength:[\s\S]*?Math\.hypot\(proximityDx, proximityDy\)/);
  assert.match(material,/pointerActive:[\s\S]*?proximityStrength > 0/);
  assert.match(material,/material\.neonStrength \* \(material\.proximityPointerActive/);
  assert.match(home,/id: shortcutGridPointer[\s\S]*?blocking: false/);
  assert.match(home,/proximityPointerX: shortcutGridPointer\.point\.position\.x - shortcut\.x/);
  assert.match(rail,/id: widgetColumnPointer[\s\S]*?blocking: false/);
  assert.match(rail,/property: "proximityPointerActive"[\s\S]*?widgetColumnPointer\.hovered/);
  assert.match(rail,/property: "proximityPointerX"[\s\S]*?mapFromItem\(widgetColumn/);
  assert.match(stats,/id: statsKpiPointer[\s\S]*?blocking: false/);
  for(const id of ['castsKpi','repliesKpi','engagementKpi','followersKpi'])
    assert.ok(stats.includes('proximityPointerX: statsKpiPointer.point.position.x - '+id+'.x'));
  for (const id of ['assetCard','marketCapKpi','marketVolumeKpi','fearCard',
                    'marketBenchmarkCard','seasonCard','dominanceCard'])
    assert.match(market,new RegExp('proximityPointerX: [a-zA-Z]+Pointer\\.point\\.position\\.x - '+id+'\\.x'));
});

test('Masonry tracks gutters with one observer and forwards local coordinates through every Loader',()=>{
  const wall=read('ui/components/feed/FeedMasonryView.qml');
  const feed=read('ui/components/feed/FeedSurface.qml');
  const semantic=read('ui/components/feed/SemanticFeedItem.qml');
  const cast=read('ui/components/feed/CastItem.qml');
  const profile=read('ui/components/profile/ProfileSurface.qml');
  assert.match(wall,/id: masonryPointer[\s\S]*?blocking: false/);
  assert.match(wall,/sharedNeonPointerActive: masonryPointer\.hovered && !view\.moving/);
  assert.match(wall,/sharedNeonPointerY: masonryPointer\.point\.position\.y[\s\S]*?\+ view\.contentY - slot\.y/);
  assert.match(feed,/component FeedRow: Loader[\s\S]*?property bool sharedNeonPointerActive: false/);
  assert.match(feed,/sharedNeonPointerActive: parent\.sharedNeonPointerActive/);
  assert.match(feed,/sharedNeonPointerActive: rowDelegate\.sharedNeonPointerActive/);
  assert.match(semantic,/sharedNeonPointerActive: root\.magazineCard && root\.sharedNeonPointerActive/);
  assert.match(profile,/sharedNeonPointerActive: parent\.sharedNeonPointerActive/);
  assert.match(cast,/id: magazineCardSurface[\s\S]*?proximityPointerActive: root\.magazineCard && root\.sharedNeonPointerActive/);
  assert.match(cast,/mapFromItem\([\s\S]*?root, root\.sharedNeonPointerX, root\.sharedNeonPointerY\)/);
  assert.match(wall,/model: layoutModel/);
});

test('saved casts, app destinations and notification rows share hover over gaps',()=>{
 const bookmarks=read('ui/components/profile/BookmarksSurface.qml');
 const apps=read('ui/components/apps/AppsSurface.qml');
 const notices=read('ui/components/feed/NotificationSurface.qml');
 assert.match(bookmarks,/id: savedCastsPointer[\s\S]*?blocking: false/);
 assert.match(bookmarks,/proximityPointerX: savedCastsPointer\.point\.position\.x - bookmarkCard\.x/);
 assert.match(apps,/id: miniAppDestinationPointer[\s\S]*?blocking: false/);
 assert.match(apps,/proximityPointerX: miniAppDestinationPointer\.point\.position\.x - destinationCard\.x/);
 assert.match(notices,/id: notificationListPointer[\s\S]*?blocking: false/);
 assert.match(notices,/id: notificationCardSurface[\s\S]*?proximityPointerActive: notificationListPointer\.hovered/);
 assert.match(notices,/notificationCardSurface\.mapFromItem\(\s*list, notificationListPointer\.point\.position\.x/);
});

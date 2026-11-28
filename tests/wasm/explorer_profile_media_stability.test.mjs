import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const wall = read('ui/components/feed/FeedMasonryView.qml');
const profile = read('ui/components/profile/ProfileSurface.qml');
const gallery = read('ui/components/media/GalleryLayout.qml');

function method(source, name, nextName) {
  const start = source.indexOf('    function ' + name + '(');
  assert.ok(start >= 0, name + ' exists');
  const end = source.indexOf('    function ' + nextName + '(', start);
  assert.ok(end > start, nextName + ' follows');
  return source.slice(start, end);
}

test('Magazine stores measured card sizes and image aspect for reverse scroll without unbounded memory', () => {
  const source = wall.slice(wall.indexOf('    function aspectForKey('),
                            wall.indexOf('    readonly property int count:'));
  const context = vm.createContext({
    measuredHeights: {}, measuredOrder: [], aspectMemory: {}, aspectOrder: [],
    resizeInProgress: false,
    layoutModel: { retainHeight: () => false }
  });
  vm.runInContext(source, context);
  context.retainHeight('cast-4', 'r1', 336, 715);
  assert.equal(context.recalledHeight('cast-4', 'r1', 336), 715);
  assert.equal(context.recalledHeight('cast-4', 'r2', 336), 0,
    'a revised cast cannot reuse a stale height');
  assert.equal(context.recalledHeight('cast-4', 'r1', 420), 0,
    'a different card width does not reuse a stale height');
  context.rememberAspect('https://example.org/photo', 0.53);
  assert.equal(context.aspectForKey('https://example.org/photo'), 0.53);
  for (let i = 0; i < 720; ++i) {
    context.retainHeight('cast-' + i, 'r1', 340, 360 + i);
    context.rememberAspect('image-' + i, 0.5 + i / 2000);
  }
  assert.equal(Object.keys(context.measuredHeights).length, 400);
  assert.equal(Object.keys(context.aspectMemory).length, 600);
  assert.equal(context.aspectForKey('image-0'), 0, 'oldest image is evicted');
  assert.match(wall, /readonly property real retentionBuffer: cacheBuffer \+ Math\.min/);
  assert.doesNotMatch(wall, /onWidthChanged:\s*measuredHeight\s*=\s*0/);
  assert.match(wall, /onRevisionKeyChanged:\s*\{/);
  assert.match(wall, /active: slot\.admitted/);
});

test('an actual portrait aspect makes single-photo Magazine cards fill the card width', () => {
  const fn = method(gallery, 'buildLayout', 'layoutHeight');
  const ctx = vm.createContext({
    root: {
      visibleCount: 1, compact: true, dynamicAspectDiscovery: true,
      singleImageHeight: 0, gap: 8,
      declaredAspect: () => 0.5,
      boundedHeight: (h, min, max) => Math.max(min, Math.min(max, h))
    }
  });
  vm.runInContext(fn, ctx);
  const plan = vm.runInContext('buildLayout(320)', ctx);
  assert.equal(plan.length, 1);
  assert.equal(plan[0].x, 0);
  assert.equal(plan[0].width, 320);
  assert.equal(plan[0].height, 640, 'full 2:1 portrait rather than a narrow letterbox');
  ctx.root.declaredAspect = () => 2;
  const wide = vm.runInContext('buildLayout(320)', ctx);
  assert.equal(wide[0].height, 160);
  assert.match(gallery, /root\.sharedMediaGeometry\.aspectForKey\(key\)/);
  assert.match(gallery, /root\.sharedMediaGeometry\.rememberAspect\(key, value\)/);
  const declared = method(gallery, 'declaredAspect', 'initialAspect');
  const memory = vm.createContext({root: {
    embedAt: () => ({url: 'photo'}),
    mediaKey: () => 'photo',
    discoveredAspects: {}, reservedAspects: {photo: 4 / 3},
    sharedMediaGeometry: {aspectForKey: () => 0.5},
    dynamicAspectDiscovery: true,
    displayAspect: v => v, initialAspect: () => 4 / 3, rawAspect: () => 0
  }});
  vm.runInContext(declared, memory);
  assert.equal(vm.runInContext('declaredAspect(0)', memory), 0.5,
    'cached decoded ratio must override a prior 4:3 placeholder');
  assert.match(gallery, /if \(scroller && scroller\.moving\)/);
  assert.match(gallery, /ambientEnabled: !root\.singleSticker\s*&& root\.visibleCount === 1 && !root\.dynamicAspectDiscovery/);
});

test('phone portrait screenshots expand edge-to-edge to their full natural height without a crop ceiling', () => {
  const calculate = method(gallery, 'buildLayout', 'layoutHeight');
  const normalize = method(gallery, 'displayAspect', 'declaredAspect');
  const initial = gallery.slice(gallery.indexOf('    function initialAspect('),
                                gallery.indexOf('    onEmbedsChanged:', gallery.indexOf('    function initialAspect(')));
  const raw = method(gallery, 'rawAspect', 'declaredAspect');
  const base = {
    visibleCount: 1, compact: true, dynamicAspectDiscovery: true,
    mobileFullBleed: true, singleImageHeight: 180, gap: 8,
    declaredAspect: () => 0.17,
    boundedHeight: (h, lo, hi) => Math.max(lo, Math.min(hi, h)),
    clampAspect: v => Math.max(0.45, Math.min(2.8, v))
  };
  const c = vm.createContext({root: base});
  vm.runInContext(normalize + '\n' + raw + '\n' + initial + '\n' + calculate, c);
  c.root.displayAspect = vm.runInContext('displayAspect', c);
  c.root.rawAspect = vm.runInContext('rawAspect', c);
  assert.equal(vm.runInContext('displayAspect(0.17)', c), 0.17);
  assert.equal(vm.runInContext('initialAspect({width:170,height:1000})', c), 0.17);
  const tall = vm.runInContext('buildLayout(360)', c);
  assert.equal(tall.length, 1);
  assert.equal(tall[0].width, 360);
  assert.equal(tall[0].x, 0);
  assert.equal(tall[0].height, Math.ceil(360 / 0.17),
      'photo is full width and height is not capped at 720/860/900px');
  c.root.mobileFullBleed = false;
  assert.equal(vm.runInContext('displayAspect(0.17)', c), 0.45,
      'desktop editorial mosaic keeps the bounded aspect policy');
  const desktop = vm.runInContext('buildLayout(360)', c);
  assert.equal(desktop[0].height, 180,
      'desktop link-to-media promotion still retains its original slot');
  assert.match(gallery, /root\.mobilePresentation/);
  assert.match(read('ui/components/media/CastEmbedsPanel.qml'), /mobilePresentation: root\.mobileViewport/);
  assert.match(read('ui/components/media/EmbedPreview.qml'), /mobilePresentation: root\.mobileViewport/);
  assert.match(gallery, /if \(scroller && scroller\.moving\)/);
  const preview = read('ui/components/media/GalleryTile.qml');
  assert.match(preview, /fillMode: root\.officialSticker \|\| root\.preserveAspect\s*\? Image\.PreserveAspectFit : Image\.PreserveAspectCrop/);
});

test('image ratio knowledge reaches Explorer and Profile through the same magazine media renderer', () => {
  const feed = read('ui/components/feed/FeedSurface.qml');
  const semantic = read('ui/components/feed/SemanticFeedItem.qml');
  const cast = read('ui/components/feed/CastItem.qml');
  const panel = read('ui/components/media/CastEmbedsPanel.qml');
  const embed = read('ui/components/media/EmbedPreview.qml');
  const rich = read('ui/components/content/RichCastRenderer.qml');
  assert.match(feed, /sharedMediaGeometry: magazineView/);
  assert.match(semantic, /sharedMediaGeometry: root\.magazineCard \? root\.sharedMediaGeometry : null/);
  assert.match(cast, /sharedMediaGeometry: root\.sharedMediaGeometry/);
  assert.match(profile, /sharedMediaGeometry: profileMagazineView/);
  assert.match(panel, /dynamicAspectDiscovery: root\.magazineCard/);
  assert.match(panel, /root\.mobileViewport \|\| root\.scrollHost === null/);
  assert.match(embed, /dynamicAspectDiscovery: root\.magazineCard \|\| root\.mobileViewport/);
  assert.match(panel, /sharedMediaGeometry: root\.sharedMediaGeometry/);
  assert.match(embed, /sharedMediaGeometry: root\.sharedMediaGeometry/);
  assert.match(rich, /sharedMediaGeometry: root\.sharedMediaGeometry/);
  assert.match(cast, /id: messageSurface\s+\/\/ Long URLs,[\s\S]*?clip: root\.magazineCard/);
  assert.match(cast, /visible: root\.hasContext[\s\S]*?Layout\.minimumWidth: 0\s+clip: true/);
  assert.match(cast, /id: contextContent\s+Layout\.fillWidth: true\s+Layout\.minimumWidth: 0/);
});

test('Profile shares its one header, footer and native identity model between two modes', () => {
  const shell = read('ui/components/shell/AppShell.qml');
  assert.match(profile, /readonly property bool profileMagazineEnabled: !root\.compact/);
  assert.match(profile, /readonly property var readList: root\.profileMagazineEnabled \? profileMagazineView : profileList/);
  assert.match(profile, /id: profileHeaderComponent/);
  assert.match(profile, /id: profileFooterComponent/);
  assert.match(profile, /header: !root\.profileMagazineEnabled \|\| root\.timelineHeaderRetained\s*\? profileHeaderComponent : null/);
  assert.match(profile, /header: root\.profileMagazineEnabled \|\| root\.magazineHeaderRetained\s*\? profileHeaderComponent : null/);
  assert.match(profile, /function retainSelectedHeader\(\)/);
  assert.equal((profile.match(/footer: profileFooterComponent/g) || []).length, 2);
  assert.match(profile, /model: profileRowsModel/);
  assert.match(profile, /model: root\.profileMagazineEnabled \? null : profileRowsModel/);
  assert.match(profile, /modelData: \{\s*\/\/ Native role notification[\s\S]*?const revision = parent\.revisionKey/);
  assert.match(profile, /presentationKind: "conversation", mediaLayoutKind: "other"/);
  assert.match(profile, /ExplorerLayoutControls \{\s*layoutMode: root\.displayLayout/);
  assert.match(shell, /category: "profile-feed"\s+property alias displayLayout: root\.profileDisplayLayout/);
  assert.match(shell, /displayLayout: root\.profileDisplayLayout\s+onLayoutRequested: mode => root\.profileDisplayLayout = mode/);
  assert.match(profile, /PullToRefresh \{\s*flickable: root\.readList/);
  assert.match(profile, /ScrollToTopButton \{\s*flickable: root\.readList/);
  assert.doesNotMatch(profile, /profileRowsModel\.clear\(\)\s*\/\/ toggle/i);
});

test('no duplicate node identity is introduced to the profile header component', () => {
  assert.equal((profile.match(/id: profileHeaderComponent/g) || []).length, 1);
  assert.equal((profile.match(/id: profileFooterComponent/g) || []).length, 1);
  assert.equal((profile.match(/id: profileTabs\b/g) || []).length, 1);
  assert.equal((profile.match(/id: profileMagazineView\b/g) || []).length, 1);
  assert.equal((profile.match(/id: profileList\b/g) || []).length, 1);
});

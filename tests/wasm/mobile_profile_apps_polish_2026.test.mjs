import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('profile labels localize only presentation; canonical server options stay stable', () => {
  const profile = read('ui/components/profile/ProfileSurface.qml');
  const combo = read('ui/components/controls/GenyComboBox.qml');
  assert.match(profile, /function profileOptionLabel\(value\)/);
  assert.match(profile, /optionLabelProvider: function\(value\)/);
  assert.match(combo, /text: control\.localizedOption\(control\.displayText\)/);
  assert.match(profile, /selectedValues: profileExtrasEditor\.roleSelection/);
  assert.match(profile, /String\(profileExtrasEditor\.roleSelection\[0\]\)/);
  assert.match(profile, /categoryEditor\.currentText/);
  assert.match(profile, /genycaster\.profile\.more_links/);
  assert.match(profile, /genycaster\.share\.copy_profile_link/);
});

test('every locale includes user-visible share and option keys; Persian strings are translated', () => {
  for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
    const catalog = read('ui/i18n/qml_' + locale + '.ts');
    for (const key of ['genycaster.profile.option.creator',
                       'genycaster.profile.option.technology',
                       'genycaster.profile.more_links',
                       'genycaster.share.copy_profile_link',
                       'genycaster.share.open_farcaster',
                       'genycaster.brand.genyleap']) {
      assert.ok(catalog.includes('id="' + key + '"'), locale + ':' + key);
    }
  }
  const persian = read('ui/i18n/qml_fa_IR.ts');
  assert.match(persian, /<translation>و %1 لینک دیگر<\/translation>/);
  assert.match(persian, /<translation>جنی‌لیپ<\/translation>/);
  assert.match(persian, /<translation>فناوری<\/translation>/);
  assert.match(persian, /جست‌وجوی افراد، @کاربر، \/کانال یا نشرها/);
});

test('desktop app details open from the right in a bounded inspector', () => {
  const apps = read('ui/components/apps/AppsSurface.qml');
  const detail = apps.slice(apps.indexOf('id: miniAppDetailDrawer'));
  assert.match(detail, /bottomSheet: mobilePresentation/);
  assert.doesNotMatch(detail, /edge:\s*(?:Qt\.|mobilePresentation)/);
  const shared = read('ui/components/controls/GenyDrawer.qml');
  assert.match(shared, /edge:\s*bottomSheet\s*\?\s*Qt\.BottomEdge\s*:\s*\(Theme\.rtl\s*\?\s*Qt\.LeftEdge\s*:\s*Qt\.RightEdge\)/);
  assert.match(detail, /Math\.min\(480, parent\.width \* 0\.9\)/);
  assert.match(detail, /parent\.height\) : 640/);
});

test('apps saved indicators remain legibly green and enabled when already saved', () => {
  const apps = read('ui/components/apps/AppsSurface.qml');
  const shelf = read('ui/components/apps/FeaturedCarousel.qml');
  assert.match(apps, /positive: saved/);
  assert.match(apps, /positive: saved && !inLibrary/);
  assert.match(shelf, /positive: Boolean\(featuredCell\.app\.saved\)/);
  assert.match(shelf, /enabled: true/);
  assert.match(apps, /positive: Boolean\(root\.selectedMiniApp\.saved\)/);
});

test('bookmarks show compact icon-only actions and a gold priority star', () => {
  const saved = read('ui/components/profile/BookmarksSurface.qml');
  assert.match(saved, /CastItem \{/);
  assert.match(saved, /customIconColor: selected \? Theme.warning : Theme.inkMuted/);
  assert.match(saved, /iconGlyph: Theme.iconPriorityStar/);
  assert.match(saved, /iconOnly: true/);
  assert.doesNotMatch(saved, /Layout\.fillWidth: true\s*compact: true\s*quiet: true\s*selected: Number\(modelData\.priority/);
});

test('link rows retain real OpenGraph artwork when a favicon is missing', () => {
  const links = read('ui/components/profile/ProfileLinkRow.qml');
  assert.match(links, /String\(metadata\.favicon \|\| ""\)/);
  assert.match(links, /String\(metadata\.previewImage \|\| ""\)/);
  assert.match(links, /favicon\.ico/);
});

test('flicking freezes async media row reflows and captures anchor before reconciliation', () => {
  const feed = read('ui/components/feed/FeedSurface.qml');
  assert.match(feed, /function synchronizeHeight\(\) \{[\s\S]*?root\.rememberVisibleGeometry\(\)\s*rowDelegate\.committedHeight = nextHeight\s*if \(!readList\.moving && root\.geometryAnchorKey\.length > 0\)/);
  assert.match(feed, /onMovingChanged\(\) \{[\s\S]*?root\.rememberVisibleGeometry\(\)[\s\S]*?rowDelegate\.synchronizeHeight\(\)/);
});

test('browser tab icon is rebuilt from high-resolution vector mark with cache bump', () => {
  const html = read('ui/wasm/genycaster-shell.html');
  const mark = read('ui/wasm/genycaster-icon.svg');
  assert.match(html, /genycaster-icon\.svg\?v=brand-6/);
  assert.match(mark, /viewBox="48 48 416 416"/);
  assert.match(mark, /#BEBEBE/);
});

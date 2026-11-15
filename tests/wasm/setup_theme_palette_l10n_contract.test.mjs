import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = (path) => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const theme = source('ui/theme/Theme.qml');

test('onboarding hero, footer and typography use central responsive Theme roles', () => {
  const qml = source('ui/components/setup/InitialSetup.qml');
  const colorKeys = [
    'setupHeroTop', 'setupHeroMiddle', 'setupHeroBottom',
    'setupHeroPulseStroke', 'setupHeroTraceStroke',
    'setupHeroTitleInk', 'setupHeroCardBorder',
    'setupFooterDivider', 'setupFooterCopyrightInk', 'setupFooterVersionInk'
  ];
  for (const key of colorKeys) {
    assert.equal(theme.split('readonly property color ' + key + ':').length - 1, 1,
      'Theme token declared exactly once: ' + key);
    assert.ok(qml.includes('Theme.' + key), 'Setup actually uses token: ' + key);
  }
  assert.doesNotMatch(qml, /#[0-9a-f]{6,8}/i, 'setup palette must not be hardcoded');
  assert.doesNotMatch(qml, /font\.pixelSize\s*:\s*\d/, 'setup type must scale with user font settings');
  assert.match(qml, /font\.pixelSize: Theme\.typeCaptionSize/);
  assert.match(qml, /font\.pixelSize: Theme\.typeBodySize/);
});

test('GENY holder colors and mobile button preset swatches come from Theme', () => {
  const profile = source('ui/components/profile/ProfileSurface.qml');
  const settings = source('ui/components/settings/SettingsPopup.qml');
  const colors = [
    'verifiedGenyBadgeBackground', 'verifiedGenyBadgeBorder', 'verifiedGenyBadgeInk',
    'mobileActionBlue', 'mobileActionCoral'
  ];
  for (const key of colors) {
    assert.equal(theme.split('readonly property color ' + key + ':').length - 1, 1,
      'Theme token declared exactly once: ' + key);
  }
  for (const key of colors.slice(0, 3)) assert.ok(profile.includes('Theme.' + key));
  for (const key of colors.slice(3)) assert.ok(settings.includes('Theme.' + key));
  assert.doesNotMatch(profile, /#[0-9a-f]{6,8}/i);
  assert.doesNotMatch(settings, /#[0-9a-f]{6,8}/i);
});

test('all twelve supported locales present the process path without directional arrows', () => {
  const locales = ['en_US','ar_SA','de_DE','es_ES','fa_IR','fr_FR',
    'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  const path = /<message id="genycaster\.nodes\.provision_pipeline">([\s\S]*?)<\/message>/;
  for (const locale of locales) {
    const catalog = source('ui/i18n/qml_' + locale + '.ts');
    const match = catalog.match(path);
    assert.ok(match, 'provision path missing: ' + locale);
    assert.match(match[1], /<source>Server · Install · Sync · Connect<\/source>/);
    assert.match(match[1], /<translation>[^<]*·[^<]*·[^<]*·[^<]*<\/translation>/);
    assert.doesNotMatch(catalog, /[←→⇐⇒⬅➡]/, 'decorative arrows in ' + locale);
  }
}
);

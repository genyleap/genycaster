import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const components = [
  'ui/components/apps/AndroidWebView.qml',
  'ui/components/apps/AppsSurface.qml',
  'ui/components/settings/UpdateNoticeDrawer.qml',
  'ui/components/home/StatsDistributionRings.qml',
  'ui/components/content/RichInlineText.qml',
  'ui/components/composer/ComposerDrawer.qml',
  'ui/app/miniapp/EmbeddedBrowser.qml',
];

for (const path of components) {
  test(`semantic corner geometry and native QSG fill in ${path}`, () => {
    const qml = readFileSync(path, 'utf8');
    assert.match(qml, /Superellipse\s*\{/);
    assert.match(qml, /\bcurveDepth\s*:/);
    assert.match(qml, /\bexponent:\s*2/);
    assert.match(qml, /\bfillColor\s*:/);
    assert.doesNotMatch(qml, /\bradius\s*:/);
  });
}

test('WebView surface reshaping does not enable a privileged Android SDK bridge', () => {
  const source = readFileSync('ui/components/apps/AndroidWebView.qml', 'utf8');
  assert.match(source, /QtWebView/);
  assert.doesNotMatch(source, /GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST=1/);
});

test('Linux WebEngine host stays ephemeral when reshaping its background', () => {
  const source = readFileSync('ui/app/miniapp/EmbeddedBrowser.qml', 'utf8');
  assert.match(source, /offTheRecord:\s*true/);
  assert.match(source, /background:\s*Superellipse\s*\{/);
});

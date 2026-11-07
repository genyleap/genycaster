import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const base=new URL('../../', import.meta.url);
const read=(path)=>readFileSync(new URL(path,base),'utf8');

test('contextual menus exclusively own pointer/touch events',()=>{
  const cast=read('ui/components/feed/CastItem.qml');
  assert.match(cast,/CastActionsMenu \{/);
  assert.match(read('ui/components/feed/CastActionsMenu.qml'),/modal: true/);
  for (const path of ['ui/components/feed/ThreadReplyItem.qml',
      'ui/components/composer/EditorContextMenu.qml','ui/components/media/GenyVideoPlayer.qml']) {
    const source=read(path);
    assert.match(source,/modal: true/,path);
  }
  const preview=read('ui/components/media/LinkPreview.qml');
  assert.match(preview,/enabled: root\.targetUrl\.length > 0 && !root\.layoutMiniApp/);
  const setup=read('ui/components/setup/InitialSetup.qml');
  assert.match(setup,/MouseArea \{[\s\S]*?z: -1[\s\S]*?propagateComposedEvents: false/);
});
test('search scopes filter the actual result models and avoid translucent bleed-through',()=>{
  const qml=read('ui/components/navigation/SearchPalette.qml');
  assert.match(qml,/readonly property string searchScope/);
  assert.match(qml,/if \(root\.searchScope !== 'all' && kind !== root\.searchScope\)/);
  assert.match(qml,/rowIndex\+\+/);
  assert.match(qml,/snapshotBlurOnBrowser: true/);
  assert.match(qml,/genycaster\.search\.no_channel_results/);
  const backdrop=read('ui/components/visual/DrawerBackdrop.qml');
  assert.match(backdrop,/capture\.scheduleUpdate\(\)/);
  assert.match(backdrop,/!root\.browserRenderer/);
});
test('browser miniapps resolve verified public manifests and use in-app presentation',()=>{
  const qml=read('ui/components/media/LinkPreview.qml');
  const browser=read('ui/wasm/miniapp-host-pre.js');
  const gateway=read('services/media-gateway/gateway.mjs');
  assert.match(qml,/requestBrowserMiniAppManifest/);
  assert.match(qml,/browserMiniAppPreview = metadata/);
  assert.match(qml,/miniAppCatalog\.myApps/);
  assert.match(qml,/genycaster\.miniapp\.preview_unavailable/);
  assert.doesNotMatch(browser,/window\.open\(url\.href/);
  assert.match(browser,/function applyPresentation\(\)/);
  assert.match(browser,/function toggleFullscreen\(\)/);
  assert.match(browser,/frame\.setAttribute\("sandbox"/);
  assert.match(gateway,/requestUrl\.pathname === '\/v1\/browser\/miniapp-preview'/);
  assert.match(gateway,/resolveMiniAppPreview\(rawUrl\)/);
});
test('scrolling never demotes a loaded Mini App or detaches an existing image',()=>{
  const preview=read('ui/components/media/LinkPreview.qml');
  assert.doesNotMatch(preview,/browserMiniAppPreview\.providerKind === "miniapp"\)\s*&& !root\.scrollMoving/);
  assert.match(preview,/readonly property string animatedPosterUrl/);
  assert.match(preview,/AnimatedImage \{\s*id: miniAppAnimation/);
  const image=read('ui/components/visual/SuperellipseImage.qml');
  assert.doesNotMatch(image,/current !== requested \|\| \(!root\.ready/);
  assert.match(image,/if \(current !== requested\)\s*root\.activeSource = ""/);
  assert.match(image,/AnimatedImage \{\s*id: animatedGif/);
  const engagement=read('ui/components/feed/EngagementButton.qml');
  assert.doesNotMatch(engagement,/containmentMask:\s*background/);
  assert.match(engagement,/implicitHeight: Theme\.touchTarget/);
});
test('account summary has an aligned avatar and visible text target',()=>{
  const qml=read('ui/components/shell/AppShell.qml');
  assert.match(qml,/id: accountSummary[\s\S]*?RowLayout \{[\s\S]*?Avatar \{/);
  assert.match(qml,/sourceUrl: presentationBridge\.accountAvatarUrl/);
  assert.match(qml,/forceLeftToRight: true\s*horizontalAlignment: Text\.AlignLeft/);
});
test('approved onboarding stays minimal while advanced nodes remain available in Settings',()=>{
  const setup=read('ui/components/setup/InitialSetup.qml');
  const settings=read('ui/components/settings/SettingsPopup.qml');
  assert.doesNotMatch(setup,/nodeSelectionController\.nodes/);
  assert.doesNotMatch(setup,/genycaster\.setup\.advanced_nodes/);
  assert.match(settings,/NodeManagementPanel \{/);
  assert.match(read('ui/components/setup/SetupActionCard.qml'),/root\.latencyMs <= 300 \? Theme\.success/);
});
test('Persian setup translations do not use English brand or setup names',()=>{
  const xml=read('ui/i18n/qml_fa_IR.ts');
  const entries=[...xml.matchAll(/<message id="(genycaster\.setup\.[^"]+)">[\s\S]*?<translation(?:[^>]*)>([\s\S]*?)<\/translation>/g)];
  assert.ok(entries.length>80);
  for (const [,id,translation] of entries) {
    // Keep the product brand in Latin script in its logo accessibility label.
    if (id === 'genycaster.setup.logo_3d_accessible')
      assert.match(translation,/GenyCaster/);
    else
      assert.doesNotMatch(translation,/\b(?:Farcaster|Setup|Hypersnap|HyperSnap|Genycaster|GenyCaster)\b/);
  }
  assert.match(xml,/<message id="genycaster\.setup\.recovery_phrase_unavailable">/);
});

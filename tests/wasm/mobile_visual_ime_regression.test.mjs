import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('studio second scene uses portable protocol order and responsive safe-area spacing', () => {
  const shell = read('ui/wasm/genycaster-shell.html');
  const section = shell.split('<div class="studio-ecosystem">')[1].split('</div>\n    </div>')[0];
  const logos = [...section.matchAll(/intro-(farcaster|hypersnap|openproof|base)\.svg/g)].map(m=>m[1]);
  assert.deepEqual(logos, ['farcaster','hypersnap','openproof','base']);
  assert.match(shell, /\.studio-ecosystem\{width:calc\(100vw - 64px\)/);
  assert.match(shell, /\.studio-brand-logo\{max-width:58px/);
});

test('public profile link drawer provides favicon candidate and site monogram fallback', () => {
  const link = read('ui/components/ProfileLinkRow.qml');
  assert.match(link, /metadata\.favicon/);
  assert.match(link, /favicon\.ico/);
  assert.match(link, /linkPreviewResolver\.imageSourceForUrl/);
  assert.match(link, /root\.siteInitial/);
  assert.match(link, /Image\.PreserveAspectFit/);
});

test('raw link text is hidden only if a destination card already represents it', () => {
  const detail = read('ui/components/DetailSurface.qml');
  assert.match(detail, /readonly property string visibleCastText:/);
  assert.match(detail, /const embedded = root\.protocolEmbedUrls \|\| \[\]/);
  assert.match(detail, /plainText: root\.visibleCastText/);
  assert.match(detail, /String\(embedded\[i\]\)/);
  const cast = read('ui/components/CastItem.qml');
  assert.match(cast, /readonly property string displayText:/);
  assert.match(cast, /return source\.replace/);
});

test('link previews keep complete art and collectibles do not reserve blank panels', () => {
  const preview = read('ui/components/LinkPreview.qml');
  const collectible = read('ui/components/CollectiblePreview.qml');
  assert.match(preview, /id: genericThumb[\s\S]*?fillMode: Image\.PreserveAspectFit/);
  assert.match(preview, /readonly property bool shouldReserve: root\.reserveArtwork[\s\S]*?root\.previewSource\.length > 0/);
  assert.match(collectible, /visible: root\.artworkSource\.length > 0 && !Boolean\(root\.embed\.pairedVisual\)/);
  assert.match(collectible, /fillMode: Image\.PreserveAspectFit/);
  assert.match(collectible, /visible: root\.creatorText\.length > 0/);
});

test('discovery covers share a superellipse shape and Save action is visible', () => {
  const feature = read('ui/components/MiniAppFeaturedCarousel.qml');
  assert.match(feature, /id: featureCard[\s\S]*?curveDepth: Theme\.curveContainer/);
  assert.match(feature, /id: cover[\s\S]*?clipToSuperellipse: true[\s\S]*?curveDepth: Theme\.curveContainer/);
  assert.match(feature, /prominent: !Boolean\(featuredCell\.app\.saved\)/);
  assert.match(feature, /quiet: false/);
});

test('mobile reading surface reduces double insets without changing desktop geometry', () => {
  const cast = read('ui/components/CastItem.qml');
  const detail = read('ui/components/DetailSurface.qml');
  assert.match(cast, /anchors\.leftMargin: root\.compact \? Theme\.space2 : Theme\.space3/);
  assert.match(cast, /anchors\.leftMargin: root\.compact \? Theme\.space2 : Theme\.space4/);
  assert.match(detail, /anchors\.margins: root\.compact \? Theme\.space2 : Theme\.space5/);
});

test('reply TextArea retains IME preedit and commits native keyboard text after composition', () => {
  const composer = read('ui/components/ComposerDrawer.qml');
  assert.match(composer, /fullScreen: compact/);
  assert.match(composer, /height: parent \? \(compact \? parent\.height/);
  assert.match(composer, /mobilePlainEditor\.forceActiveFocus\(\)/);
  assert.match(composer, /Qt\.inputMethod\.show\(\)/);
  assert.match(composer, /onPreeditTextChanged:/);
  assert.match(composer, /preeditText\.length === 0/);
  assert.match(composer, /id: mobileInputCommit[\s\S]*?richComposer\.setBlockText\(0, mobilePlainEditor\.text\)/);
  assert.match(composer, /function synchronizeEditorState\(\)/);
});

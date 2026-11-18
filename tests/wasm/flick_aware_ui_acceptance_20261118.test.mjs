import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('transparent Timeline cards never paint an extra Magazine Neon plate', () => {
  const cast = read('ui/components/feed/CastItem.qml');
  assert.match(cast, /Item \{\s*id: messageSurface/);
  assert.match(cast, /NeonSurface \{\s*id: magazineCardSurface[\s\S]*?visible: root\.magazineCard/);
  assert.match(cast, /fillColor: root\.magazineCard \? Theme\.surfaceRaised : "transparent"/);
  assert.match(cast, /interactive: root\.magazineCard && !root\.scrollMoving/);
  assert.doesNotMatch(cast, /HoverHandler \{ id: messageHover \}/);
});

test('fitted Gallery media preserves its alpha corners without added dynamic ambient passes', () => {
  const gallery=read('ui/components/media/GalleryLayout.qml');
  assert.match(gallery, /useLayeredMask: Qt\.platform\.os === "wasm"[\s\S]*?\|\| root\.visibleCount === 1/);
  assert.match(gallery, /ambientEnabled: root\.visibleCount === 1 && !root\.dynamicAspectDiscovery/);
  assert.match(gallery, /active: tileSlot\.admitted/);
  assert.match(gallery, /asynchronous: root\.asynchronousTiles/);
});

test('in-flight image identity is preserved on repeated flicks but obsolete source is cleared', () => {
  const image=read('ui/components/visual/SuperellipseImage.qml');
  assert.match(image, /if \(current !== requested\)/);
  assert.match(image, /if \(current\.length > 0\)/);
  assert.match(image, /root\.activeSource = ""/);
  assert.match(image, /if \(root\.activeSource\.toString\(\)\.length === 0\)\s*sourceStartTimer\.stop\(\)/);
  assert.match(image, /root\.synchronizePresentation\(\)/);
});

test('scroll metadata only incubates rich Timeline previews, not recycled Magazine cards', () => {
  const embed=read('ui/components/media/EmbedPreview.qml');
  const link=read('ui/components/media/LinkPreview.qml');
  for (const source of [embed,link]) {
    assert.match(source,/asynchronous: !root\.magazineCard/);
    assert.match(source,/readonly property bool scrollMoving:/);
  }
  assert.match(embed,/root\.metadataUpdatePending = true/);
  assert.match(embed,/if \(root\.metadataUpdatePending\)/);
  assert.match(link,/root\.pendingResolvedPreview = preview/);
  assert.match(link,/if \(root\.resolvedPreviewPending\)/);
});

test('one fallback emoji renderer is failure-driven, lifetime-scoped, and never a detached incubator', () => {
  const rich=read('ui/components/content/RichInlineText.qml');
  assert.match(rich,/active: emojiOverlay\.spriteFallbackArmed && !lottieEmoji\.animationReady/);
  assert.match(rich,/visible: active && !lottieEmoji\.animationReady/);
  assert.match(rich,/asynchronous: false/);
  assert.match(rich,/textLayout\.setEmojiAnimated\(emojiPosition, emojiGeneration, false\)/);
  assert.doesNotMatch(rich,/spriteFallbackTimer/);
});

test('CastItem heavier media incubates only on Timeline and keeps compact responsive geometry', () => {
  const cast=read('ui/components/feed/CastItem.qml');
  assert.ok((cast.match(/asynchronous: !root\.magazineCard/g)||[]).length >= 3);
  assert.match(cast,/id: castTextBody[\s\S]*?asynchronous: false/);
  assert.match(cast,/active: root\.mediaFirst && root\.supportingEmbeds\.length > 0\s*&& root\.supportingAdmitted/);
  assert.match(cast,/implicitHeight: messageRow\.implicitHeight \+ \(root\.magazineCard \? 0 : Theme\.space3\)/);
  assert.match(cast,/implicitHeight: messageContent\.implicitHeight \+ \(root\.magazineCard \? Theme\.space4 : root\.compact \? Theme\.space2 : Theme\.space4\) \* 2/);
});

test('strict design audit remains hard gated and enforces these current concrete invariants', () => {
  const checker=read('ci/check_ui_design.py');
  assert.match(checker,/GalleryLayout scroll-time tile admission missing/);
  assert.match(checker,/transparent' not in cast_item/);
  assert.match(checker,/SuperellipseImage must detach obsolete image identities/);
  assert.match(checker,/EmbedPreview must defer moving-scroll metadata/);
  assert.match(checker,/LinkPreview must defer scroll metadata/);
  assert.match(checker,/Inline emoji must use bounded synchronous sprite fallback/);
  assert.match(checker,/CastItem must admit only supporting embeds/);
  assert.match(checker,/CastItem must retain responsive compact\/Magazine vertical/);
  assert.match(checker,/sys\.exit\(1\)|raise SystemExit\(1\)|return 1/);
});

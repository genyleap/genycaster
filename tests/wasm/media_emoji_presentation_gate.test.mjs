import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('decoded artwork is independent of deferred aspect geometry on every platform', () => {
    const tile = source('ui/components/media/GalleryTile.qml');
    const image = source('ui/components/visual/SuperellipseImage.qml');
    const layout = source('ui/components/media/GalleryLayout.qml');
    assert.doesNotMatch(tile, /finalAspectReady|awaitFinalAspect|presentationEnabled:/);
    assert.match(tile, /Image.PreserveAspectFit : Image.PreserveAspectCrop/);
    assert.match(image, /property bool presentationEnabled: true/);
    assert.match(image, /\(root\.decodedReady \|\| root\.retainingSizedTexture\)\s*&& root\.presentationReady && root\.presentationEnabled/);
    assert.match(layout, /onAspectRatioResolved: ratio => root\.rememberAspect/);
    assert.match(tile, /reportedMediaIdentity/);
    const remember=layout.slice(layout.indexOf('    function rememberAspect('),layout.indexOf('    function commitPendingAspects('));
    assert.doesNotMatch(remember, /sharedMediaGeometry\.rememberAspect/);
    const commit=layout.slice(layout.indexOf('    function commitPendingAspects('),layout.indexOf('    Timer {\n        id: aspectCommit'));
    assert.match(commit, /if \(scroller && scroller\.moving\) \{[\s\S]*?return[\s\S]*?sharedMediaGeometry\.rememberAspect/);
});

test('emoji one-shot is obeyed by policy, both renderers, persisted settings and refresh', () => {
    const policy = source('src/application/emoji/animation_policy.cpp');
    const emoji = source('ui/components/media/AnimatedEmoji.qml');
    const lottie = source('ui/components/media/AnimatedEmojiLottie.qml');
    const sprite = source('ui/components/media/SpriteEmoji.qml');
    const theme = source('ui/theme/Theme.qml');
    const settings = source('ui/components/settings/SettingsPopup.qml');
    const rich = source('ui/components/content/RichInlineText.qml');
    assert.match(policy, /PlaybackMode::PlayOnce/);
    assert.doesNotMatch(policy, /PlaybackMode::Loop/);
    assert.match(lottie, /animations\.loops: 1/);
    assert.match(sprite, /loops: 1/);
    assert.match(lottie, /root\.timeline\.frameCounter >= root\.timeline\.endFrame/);
    assert.match(lottie, /root\.cycleFinished = true/);
    // Playback completion holds the last visible frame; do not issue a pause
    // to the already-completed one-shot animation when it leaves the viewport.
    assert.match(lottie, /animations\.paused: !root\.animationActive && !root\.playbackCompleted/);
    assert.match(sprite, /root\.playbackFinished = true/);
    assert.match(sprite, /root\.framePosition = 0/);
    assert.match(emoji, /root\.emojiPlaybackEnabled && !root\.reducedMotion/);
    assert.match(sprite, /Theme\.animateEmoji && !Theme\.powerSave && !Theme\.reduceMotion/);
    assert.match(theme, /category: "MediaPowerSave"[\s\S]*?property bool animateEmoji: true/);
    assert.match(theme, /function setAnimateEmoji\(enabled\)/);
    assert.match(settings, /checked: Theme\.animateEmoji\s+onToggled: Theme\.setAnimateEmoji\(checked\)/);
    assert.match(rich, /lottieEmoji\.requestReplay\(\)/);
    assert.match(rich, /spriteEmojiLoader\.item\.requestReplay\(\)/);
    assert.match(emoji, /onInsideViewportChanged:/);
    assert.match(sprite, /onViewportVisibleChanged:/);
});

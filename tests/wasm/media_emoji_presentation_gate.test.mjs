import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('single-image reveal remains behind placeholder until final aspect is committed', () => {
    const tile = source('ui/components/media/GalleryTile.qml');
    const image = source('ui/components/visual/SuperellipseImage.qml');
    const layout = source('ui/components/media/GalleryLayout.qml');
    const extracted = tile.match(/readonly property bool finalAspectReady: \{([\s\S]*?)\n    \}/);
    assert.ok(extracted, 'actual GalleryTile eligibility expression exists');
    const ready = (width, height, ratio, fullBleed, loaded = true) => {
        const root = { awaitFinalAspect: true, preserveAspect: true,
                       reportedAspect: ratio, width, height, fullBleedAspect: fullBleed };
        const preview = { status: loaded ? 1 : 2 };
        const Image = { Ready: 1 };
        return vm.runInNewContext('(function(){' + extracted[1] + '})()', { root, preview, Image, Math });
    };
    assert.equal(ready(390, 293, 0.35, true), false,
      'a tall portrait must not flash inside its provisional landscape box');
    assert.equal(ready(390, Math.ceil(390 / 0.35), 0.35, true), true,
      'release after the full natural mobile height is committed');
    assert.equal(ready(400, 300, 0.2, false), false,
      'a desktop editorial card must not expose its small provisional box');
    assert.equal(ready(400, 889, 0.2, false), true,
      'desktop layout accepts the same editorial ratio clamp as GalleryLayout');
    assert.equal(ready(400, 940, 0.2, false), false,
      'do not wait for an impossible height derived from the raw portrait ratio');
    assert.equal(ready(400, 500, 0.8, false, false), false,
      'decoding must finish even if geometry already fits');
    assert.match(image, /property bool presentationEnabled: true/);
    assert.match(image, /root\.decodedReady && root\.presentationReady\s+&& root\.presentationEnabled/);
    assert.match(image, /root\.decodedReady && !root\.ready/);
    assert.match(tile, /presentationEnabled: root\.finalAspectReady/);
    assert.match(layout, /awaitFinalAspect: root\.dynamicAspectDiscovery && root\.visibleCount === 1/);
    assert.match(layout, /onAspectRatioResolved: ratio => root\.rememberAspect/);
    assert.match(tile, /reportedMediaIdentity/);
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

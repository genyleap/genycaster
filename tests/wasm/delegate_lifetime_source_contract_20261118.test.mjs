import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path,'utf8');

test('EmbedPreview preserves scroll metadata and differentiates async Timeline vs admitted Magazine',()=>{
  const qml=read('ui/components/media/EmbedPreview.qml');
  assert.match(qml,/property bool metadataUpdatePending: false/);
  assert.match(qml,/if \(root\.scrollMoving\)[\s\S]*?root\.metadataUpdatePending = true/);
  assert.match(qml,/if \(root\.metadataUpdatePending\)\s*root\.acceptLinkMetadata\(root\.pendingLinkMetadata\)/);
  assert.match(qml,/asynchronous: !root\.magazineCard && root\.rendererAsynchronous && root\.resolvedKind !== "link"/);
  const panel=read('ui/components/media/CastEmbedsPanel.qml');
  assert.match(panel,/asynchronousPreviews: asynchronousTiles && !magazineCard/);
  assert.match(panel,/asynchronous: root\.asynchronousPreviews/);
  assert.match(panel,/rendererAsynchronous: false/);
  assert.match(qml,/asynchronousTiles: !root\.magazineCard && root\.rendererAsynchronous/);
  assert.match(panel,/if \(!root\.visible\)\s*return/);
  assert.match(panel,/else\s*previewAdmission\.stop\(\)/);
});

test('LinkPreview defers resolved metadata while moving and avoids pending Magazine incubators',()=>{
  const qml=read('ui/components/media/LinkPreview.qml');
  assert.match(qml,/property bool resolvedPreviewPending: false/);
  assert.match(qml,/root\.pendingResolvedPreview = preview/);
  assert.match(qml,/if \(root\.resolvedPreviewPending\)/);
  assert.match(qml,/asynchronous: false/);
  assert.match(qml,/enabled: !root\.scrollMoving/);
});

test('inline emoji sprite is a single lifetime-scoped failure-only fallback',()=>{
  const qml=read('ui/components/content/RichInlineText.qml');
  assert.match(qml,/active: emojiOverlay\.spriteFallbackArmed && !lottieEmoji\.animationReady/);
  assert.match(qml,/visible: active && !lottieEmoji\.animationReady/);
  assert.match(qml,/asynchronous: false/);
  assert.match(qml,/textLayout\.setEmojiAnimated\(emojiPosition, emojiGeneration, false\)/);
  assert.doesNotMatch(qml,/spriteFallbackTimer/);
});

test('CastItem heavy media is async in Timeline and supporting cards wait for admission',()=>{
  const qml=read('ui/components/feed/CastItem.qml');
  assert.ok((qml.match(/asynchronous: !root\.magazineCard/g)||[]).length>=3);
  assert.match(qml,/id: castTextBody[\s\S]*?asynchronous: false/);
  assert.match(qml,/active: root\.mediaFirst && root\.supportingEmbeds\.length > 0\s*&& \(root\.magazineCard \|\| root\.supportingAdmitted\)/);
  assert.match(qml,/active: !root\.mediaFirst && root\.supportingEmbeds\.length > 0/);
});

test('strict audit enforces the actual scroll and lifetime invariants, without relaxing failure semantics',()=>{
  const guard=read('ci/check_ui_design.py');
  for(const phrase of [
    'asynchronous: !root.magazineCard',
    'if (root.metadataUpdatePending)',
    'if (root.resolvedPreviewPending)',
    'textLayout.setEmojiAnimated(emojiPosition, emojiGeneration, false)',
    'CastItem must admit only supporting embeds',
  ]) assert.ok(guard.includes(phrase),phrase);
  assert.match(guard,/sys\.exit\(1\)/);
});

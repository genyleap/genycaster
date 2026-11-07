import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Explore active tabs use typography, not a crowded blue background or outline', () => {
  const tabs=read('ui/components/navigation/ContentTabs.qml');
  assert.match(tabs,/color: tabButton.selected \? Theme\.accent/);
  assert.match(tabs,/font\.weight: Theme\.weightDemiBold/);
  assert.match(tabs,/Layout\.preferredWidth: String\(modelData\.id\) === "conversations" \? 1\.65 : 1/);
  assert.match(tabs,/fontSizeMode: Text\.HorizontalFit/);
  assert.doesNotMatch(tabs,/fillColor: tabButton\.selected/);
  assert.doesNotMatch(tabs,/borderColor: tabButton\.selected/);
});

test('like uses theme-aware filled-heart asset and quote sits directly beside reply', () => {
  const bar=read('ui/components/feed/EngagementBar.qml');
  const button=read('ui/components/feed/EngagementButton.qml');
  const theme=read('ui/theme/Theme.qml');
  const replyAt=bar.indexOf('objectName: "castReplyButton"');
  const quoteAt=bar.indexOf('objectName: "castQuoteButton"');
  const repostAt=bar.indexOf('iconGlyph: Theme.iconRepost');
  const likeAt=bar.indexOf('iconGlyph: root.displayedLikeIcon');
  assert.ok(replyAt>=0 && quoteAt>replyAt && repostAt>quoteAt && likeAt>repostAt);
  assert.match(bar,/iconFilled: root\.effectiveLikedByMe/);
  assert.match(bar,/activeColor: Theme\.negative/);
  assert.match(button,/filled: control\.iconFilled/);
  assert.match(theme,/case "heart": path = filled \? "actions\/heart-filled" : "actions\/heart"/);
});

test('OpenSea NFT fallback restricts API hosts and uses trusted preview-image leases', () => {
  const gateway=read('services/media-gateway/browser_link_preview.mjs');
  const app=read('services/media-gateway/gateway.mjs');
  const qml=read('ui/components/media/EmbedPreview.qml');
  assert.match(gateway,/export function parseOpenSeaAsset\(pageUrl\)/);
  assert.match(gateway,/api\.opensea\.io\/api\/v2\/chain/);
  assert.match(gateway,/verifiedImageUrl:candidate\.href/);
  assert.match(gateway,/if\(openSeaAsset\)/);
  assert.match(app,/verifiedLinkImages\.set\(body\.verifiedImageUrl/);
  assert.match(qml,/function requestBrowserMarketplacePreview\(\)/);
  assert.match(qml,/root\.linkMetadata\.previewImage/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('native and WASM reacted footer uses semantic icon/count hues, no tinted box',()=>{
  const button=read('ui/components/feed/EngagementButton.qml');
  const bar=read('ui/components/feed/EngagementBar.qml');
  assert.match(button,/fillColor: "transparent"/);
  assert.doesNotMatch(button,/fillColor: control.down \? Theme.surfacePressed/);
  assert.doesNotMatch(button,/control.active \? Qt.rgba/);
  assert.match(button,/color: control.active \? control.activeColor/g);
  assert.match(button,/borderWidth: Theme.keyboardFocusVisible\(control\) \? 2 : 0/);
  assert.match(bar,/activeColor: Theme.positive/);
  assert.match(bar,/activeColor: Theme.negative/);
  assert.match(bar,/iconFilled: root.effectiveLikedByMe/);
  assert.match(bar,/active: root.effectiveRecastedByMe/);
});

test('browser reactions fetch positive evidence through bounded account-scoped singleton',()=>{
  const qml=read('ui/components/feed/CastReactions.qml');
  const bar=read('ui/components/feed/EngagementBar.qml');
  const cmake=read('ui/CMakeLists.txt');
  const gateway=read('services/media-gateway/gateway.mjs');
  assert.match(qml,/pragma Singleton/);
  assert.match(cmake,/components\/feed\/CastReactions.qml/);
  assert.match(cmake,/set_source_files_properties\(components\/feed\/CastReactions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE\)/);
  assert.match(qml,/keyFor\(castId, viewerFid\)/);
  assert.doesNotMatch(qml,/XMLHttpRequest|JSON\.parse|https:\/\//);
  assert.match(qml,/browserReactionController/);
  assert.match(qml,/function retain\(castId, viewerFid\)/);
  assert.match(qml,/function release\(castId, viewerFid\)/);
  assert.match(read('ui/app/browser/reactions.cpp'),
    /genycaster::deployment::browserGateway[\s\S]*?\/v1\/browser\/cast-reactions/);
  assert.match(bar,/CastReactions.ensure\(root.reactionCastId, root.viewerFid\)/);
  assert.match(bar,/CastReactions.release\(/);
  assert.match(bar,/root.enrichmentActive/);
  assert.match(bar,/effectiveLikedByMe/);
  assert.match(bar,/CastReactions.setLocal\(root.reactionCastId, root.viewerFid, "recast", next\)/);
  assert.match(bar,/CastReactions.setLocal\(root.reactionCastId, root.viewerFid, "like", next\)/);
  assert.match(bar,/scrollMoving: false/);
  assert.match(read('ui/components/feed/CastItem.qml'),/scrollMoving: root.scrollMoving/g);
  assert.match(gateway,/requestUrl.pathname === '\/v1\/browser\/cast-reactions'/);
});

test('cast identity keeps name and handle stacked beside the avatar on narrow widths',()=>{
  const cast=read('ui/components/feed/CastItem.qml');
  assert.match(cast,/id: messageSurface[\s\S]*?Layout.minimumWidth: 0/);
  assert.match(cast,/id: castAuthorIdentity/);
  assert.match(cast,/id: castAuthorIdentity[\s\S]*?text: root.authorName[\s\S]*?root.ltrHandle\(root.authorHandle\)/);
  assert.match(cast,/objectName: "compactCastAuthorMetadata"/);
  assert.match(cast,/Layout.minimumWidth: 0[\s\S]*?root.ltrHandle\(root.authorHandle\)/);
  assert.match(cast,/id: castInlineChannel/);
  assert.match(cast,/visible: root.castChannel.length > 0/);
  assert.ok(cast.indexOf('id: castInlineChannel') < cast.indexOf('id: castTrailingMetadata'));
  assert.ok(cast.indexOf('root.ltrHandle(root.authorHandle)') < cast.indexOf('objectName: "compactCastAuthorMetadata"'));
  assert.doesNotMatch(cast,/Layout.leftMargin: root.mediaContentInset \+ 36 \+ Theme.space1/);
  assert.ok(cast.includes("LocaleFormat.formatRelativeTime("));
  assert.doesNotMatch(cast,/Layout.maximumWidth: root.compact[\s\S]*?messageSurface.width - Theme.space4 \* 2/);
});

test('rich cast text wraps long unspaced terms within its actual width',()=>{
  const native=read('src/presentation/qt/rich_inline_text_item.cpp');
  const view=read('ui/components/content/RichTextView.qml');
  const inline=read('ui/components/content/RichInlineText.qml');
  const renderer=read('ui/components/content/RichCastRenderer.qml');
  assert.match(native,/QTextOption::WrapAtWordBoundaryOrAnywhere/);
  assert.doesNotMatch(native,/option.setWrapMode\(QTextOption::WordWrap\)/);
  assert.match(view,/clip: false/);
  assert.match(inline,/clip: false/);
   assert.match(inline,/wrapMode: TextEdit\.Wrap/);
   assert.match(inline,/width: parent\.width/);
  assert.match(renderer,/Layout.minimumWidth: 0/);
});

test('generic link cards constrain thumbnail and metadata to available width',()=>{
  const link=read('ui/components/media/LinkPreview.qml');
  const embed=read('ui/components/media/EmbedPreview.qml');
  const panel=read('ui/components/media/CastEmbedsPanel.qml');
  assert.match(link,/clip: true/);
  assert.match(link,/Layout.minimumWidth: 0/);
  assert.match(link,/Math\.min\(Theme\.scaleUi\(240\),\s+Math\.max\(0, root\.width - root\.cardInset \* 2\) \* 0\.30\)/);
  assert.match(link,/Math.max\(0, root.width - content.anchors.leftMargin/);
  assert.match(embed,/Layout.minimumWidth: 0/);
  assert.match(panel,/Layout.minimumWidth: 0/);
});

test('reaction and view lifecycle handlers are combined rather than assigned twice',()=>{
  const bar=read('ui/components/feed/EngagementBar.qml');
  assert.equal((bar.match(/Component.onCompleted:/g)||[]).length,1);
  assert.equal((bar.match(/onCastDataChanged:/g)||[]).length,1);
  assert.match(bar,/Component.onCompleted: \{\s*syncViewInterest\(\)\s*requestViews\(\)\s*requestBrowserReactions\(\)/);
  assert.match(bar,/onCastDataChanged: \{\s*syncViewInterest\(\)\s*requestViews\(\)\s*requestBrowserReactions\(\)/);
  assert.match(bar,/Component.onDestruction: \{\s*if \(root\.retainedViewsCastId\.length > 0 && castViewController\)\s*castViewController\.release\(root\.retainedViewsCastId\)/);
});

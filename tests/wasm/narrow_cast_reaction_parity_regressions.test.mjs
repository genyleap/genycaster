import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('native and WASM reacted footer uses semantic icon/count hues, no tinted box',()=>{
  const button=read('ui/components/EngagementButton.qml');
  const bar=read('ui/components/EngagementBar.qml');
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
  const qml=read('ui/components/CastReactions.qml');
  const bar=read('ui/components/EngagementBar.qml');
  const cmake=read('ui/CMakeLists.txt');
  const gateway=read('services/media-gateway/gateway.mjs');
  assert.match(qml,/pragma Singleton/);
  assert.match(cmake,/components\/CastReactions.qml/);
  assert.match(cmake,/set_source_files_properties\(components\/CastReactions.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE\)/);
  assert.match(qml,/keyFor\(castId, viewerFid\)/);
  assert.match(qml,/pending\)\.slice\(0, 8\)/);
  assert.match(qml,/positive\[key \+ ":" \+ kind\] = true/);
  assert.match(qml,/root.revision \+= 1/);
  assert.match(qml,/storage-farcaster.genyleap.com\/v1\/browser\/cast-reactions/);
  assert.match(bar,/CastReactions.ensure\(root.descriptorCastId, root.viewerFid\)/);
  assert.match(bar,/root.enrichmentActive/);
  assert.match(bar,/effectiveLikedByMe/);
  assert.match(bar,/CastReactions.setLocal\(root.castId, root.viewerFid, "recast", next\)/);
  assert.match(bar,/CastReactions.setLocal\(root.castId, root.viewerFid, "like", next\)/);
  assert.match(bar,/scrollMoving: false/);
  assert.match(read('ui/components/CastItem.qml'),/scrollMoving: root.scrollMoving/g);
  assert.match(gateway,/requestUrl.pathname === '\/v1\/browser\/cast-reactions'/);
});

test('narrow cast header moves handle/time out of crowded name and menu row',()=>{
  const cast=read('ui/components/CastItem.qml');
  assert.match(cast,/id: messageSurface[\s\S]*?Layout.minimumWidth: 0/);
  assert.match(cast,/visible: !root.compact && Boolean\(root.modelData.author\)/);
  assert.match(cast,/objectName: "compactCastAuthorMetadata"/);
  assert.match(cast,/Layout.minimumWidth: 0[\s\S]*?text: root.ltrHandle/);
  assert.match(cast,/Layout.maximumWidth: root.compact[\s\S]*?messageSurface.width - Theme.space4 \* 2/);
  assert.match(cast,/LocaleFormat.formatRelativeTime\(Number\(root.modelData.createdAtMs\)\)/);
});

test('rich cast text wraps long unspaced terms within its actual width',()=>{
  const native=read('src/presentation/qt/rich_inline_text_item.cpp');
  const view=read('ui/components/RichTextView.qml');
  const inline=read('ui/components/RichInlineText.qml');
  const renderer=read('ui/components/RichCastRenderer.qml');
  assert.match(native,/QTextOption::WrapAtWordBoundaryOrAnywhere/);
  assert.doesNotMatch(native,/option.setWrapMode\(QTextOption::WordWrap\)/);
  assert.match(view,/clip: true/);
  assert.match(inline,/clip: true/);
  assert.match(renderer,/Layout.minimumWidth: 0/);
});

test('generic link cards constrain thumbnail and metadata to available width',()=>{
  const link=read('ui/components/LinkPreview.qml');
  const embed=read('ui/components/EmbedPreview.qml');
  const panel=read('ui/components/CastEmbedsPanel.qml');
  assert.match(link,/clip: true/);
  assert.match(link,/Layout.minimumWidth: 0/);
  assert.match(link,/Math.min\(root.compact \? 116 : 184,/);
  assert.match(link,/Math.max\(0, root.width - content.anchors.leftMargin/);
  assert.match(embed,/Layout.minimumWidth: 0/);
  assert.match(panel,/Layout.minimumWidth: 0/);
});

test('reaction and view lifecycle handlers are combined rather than assigned twice',()=>{
  const bar=read('ui/components/EngagementBar.qml');
  assert.equal((bar.match(/Component.onCompleted:/g)||[]).length,1);
  assert.equal((bar.match(/onCastDataChanged:/g)||[]).length,1);
  assert.match(bar,/Component.onCompleted: \{\s*syncViewInterest\(\)\s*requestViews\(\)\s*requestBrowserReactions\(\)/);
  assert.match(bar,/onCastDataChanged: \{\s*syncViewInterest\(\)\s*requestViews\(\)\s*requestBrowserReactions\(\)/);
  assert.match(bar,/Component.onDestruction: \{\s*if \(root.retainedViewsCastId.length > 0\)\s*castViewController.release\(root.retainedViewsCastId\)/);
});

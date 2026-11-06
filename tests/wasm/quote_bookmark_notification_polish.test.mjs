import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('Farcaster Quote and Recast notification events survive the provider and presentation boundaries',()=>{
  const capability=read('src/capabilities/notification.cppm');
  const codec=read('src/providers/hypersnap/codec.cpp');
  const presenter=readGenycasterMain();
  const screen=read('ui/components/NotificationSurface.qml');
  assert.match(capability,/\bQuote,/);
  for(const type of ['cast-quote','quote-cast','cast-recast']) assert.ok(codec.includes('"'+type+'"'));
  assert.match(codec,/NotificationKind::Quote/);
  assert.match(presenter,/case capability::notification::NotificationKind::Quote:[\s\S]*?kind = QStringLiteral\("quote"\)/);
  assert.match(screen,/case "quote": return Theme.iconQuote/);
  assert.match(screen,/kind === "quote"/);
  assert.match(screen,/kind === "recast"/);
  assert.match(screen,/if \(category === "all"\) return true/);
  assert.match(screen,/kind === "quote"/);
  assert.match(screen,/fillColor: rowTap.pressed \? Theme.surfacePressed[\s\S]*?Qt\.rgba\(Theme\.accent/);
  assert.match(screen,/id: castExcerpt[\s\S]*?implicitHeight: excerptText.implicitHeight/);
  assert.doesNotMatch(screen,/id: castExcerpt[\s\S]{0,450}fillColor: Theme.surfaceMuted/);
});

test('direct cast detail hydrates quote and parent context with a distinct primary request lifetime',()=>{
  const source=readGenycasterMain();
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(source,/std::vector<domain::cast::CastId> detailReferences/);
  assert.match(source,/if \(state.cast->parent\) appendReference\(\*state.cast->parent\)/);
  assert.match(source,/for \(const auto& embed : state.cast->embeds\)/);
  assert.match(source,/for \(const auto& referencedId : detailReferences\)/);
  assert.match(source,/runtime->isCurrentPrimaryRequestGeneration\(requestGeneration\)/);
  assert.match(source,/bridge.applyReferencedCastPreview\(/);
  assert.match(bridge,/bool applyReferencePreviewCache\(\s*QVariantMap& row/);
  assert.match(bridge,/changed = applyReferencePreviewCache\(m_detail, m_referencedPreviewCache\)/);
  assert.match(bridge,/if \(applyReferencePreviewCache\(reply, cache\)\)/);
  assert.match(bridge,/m_referencedPreviewCache.insert\(targetKey, preview\)/);
});

test('cast detail keeps timestamp beside author and views in the same action footer',()=>{
  const detail=read('ui/components/DetailSurface.qml');
  const bar=read('ui/components/EngagementBar.qml');
  assert.doesNotMatch(detail,/id: detailMetadata/);
  assert.match(detail,/LocaleFormat\.formatDateTime\(milliseconds\)/);
  assert.match(detail,/LocaleFormat\.formatRelativeTime\(milliseconds\)/);
  assert.match(detail,/EngagementBar \{[\s\S]*?showViews: true/);
  assert.match(bar,/visible: root.showViews && root.resolvedViewCount/);
  assert.match(bar,/id: reactionActions[\s\S]*?iconGlyph: Theme.iconReply/);
  assert.match(bar,/id: utilityActions[\s\S]*?id: bookmarkButton/);
  assert.match(bar,/Item \{ Layout.fillWidth: true; Layout.minimumWidth: 0 \}/);
  assert.match(bar,/id: reactionActions[\s\S]*?iconGlyph: Theme.iconRepost/);
  assert.match(bar,/metricText: root.likeCount > 0 \? LocaleFormat.formatCompactNumber\(root.likeCount\)/);
  assert.match(bar,/id: bookmarkButton/);
  assert.match(bar,/iconFilled: root.isBookmarked/);
});

test('bookmarks persist on-device per verified account and have a navigable route',()=>{
  const c=read('src/presentation/qt/presentation_bridge.cpp');
  const h=read('src/presentation/qt/presentation_bridge.hpp');
  const shell=read('ui/components/AppShell.qml');
  const ui=read('ui/components/BookmarksSurface.qml');
  assert.match(h,/Q_PROPERTY\(QStringList bookmarkedCastIds READ bookmarkedCastIds NOTIFY bookmarksChanged\)/);
  assert.match(h,/Q_INVOKABLE void toggleBookmark\(const QVariantMap& cast\)/);
  assert.match(c,/bookmarks\/account_%1/);
  assert.match(c,/previousAccountFid != m_accountFid/);
  assert.match(c,/canonicalProtocolCastId\(cast.value\(QStringLiteral\("castId"\)\)/);
  assert.match(shell,/active: root.shellRoute === "bookmarks"/);
  assert.match(shell,/BookmarksSurface \{/);
  assert.match(ui,/objectName: "bookmarkRichCast"/);
  assert.match(ui,/onOpenRequested: castId => root.castRequested\(castId\)/);
  assert.match(shell,/objectName: "mobileAccountBookmarksButton"/);
  assert.match(read('ui/CMakeLists.txt'),/components\/BookmarksSurface.qml/);
});

test('main casts and every threaded reply show views in a common footer at all widths',()=>{
  const cast=read('ui/components/CastItem.qml');
  const reply=read('ui/components/ThreadReplyItem.qml');
  const bar=read('ui/components/EngagementBar.qml');
  const button=read('ui/components/EngagementButton.qml');
  assert.equal((cast.match(/showViews: true/g)||[]).length,2);
  assert.match(reply,/showViews: true/);
  assert.match(reply,/authorFid: Number\(root.reply.authorFid \|\| 0\)/);
  assert.match(reply,/castText: String\(root.reply.text \|\| ""\)/);
  assert.doesNotMatch(cast,/compactViewsText/);
  assert.match(cast,/objectName: "compactCastAuthorMetadata"/);
  assert.match(cast,/Number\(root.modelData.createdAtMs \|\| 0\) > 0/);
  assert.match(bar,/id: castViewsLabel[\s\S]*?Layout.fillWidth: true/);
  assert.match(bar,/metricText: root.replyCount > 0 \? LocaleFormat.formatCompactNumber/);
  assert.match(bar,/objectName: "castBookmarkButton"/);
  assert.match(button,/RowLayout \{[\s\S]*?Item \{ Layout.fillWidth: true/);
});

test('quote notifications preserve a clickable reference when embedded cast details are absent',()=>{
  const presenter=readGenycasterMain();
  const ui=read('ui/components/NotificationSurface.qml');
  assert.match(presenter,/NotificationKind::Quote\) \{[\s\S]*?row.insert\(QStringLiteral\("quotedCastId"\)/);
  assert.match(ui,/quotedCastId: source.quotedCastId/);
  assert.match(ui,/notificationRow.kindKey === "quote"[\s\S]*?notificationRow.modelData.quotedCastId/);
  assert.match(ui,/onTapped: root.castRequested\(String\(notificationRow.modelData.quotedCastId/);
});

test('quoted-cast author avatar is inset and aligned inside the media card',()=>{
  const card=read('ui/components/EmbedCard.qml');
  assert.match(card,/anchors.margins: root.isCastReference \? Theme.space4 : Theme.space2/);
  assert.match(card,/Layout.preferredWidth: 32[\s\S]*?Layout.preferredHeight: 32[\s\S]*?avatarSize: 32/);
  assert.match(card,/spacing: Theme.space3[\s\S]*?layoutDirection: root.quotedRtl/);
});

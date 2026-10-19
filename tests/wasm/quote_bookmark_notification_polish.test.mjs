import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('Farcaster Quote and Recast notification events survive the provider and presentation boundaries',()=>{
  const capability=read('src/capabilities/notification.cppm');
  const codec=read('src/providers/hypersnap/codec.cpp');
  const presenter=read('ui/mobile/main.cpp');
  const screen=read('ui/components/NotificationSurface.qml');
  assert.match(capability,/\bQuote,/);
  for(const type of ['cast-quote','quote-cast','cast-recast']) assert.ok(codec.includes('"'+type+'"'));
  assert.match(codec,/NotificationKind::Quote/);
  assert.match(presenter,/case capability::notification::NotificationKind::Quote:[\s\S]*?kind = QStringLiteral\("quote"\)/);
  assert.match(screen,/case "quote": return Theme.iconQuote/);
  assert.match(screen,/kind === "quote"/);
  assert.match(screen,/kind === "recast"/);
  assert.match(screen,/root\.filterKind === "reactions"[\s\S]*?String\(row.kind \|\| ""\) === "quote"/);
  assert.match(screen,/fillColor: rowTap.pressed \? Theme.surfacePressed[\s\S]*?Qt\.rgba\(Theme\.accent/);
  assert.match(screen,/id: castExcerpt[\s\S]*?implicitHeight: excerptText.implicitHeight/);
  assert.doesNotMatch(screen,/id: castExcerpt[\s\S]{0,450}fillColor: Theme.surfaceMuted/);
});

test('direct cast detail hydrates quote and parent context with a distinct primary request lifetime',()=>{
  const source=read('ui/mobile/main.cpp');
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  assert.match(source,/std::vector<domain::cast::CastId> detailReferences/);
  assert.match(source,/if \(state.cast->parent\) appendReference\(\*state.cast->parent\)/);
  assert.match(source,/for \(const auto& embed : state.cast->embeds\)/);
  assert.match(source,/for \(const auto& referencedId : detailReferences\)/);
  assert.match(source,/runtime->isCurrentPrimaryRequestGeneration\(requestGeneration\)/);
  assert.match(source,/bridge.applyReferencedCastPreview\(/);
  assert.match(bridge,/void applyReferencePreviewCache\(QVariantMap& row/);
  assert.match(bridge,/applyReferencePreviewCache\(m_detail, m_referencedPreviewCache\)/);
  assert.match(bridge,/m_referencedPreviewCache.insert\(targetKey, preview\)/);
});

test('cast detail separates full date and exact impressions from responsive action rail',()=>{
  const detail=read('ui/components/DetailSurface.qml');
  const bar=read('ui/components/EngagementBar.qml');
  assert.match(detail,/id: detailMetadata/);
  assert.match(detail,/LocaleFormat.formatDateTime\(Number\(root\.detail\.createdAtMs\)\)/);
  assert.match(detail,/return CastViews.countFor\(String\(root.detail.castId \|\| ""\)\)/);
  assert.match(detail,/showViews: false/);
  assert.match(bar,/showViews: true/);
  assert.match(bar,/visible: root.showViews && root.resolvedViewCount/);
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
  assert.match(ui,/onTapped: root.castRequested\(String\(modelData.castId \|\| ""\)\)/);
  assert.match(read('ui/CMakeLists.txt'),/components\/BookmarksSurface.qml/);
});

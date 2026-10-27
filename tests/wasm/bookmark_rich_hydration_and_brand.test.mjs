import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../'+file,import.meta.url),'utf8');

test('saved casts render through canonical CastItem with full media, link and parent pipeline',()=>{
  const ui=read('ui/components/BookmarksSurface.qml');
  const cast=read('ui/components/CastItem.qml');
  assert.match(ui,/CastItem \{/);
  assert.match(ui,/modelData: bookmarkCard.modelData/);
  assert.match(ui,/onExternalRequested: url => root.externalRequested\(url\)/);
  assert.match(ui,/onMediaRequested: \(mediaItems, index\) => root.mediaRequested\(mediaItems, index\)/);
  assert.match(cast,/CastEmbedsPanel \{/);
  assert.match(cast,/RichCastRenderer \{/);
  assert.match(ui,/CastSkeleton \{/);
  assert.match(ui,/visible: castSkeletonFallback.visible[\s\S]*?genycaster.bookmarks.open_saved_cast/);
});

test('bookmarks hydrate by cast hash through native provider read without navigating or uploading',()=>{
  const bridge=read('src/presentation/qt/presentation_bridge.cpp');
  const binding=read('ui/mobile/runtime_ui_bindings.cpp');
  const surface=read('ui/components/BookmarksSurface.qml');
  const shell=read('ui/components/AppShell.qml');
  assert.match(bridge,/void PresentationBridge::hydrateBookmarkedCasts\(/);
  assert.match(bridge,/void PresentationBridge::applyHydratedBookmark\(/);
  assert.match(bridge,/if \(!pending.isEmpty\(\)\) emit bookmarkHydrationRequested/);
  assert.match(binding,/&PresentationBridge::bookmarkHydrationRequested/);
  assert.match(binding,/runtime->service->openCast\(castId/);
  assert.match(binding,/castPreviewPresentation\(state\)/);
  assert.match(binding,/bridge.applyHydratedBookmark\(hash/);
  assert.match(surface,/function hydrateVisible\(\)/);
  assert.match(shell,/presentationBridge.hydrateBookmarkedCasts\(castIds\)/);
  const body=bridge.slice(bridge.indexOf('void PresentationBridge::applyHydratedBookmark'),
                          bridge.indexOf('QVariantMap PresentationBridge::bookmarkEntry'));
  assert.doesNotMatch(body,/\bemit\s+bookmarkMutated\(/);
  assert.match(body,/QStringLiteral\("embeds"\)/);
  assert.match(body,/QStringLiteral\("richHydrated"\)/);
});

test('priority uses an outlined star when off and amber filled star when on',()=>{
  const qml=read('ui/components/BookmarksSurface.qml');
  const theme=read('ui/theme/Theme.qml');
  assert.match(qml,/selected: Number\(bookmarkCard.modelData.priority \|\| 0\) > 0/);
  assert.match(qml,/customIconColor: selected \? Theme.warning : Theme.inkMuted/);
  assert.match(qml,/iconGlyph: Theme.iconPriorityStar/);
  assert.match(theme,/case "priority-star": path = filled \? "actions\/star-filled" : "actions\/star"/);
  assert.match(read('ui/assets/icons/genyleap/icons/actions/star.svg'),/fill="none"/);
  assert.match(read('ui/assets/icons/genyleap/icons/actions/star-filled.svg'),/fill="currentColor"/);
});

test('header uses a stable wordmark, separated Beta label and bidi-isolated version',()=>{
  const qml=read('ui/components/AppHeader.qml');
  const fa=read('ui/i18n/qml_fa_IR.ts');
  assert.match(qml,/genycaster-wordmark-dark.svg/);
  assert.match(qml,/ColumnLayout \{[\s\S]*?LayoutMirroring.enabled: false/);
  assert.match(qml,/genycaster.release.beta_label/);
  assert.match(qml,/\\u2066/);
  assert.match(fa,/id="genycaster.release.beta_label"/);
});

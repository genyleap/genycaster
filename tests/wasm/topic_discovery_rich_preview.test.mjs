import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const bridgeHeader = read('src/presentation/qt/presentation_bridge.hpp');
const bridgeSource = read('src/presentation/qt/presentation_bridge.cpp');
const runtime = read('ui/app/runtime_ui_bindings.cpp');
const browse = read('ui/components/TopicBrowseSurface.qml');
const shell = read('ui/components/AppShell.qml');
const picker = read('ui/components/TopicFilterPicker.qml');

test('topic discovery can request canonical provider previews without creating bookmarks', () => {
  assert.match(bridgeHeader, /Q_PROPERTY\(QVariantList topicPreviewCasts READ topicPreviewCasts NOTIFY topicPreviewCastsChanged\)/);
  assert.match(bridgeHeader, /void topicHydrationRequested\(QStringList castIds\)/);
  assert.match(bridgeSource, /void PresentationBridge::hydrateTopicCasts\(const QStringList& castIds\)/);
  assert.match(bridgeSource, /void PresentationBridge::applyHydratedTopicCast\(QString castId, QVariantMap cast\)/);
  assert.match(runtime, /PresentationBridge::topicHydrationRequested/);
  assert.match(runtime, /runtime->service->openCast\(castId,/);
  assert.match(runtime, /bridge\.applyHydratedTopicCast\(hash, std::move\(preview\)\)/);
  assert.doesNotMatch(runtime.slice(runtime.indexOf('// Topic discovery hydrates only visible'), runtime.indexOf('// Hydrate only visible saved casts')), /applyHydratedBookmark/);
});
test('late topic previews are ignored after account switch, and their cache is bounded', () => {
  assert.match(runtime, /bridge\.accountFid\(\) == ownerFid/);
  assert.match(bridgeSource, /m_topicPreviewCasts\.size\(\) > 90/);
  assert.match(bridgeSource, /if \(previousAccountFid != m_accountFid\) \{\s*m_topicPreviewCasts\.clear\(\);/);
  assert.match(bridgeSource, /canonicalProtocolCastId\(\s*cast\.value\(QStringLiteral\("castId"\)\)\.toString\(\)\) != hash/);
});
test('topic list hydrates only visible delegates and uses rich previews once available', () => {
  assert.match(browse, /results\.itemAtIndex\(i\)/);
  assert.match(browse, /requested\.length>=8/);
  assert.match(browse, /onContentYChanged: visibleHydrationTimer\.restart\(\)/);
  assert.match(browse, /onHydratedRowsChanged: visibleHydrationTimer\.restart\(\)/);
  assert.match(browse, /hydrationRequested\(requested\)/);
  assert.match(shell, /hydratedRows: presentationBridge\.topicPreviewCasts/);
  assert.match(shell, /onHydrationRequested: castIds => presentationBridge\.hydrateTopicCasts\(castIds\)/);
});
test('topic filter uses compact native menu and dynamically labels new slugs', () => {
  assert.match(picker, /FilterDropdown \{/);
  assert.match(picker, /default: return slug\.replace\(/);
  assert.match(picker, /maximumTriggerWidth: 156/);
});


test('bookmark collection creation keeps chooser open while the server confirms the saved collection', () => {
  const chooser = read('ui/components/BookmarkCollectionChooser.qml');
  assert.match(chooser, /property string errorText:/);
  assert.match(chooser, /text: root\.errorText/);
  assert.match(chooser, /root\.createRequested\(root\.newName\.trim\(\)\)/);
  assert.match(chooser, /collectionsProNotice\.open\(\)/);
  assert.doesNotMatch(chooser, /root\.createRequested\(root\.newName\.trim\(\)\)[\s\S]{0,70}root\.close\(\)/);
  assert.match(shell, /onClosed: root\.bookmarkAssignmentCreateName=""/);
  assert.match(shell, /openProofConnections\.bookmarkCollectionsError\.length\)return/);
  assert.match(shell, /root\.assignSavedBookmark\(root\.bookmarkChoiceHash,String\(found\.id\)\)\s*bookmarkCollectionChooser\.close\(\)/);
});

test('topic pagination advances by raw server rows, not deduplicated cast count', () => {
  const controller = read('ui/app/openproof/connections.cpp');
  const header = read('ui/app/openproof/connections.hpp');
  assert.match(header, /qsizetype m_topicCastOffset/);
  assert.match(controller, /m_topicCastOffset=0;/);
  assert.match(controller, /const auto offset=m_topicCastOffset;/);
  assert.match(controller, /m_topicCastOffset=offset\+rows\.size\(\);/);
  assert.match(controller, /m_topicCastsHasMore=rows\.size\(\)==pageSize;/);
  assert.doesNotMatch(controller, /const auto offset=m_topicCastItems\.size\(\)/);
});

test('topic discovery reuses canonical quote previews and hydrated bookmark cards', () => {
  assert.match(bridgeSource, /m_referencedPreviewCache\.constFind\(hash\)/);
  assert.match(bridgeSource, /applyHydratedTopicCast\(hash, referenced\.value\(\)\)/);
  assert.match(browse, /property var bookmarkedRows: \[\]/);
  assert.match(browse, /source===2 && !Boolean\(item\.richHydrated\)/);
  assert.match(shell, /bookmarkedRows: presentationBridge\.bookmarkedCasts/);
});

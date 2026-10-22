import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const read = relative => readFileSync(new URL('../../' + relative, import.meta.url), 'utf8');

test('Explorer For You keeps all recent root casts in provider order', () => {
    const qml = read('ui/components/FeedSurface.qml');
    const match = qml.match(/    function sourceRows\(\) \{([\s\S]*?)\n    \}\n\n    function rowKey\(/);
    assert.ok(match, 'sourceRows() must remain extractable for a regression simulation');
    const rows = [
        {castId: 'newest-nft', isReply: false, embeds: [{kind: 'image'}]},
        {castId: 'newest-discussion', isReply: false, replyCount: 4},
        {castId: 'nested-reply', isReply: true, parentCastId: 'parent'},
        {castId: 'latest-dev', isReply: false, text: 'https://github.com/example'},
        {castId: 'root-ordinary', isReply: false, text: 'hello'},
    ];
    const root = {
        rows, supplementalRows: [], socialMode: false, presentationMode: 'for-you',
        isRootPost: row => !row.isReply && !row.parentCastId,
    };
    const execute = new Function('root', 'function sourceRows() {' + match[1]
        + '\n}\nreturn sourceRows();');
    const result = execute(root);
    assert.deepEqual(result.map(value => value.payload.castId),
        ['newest-nft', 'newest-discussion', 'latest-dev', 'root-ordinary']);
});

test('Explorer Conversations uses verified network replies instead of text heuristics or root posts', () => {
    const qml = read('ui/components/FeedSurface.qml');
    const gate = qml.match(/    function isConversation\(data\) \{([\s\S]*?)\n    \}/);
    assert.ok(gate);
    const isConversation = new Function('data', gate[0] + '\nreturn isConversation(data);');
    assert.equal(isConversation({isReply: true, parentCastId: '0xparent'}), true);
    assert.equal(isConversation({text: 'Can anyone answer this question?'}), false);
    assert.equal(isConversation({replyCount: 25}), false);
    assert.match(qml, /root.activeFeed === "conversations" && root.isConversation\(row\)/);
    const gateway = read('src/providers/hypersnap/async_gateway.cpp');
    const decoder = read('src/providers/hypersnap/codec.cpp');
    const bridge = read('src/presentation/qt/presentation_bridge.cpp');
    const selector = bridge.split('void PresentationBridge::selectFeed(const QString& feed)')[1]
        .split('void PresentationBridge::openChannel')[0];
    assert.match(selector, /feed == QStringLiteral\("conversations"\) \? QStringLiteral\("conversations"\)/);
    assert.match(gateway, /feedConversationsRead\(\)/);
    assert.match(gateway, /const auto end = shard == 1U \? cursor.firstEnd : cursor.secondEnd/);
    assert.match(gateway, /codec::decodeRecentCastEvents\(\s*\*body, start, end, repliesOnly\)/);
    assert.match(gateway, /completeShard\(1U, std::move\(body\)\)/);
    assert.match(gateway, /completeShard\(2U, std::move\(body\)\)/);
    assert.match(decoder, /if \(isReply != repliesOnly\) \{ continue; \}/);
    assert.match(read('src/storage/read_store.cpp'), /"feed:conversations"/);
});

test('single ordinary link Cast composes author sentence inside the responsive card', () => {
    const cast = read('ui/components/CastItem.qml');
    const panel = read('ui/components/CastEmbedsPanel.qml');
    const embed = read('ui/components/EmbedPreview.qml');
    const preview = read('ui/components/LinkPreview.qml');
    assert.match(cast, /readonly property bool combinedLinkCast/);
    assert.match(cast, /active: root.displayText.length > 0 && !root.combinedLinkCast/);
    assert.match(cast, /castCaption: root.combinedLinkCast \? root.displayText : ""/);
    assert.match(panel, /castCaption: root.supportingCount === 1 && root.visualCount === 0/);
    assert.match(embed, /castCaption: root.castCaption/);
    assert.match(preview, /property string castCaption: ""/);
    assert.match(preview, /Layout.preferredWidth: shouldReserve[\s\S]*?Math.min\(root.compact \? 116 : 184/);
    assert.match(preview, /localizationController.isRightToLeftText\(root.castCaption\)/);
});

test('decoded images remain bounded but warm when recycled feed rows return', () => {
    const cache = read('ui/mobile/remote_image_provider.cpp');
    assert.match(cache, /imageCacheCapacityKiB = 96 \* 1024/);
    assert.match(cache, /imageCacheCapacityKiB = 128 \* 1024/);
    assert.match(cache, /rememberImage\(cacheKey, decoded.image, decoded.originalSize\)/);
    assert.match(read('ui/components/FeedSurface.qml'), /reuseItems: true/);
});

test('explicit Explorer refresh cannot hide new casts behind the background badge', () => {
    const shell = read('ui/components/AppShell.qml');
    const cpp = read('src/presentation/qt/presentation_bridge.cpp');
    const header = read('src/presentation/qt/presentation_bridge.hpp');
    assert.match(header, /Q_INVOKABLE void refreshLatestFeed\(\)/);
    assert.match(cpp, /void PresentationBridge::refreshLatestFeed\(\)[\s\S]*?m_bufferNextFeedRefresh = false;[\s\S]*?revealPendingPosts\(\);[\s\S]*?selectFeed\(m_activeFeed\)/);
    assert.match(shell, /else if \(\(refreshRequested[\s\S]*?presentationBridge.refreshLatestFeed\(\)/);
    assert.match(shell, /onRefreshRequested: root.primaryDestination === "explorer"[\s\S]*?presentationBridge.refreshLatestFeed\(\)/);
});

test('changing Explorer semantic tabs avoids immediate full ListModel destruction', () => {
    const qml = read('ui/components/FeedSurface.qml');
    const modes = qml.split('onPresentationModeChanged: {')[1].split('onVisualFilterChanged: {')[0];
    assert.doesNotMatch(modes, /stableRows.clear\(\)/);
    assert.match(qml, /if \(retained \* 4 < Math.min\(stableRows.count, source.length\)\)/);
    assert.match(qml, /root.rebuildStableModel\(source\)/);
});

test('reply intent always reaches the composer gate and failed canonical targets show feedback', () => {
    const engagement = read('ui/components/EngagementBar.qml');
    const shell = read('ui/components/AppShell.qml');
    assert.match(engagement, /onClicked: root.replyRequested\(\)/);
    assert.doesNotMatch(engagement, /onClicked: root.replyInteractive \? root.replyRequested/);
    assert.match(shell, /if \(!presentationBridge.isAuthenticated \|\| !presentationBridge.writeReady\) \{[\s\S]*?accountPopup.open\(\)/);
    assert.match(shell, /if \(!parentCast.castId \|\| !prepared\) \{[\s\S]*?replyTargetErrorLoader.active = true/);
    assert.match(shell, /objectName: "replyTargetErrorDialog"/);
    assert.match(shell, /onDidClose: Qt.callLater\(function\(\) \{ replyTargetErrorLoader.active = false \}\)/);
});

test('Explorer uses confirmed Recent; Home restores personalized Following or ranked Trending', () => {
    const shell = read('ui/components/AppShell.qml');
    const explorer = shell.split('function selectExplorerPresentationMode(mode, forceRefresh) {')[1]
        .split('function openExplorer() {')[0];
    const home = shell.split('function openHome() {')[1]
        .split('function selectHomePresentationMode(mode) {')[0];
    assert.match(explorer, /nextMode === "visuals" \? "nfts"/);
    assert.match(explorer, /nextMode === "devs" \? "developers"/);
    assert.match(explorer, /nextMode === "conversations" \? "conversations" : "recent"/);
    assert.match(readGenycasterMain(), /FeedQuery::Kind::Recent/);
    assert.match(read('src/storage/read_store.cpp'), /"feed:recent"/);
    assert.doesNotMatch(explorer, /const source = presentationBridge\.isAuthenticated/);
    assert.match(home, /\? "following" : "trending"/);
    assert.match(home, /presentationBridge\.activeFeed !== source/);
});

test('pending images defer texture admission while already presented frames stay resident', () => {
    const image = read('ui/components/SuperellipseImage.qml');
    const block = image.split('onScrollMovingChanged: {')[1]
        .split('onDeferNewLoadsWhileScrollingChanged:')[0];
    assert.match(block, /sourceStartTimer\.stop\(\)/);
    assert.match(block, /if \(!root\.ready && !root\.loadFailed && !root\.retainPreviousWhileLoading/);
    assert.match(block, /root\.activeSource = ""/);
    assert.match(block, /sourceStartTimer\.restart\(\)/);
});

test('link previews reconcile metadata without binding loops or geometry jumps', () => {
    const qml = read('ui/components/LinkPreview.qml');
    assert.match(qml, /property var previewData: \(\{\}\)/);
    assert.match(qml, /onTriggered: root\.previewData = root\.selectedPreviewData\(\)/);
    assert.match(qml, /readonly property real genericReservedHeight/);
    assert.match(qml, /Math\.max\(content\.implicitHeight \+ Theme\.space3 \* 2,/);
    assert.match(qml, /readonly property bool reserveArtwork: !root\.suppressArtwork\s*&& root\.targetUrl\.length > 0/);
    assert.match(qml, /readonly property var reservedLayout:/);
    assert.match(qml, /root\.browserMiniAppPreview\.providerKind === "miniapp"/);
});

test('Recent has independent native C++ cache, routing and Explorer presentation', () => {
    const header = read('ui/components/FeedHeader.qml');
    const empty = read('ui/components/EmptyState.qml');
    const shell = read('ui/components/AppShell.qml');
    const cpp = read('src/providers/hypersnap/async_gateway.cpp');
    assert.match(header, /root\.activeFeed === "recent"/);
    assert.match(header, /root\.discoveryFeed/);
    assert.match(empty, /root\.activeFeed === "recent"/);
    assert.match(shell, /nextMode === "visuals" \? "nfts"/);
    assert.match(shell, /nextMode === "devs" \? "developers"/);
    assert.match(shell, /nextMode === "conversations" \? "conversations" : "recent"/);
    assert.match(cpp, /recentFeed\(std::move\(query\), std::move\(completion\), options\)/);
    assert.match(cpp, /"\/v1\/events"/);
    assert.match(cpp, /"\/v2\/farcaster\/cast\/bulk"/);
    assert.match(cpp, /encodeRecentCursor/);
});

test('native UI and WebAssembly require Qt 6.12 and offer a pinned host preset', () => {
    const root = read('CMakeLists.txt');
    const presets = JSON.parse(read('CMakePresets.json'));
    assert.match(root, /find_package\(Qt6 6\.12 REQUIRED COMPONENTS/);
    assert.match(root, /qt_standard_project_setup\(REQUIRES 6\.12\)/);
    const preset = presets.configurePresets.find(item => item.name === 'ui-qt612-debug');
    assert.equal(preset.inherits, 'ui-debug');
    assert.equal(preset.cacheVariables.Qt6_DIR,
        '/opt/genycaster/Qt/6.12.0/gcc_64/lib/cmake/Qt6');
    assert.equal(preset.cacheVariables.CMAKE_CXX_COMPILER, '/opt/gcc-16.2.0/bin/c++');
});

test('video playback preserves production audio while headless Qt 6.12 QA avoids PipeWire', () => {
    const player = read('ui/components/GenyVideoPlayer.qml');
    const preview = read('ui/components/VideoPreview.qml');
    const testFixture = read('tests/ui/media_scroll_tests.cpp');
    assert.match(player, /property bool audioOutputEnabled: true/);
    assert.match(player, /id: audioOutput[\s\S]*?active: root\.audioOutputEnabled\s*&& \(typeof nativeAudioOutputEnabled === "undefined" \|\| nativeAudioOutputEnabled\)\s*&& \(activation\.activated \|\| \(root\.eager && root\.hasSource\)\)/);
    assert.match(player, /audioOutput: audioOutput\.item/);
    assert.match(preview, /property bool audioOutputEnabled: true/);
    assert.match(preview, /audioOutputEnabled: root\.audioOutputEnabled/);
    assert.match(testFixture, /QStringLiteral\("audioOutputEnabled"\), false/);
});

test('NFT and developer tabs have independent provider searches and cache keys', () => {
    const gateway = read('src/providers/hypersnap/async_gateway.cpp');
    const local = read('src/storage/read_store.cpp');
    const model = read('src/application/read_model.cppm');
    const surface = read('ui/components/FeedSurface.qml');
    assert.match(gateway, /spec\.path = "\/v2\/farcaster\/cast\/search"/);
    assert.match(gateway, /\{"q", query\.kind/);
    assert.match(gateway, /\? "art" : "github"/);
    assert.match(gateway, /collectibleSearchTerms\{/);
    assert.match(gateway, /"art", "mint", "opensea", "superrare", "foundation"/);
    assert.match(gateway, /encodeCollectibleCursor\(state\)/);
    assert.match(local, /"feed:nfts"/);
    assert.match(local, /"feed:developers"/);
    assert.match(model, /Nfts, Developers/);
    assert.match(surface, /root\.hasCollectible\(row\)/);
    assert.doesNotMatch(surface, /root\.activeFeed === "nfts" \|\| root\.hasCollectible\(row\)/);
    assert.match(surface, /root\.activeFeed === "developers"/);
});

test('story strip uses the current account Following graph, never public cast authors', () => {
    const shell = read('ui/components/AppShell.qml');
    const bridge = read('src/presentation/qt/presentation_bridge.hpp');
    const runtime = readGenycasterMain();
    const stories = read('ui/components/StoryStrip.qml');
    assert.match(shell, /recentProfiles: presentationBridge\.followingStoryProfiles/);
    assert.doesNotMatch(shell, /recentProfiles: root\.recentPosterProfiles\(presentationBridge\.rows, 8\)/);
    assert.match(bridge, /Q_PROPERTY\(QVariantList followingStoryProfiles/);
    assert.match(bridge, /followingStoriesRequested\(qulonglong viewerFid\)/);
    assert.match(runtime, /SocialQuery::Kind::Following/);
    assert.match(runtime, /keepFollowingStoriesRequest/);
    assert.match(stories, /implicitHeight: visible \?/);
});

test('loading placeholders have real layout sizes and fixed modest row height', () => {
    const skeleton = read('ui/components/CastSkeleton.qml');
    const surface = read('ui/components/FeedSurface.qml');
    assert.match(skeleton, /implicitHeight: mediaPreview \? 222 : 142/);
    assert.match(skeleton, /Layout\.preferredWidth: 100/);
    assert.match(skeleton, /Layout\.preferredHeight: 11/);
    assert.match(surface, /root\.presentationMode === "visuals" \? 2 : 3/);
});

test('mobile brand header keeps crisp native text without an offscreen elevation layer', () => {
    const header = read('ui/components/AppHeader.qml');
    assert.match(header, /layer\.enabled: false/);
    assert.match(header, /renderType: Text\.NativeRendering/);
    // Brand DNA is now a bare Icon, not a shadowed/tiled Superellipse.
    assert.match(header, /\/\/ Keep the DNA mark light and crisp/);
    assert.doesNotMatch(header, /elevation: Theme\.elevationNone/);
    assert.doesNotMatch(header, /fillColor: Theme\.primaryFill/);
});

test('stories always start with current identity; fresh Following casts use colored rings while older follows remain visible', () => {
    const bridge = read('src/presentation/qt/presentation_bridge.cpp');
    const strip = read('ui/components/StoryStrip.qml');
    const item = read('ui/components/StoryItem.qml');
    const header = read('ui/components/HomeFeedHeader.qml');
    const shell = read('ui/components/AppShell.qml');
    assert.match(bridge, /following\.prepend\(std::move\(own\)\)/);
    assert.match(bridge, /\{QStringLiteral\("isSelf"\), true\}/);
    assert.match(bridge, /QSet<qulonglong> seen/);
    assert.match(strip, /addStory: Boolean\(modelData\.isSelf\)/);
    assert.match(strip, /Date\.now\(\) - 86400000/);
    assert.match(strip, /root\.ownStoryRequested\(\)/);
    assert.match(header, /onOwnStoryRequested: root\.composeRequested\(\)/);
    assert.match(shell, /presentationBridge\.openProfileFid\(Number\(profile\.fid\)\)/);
    assert.doesNotMatch(item, /import QtQuick\.Shapes/);
    assert.doesNotMatch(item, /PathAngleArc|ShapePath|strokeColor: "#f45b42"/);
    assert.match(item, /borderWidth: root\.addStory \? 1 : \(root\.storyState === "active" \? 2 : 1\)/);
    assert.match(item, /borderColor: root\.addStory \|\| root\.storyState === "active"/);
    assert.match(item, /opacity: root\.storyState === "viewed".*0\.74/);
});

test('tapping ordinary cast text opens conversation without covering interactive media or controls', () => {
    const cast = read('ui/components/CastItem.qml');
    assert.match(cast, /id: castTextBody/);
    assert.match(cast, /TapHandler \{\s+acceptedButtons: Qt\.LeftButton/);
    assert.match(cast, /gesturePolicy: TapHandler\.DragThreshold/);
    assert.match(cast, /!root\.scrollMoving && castId\.length > 0/);
    assert.match(cast, /root\.openRequested\(castId\)/);
    assert.match(cast, /onTapped: root\.openMoreMenu\(\)/);
});

test('desktop quick draft keeps context menu; mobile opens the dedicated editor', () => {
    const home = read('ui/components/HomeDashboard.qml');
    const composer = read('ui/components/ComposerDrawer.qml');
    const menu = read('ui/components/EditorContextMenu.qml');
    assert.match(home, /ContextMenu\.menu: EditorContextMenu \{ editor: desktopDraftEditor \}/);
    assert.match(home, /objectName: "homeComposerLauncher"/);
    assert.match(home, /onClicked: root\.composeRequested\(root\.draftText\)/);
    assert.match(composer, /RichTextEditor \{/);
    assert.match(menu, /popupType: nativePresentation \? Popup\.Native : Popup\.Item/);
    assert.match(menu, /background: Superellipse/);
});

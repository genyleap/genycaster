import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

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
    assert.match(explorer, /nextMode === "devs" \? "developers" : "recent"/);
    assert.match(read('ui/mobile/main.cpp'), /FeedQuery::Kind::Recent/);
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
    assert.match(qml, /root\.reservedLayout\.artwork/);
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
    assert.match(shell, /nextMode === "devs" \? "developers" : "recent"/);
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
    assert.match(player, /id: audioOutput\s+active: root\.audioOutputEnabled/);
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
    assert.match(gateway, /\? "nft" : "github"/);
    assert.match(local, /"feed:nfts"/);
    assert.match(local, /"feed:developers"/);
    assert.match(model, /Nfts, Developers/);
    assert.match(surface, /root\.activeFeed === "nfts"/);
    assert.match(surface, /root\.activeFeed === "developers"/);
});

test('story strip uses the current account Following graph, never public cast authors', () => {
    const shell = read('ui/components/AppShell.qml');
    const bridge = read('src/presentation/qt/presentation_bridge.hpp');
    const runtime = read('ui/mobile/main.cpp');
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
    assert.match(header, /elevation: Theme\.elevationNone/);
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

test('home desktop and mobile draft editors use the same themed context menu as composer', () => {
    const home = read('ui/components/HomeDashboard.qml');
    const menu = read('ui/components/EditorContextMenu.qml');
    assert.match(home, /ContextMenu\.menu: EditorContextMenu \{ editor: desktopDraftEditor \}/);
    assert.match(home, /ContextMenu\.menu: EditorContextMenu \{ editor: mobileDraftEditor \}/);
    assert.match(menu, /popupType: Popup\.Item/);
    assert.match(menu, /background: Superellipse/);
});

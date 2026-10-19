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

test('Explorer uses network discovery; Home restores the personal following stream', () => {
    const shell = read('ui/components/AppShell.qml');
    const explorer = shell.split('function selectExplorerPresentationMode(mode, forceRefresh) {')[1]
        .split('function openExplorer() {')[0];
    const home = shell.split('function openHome() {')[1]
        .split('function selectHomePresentationMode(mode) {')[0];
    assert.match(explorer, /const source = "trending"/);
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

import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');

test('remote link, collectible, and miniapp art use the CORS-safe image provider', () => {
    const link = read('ui/components/LinkPreview.qml');
    const collectible = read('ui/components/CollectiblePreview.qml');
    assert.match(link, /readonly property string rawPreviewSource:/);
    assert.match(link, /linkPreviewResolver\.imageSourceForUrl\(source\)/);
    assert.match(collectible, /readonly property string rawArtworkSource:/);
    assert.match(collectible, /linkPreviewResolver\.imageSourceForUrl\(source\)/);
    assert.match(collectible, /forceAutomaticDownload: true/);
    assert.match(link, /forceAutomaticDownload: true/);
});

test('missing or failed web artwork and miniapp art collapse instead of giant empty poster', () => {
    const link = read('ui/components/LinkPreview.qml');
    const collectible = read('ui/components/CollectiblePreview.qml');
    assert.match(link, /root\.previewSource\.length > 0 && !root\.miniAppArtworkUnavailable/);
    assert.match(link, /root\.previewSource\.length > 0\s+&& !root\.richArtworkUnavailable/);
    assert.match(link, /&& !root\.genericArtworkUnavailable/);
    assert.match(link, /root\.compact \? 220 : 280/);
    assert.match(link, /root\.compact \? 238 : 328/);
    assert.match(collectible, /visible: root\.artworkSource\.length > 0 && !root\.artworkUnavailable/);
    assert.match(link, /onLoadFailedChanged: \{/);
    assert.match(link, /onPreviewSourceChanged: \{/);
    assert.match(collectible, /onArtworkSourceChanged: root\.artworkUnavailable = false/);
});

test('quote action travels from every cast list to a protocol cast-reference', () => {
    const bar = read('ui/components/EngagementBar.qml');
    const cast = read('ui/components/CastItem.qml');
    const shell = read('ui/components/AppShell.qml');
    const drawer = read('ui/components/ComposerDrawer.qml');
    const cpp = read('src/presentation/qt/rich_composer_controller.cpp');
    assert.match(bar, /objectName: "castQuoteButton"/);
    assert.match(cast, /onQuoteRequested: root\.quoteRequested\(root\.modelData\)/);
    for (const file of ['FeedSurface.qml', 'ProfileSurface.qml', 'DetailSurface.qml',
                        'SemanticFeedItem.qml', 'ConversationRow.qml', 'VisualPost.qml',
                        'DeveloperPost.qml', 'ThreadReplyItem.qml']) {
        const source = read('ui/components/' + file);
        assert.match(source, /signal quoteRequested\(var cast\)/, file);
        assert.match(source, /onQuoteRequested:/, file);
    }
    assert.match(shell, /function openQuoteComposer\(cast\)/);
    assert.match(shell, /onQuoteRequested: cast => root\.openQuoteComposer\(cast\)/);
    assert.match(drawer, /richComposer\.insertCastReference\(String\(root\.quotedCast\.castId/);
    assert.match(cpp, /bool RichComposerController::insertCastReference/);
    assert.match(cpp, /\.kind = BlockKind::CastReference/);
    assert.match(cpp, /\.author = farcaster::domain::identity::Fid\{authorFid\}/);
});

test('unknown impressions never become fabricated zero views', () => {
    const bar = read('ui/components/EngagementBar.qml');
    assert.match(bar, /property var viewCount: null/);
    assert.match(bar, /visible: root\.viewCount !== null && root\.viewCount !== undefined/);
    assert.match(read('ui/components/CastItem.qml'),
        /viewCount: root\.modelData\.viewCount === undefined \? null : root\.modelData\.viewCount/);
});

test('newest activity is tinted and grouped actors get a separate nonoverlapping portrait row', () => {
    const qml = read('ui/components/NotificationSurface.qml');
    assert.match(qml, /readonly property bool latestActivity:/);
    assert.match(qml, /notificationRow\.latestActivity \? root\.kindTint\(notificationRow\.kindKey\)/);
    assert.match(qml, /id: actorStack/);
    assert.match(qml, /model: actorStack\.visible \? actorStack\.count : 0/);
    assert.match(qml, /visible: notificationRow\.visibleActors\.length <= 1/);
});

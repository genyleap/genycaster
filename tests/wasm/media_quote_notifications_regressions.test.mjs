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
    assert.match(link, /root\.miniAppArtworkUnavailable \? \(root\.compact \? 156 : 176\) : readyHeight/);
    assert.match(link, /root\.previewSource\.length > 0\s+&& !root\.richArtworkUnavailable/);
    assert.match(link, /&& !root\.genericArtworkUnavailable/);
    assert.match(link, /root\.compact \? 420 : 640/);
    assert.match(link, /root\.compact \? 238 : 328/);
    assert.match(collectible, /visible: root\.artworkSource\.length > 0/);
    assert.doesNotMatch(collectible, /root\.artworkUnavailable/);
    assert.match(link, /onLoadFailedChanged: \{/);
    assert.match(link, /onPreviewSourceChanged: \{/);
    assert.match(collectible, /showFailureIndicator: true/);
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
        assert.doesNotMatch(source, /function on[A-Za-z]+:/, file);
    }
    assert.match(shell, /function openQuoteComposer\(cast\)/);
    assert.match(shell, /onQuoteRequested: cast => root\.openQuoteComposer\(cast\)/);
    assert.match(drawer, /richComposer\.insertCastReference\(String\(root\.quotedCast\.castId/);
    assert.match(cpp, /bool RichComposerController::insertCastReference/);
    assert.match(cpp, /\.kind = BlockKind::CastReference/);
    assert.match(cpp, /\.author = farcaster::domain::identity::Fid\{authorFid\}/);
});


test('marketplace artwork retries through the CORS-safe browser metadata provider', () => {
    const embed = read('ui/components/EmbedPreview.qml');
    const art = read('ui/components/CollectiblePreview.qml');
    assert.match(embed, /function requestBrowserMarketplacePreview\(\)/);
    assert.match(embed, /storage-farcaster\.genyleap\.com\/v1\/browser\/link-preview\?url=/);
    assert.match(embed, /marketplacePreviewTimer\.restart\(\)/);
    assert.match(embed, /root\.acceptLinkMetadata\(result\)/);
    assert.match(art, /forceAutomaticDownload: true/);
    assert.match(art, /Math\.min\(360, Math\.max\(260, width \* 0\.39\)\)/);
    assert.match(art, /onClicked: root\.openRequested\(root\.targetUrl\)/);
    assert.doesNotMatch(art, /root\.artworkUnavailable/);
});

test('miniapp posters keep useful original artwork size with better inset spacing', () => {
    const link = read('ui/components/LinkPreview.qml');
    assert.match(link, /Math\.min\(root\.compact \? 420 : 640,/);
    assert.match(link, /anchors\.margins: root\.layoutMiniApp \? Theme\.space4 : Theme\.space3/);
    assert.match(link, /spacing: root\.layoutMiniApp \? Theme\.space3 : Theme\.space2/);
});

test('search text fields use the themed editor context menu', () => {
    const input = read('ui/components/DirectionAwareTextField.qml');
    const search = read('ui/components/SearchPalette.qml');
    assert.match(input, /ContextMenu\.menu: EditorContextMenu \{ editor: root \}/);
    assert.match(search, /DirectionAwareTextField \{\s+id: searchField/);
});

test('supported locales contain every public English translation key', () => {
    const en = read('ui/i18n/qml_en_US.ts');
    const ids = [...en.matchAll(/<message id="([^"]+)">/g)].map(x => x[1]);
    for (const code of ['ar_SA','de_DE','es_ES','fa_IR','fr_FR',
                        'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const ts = read('ui/i18n/qml_' + code + '.ts');
        const actual = new Set([...ts.matchAll(/<message id="([^"]+)">/g)].map(x => x[1]));
        for (const id of ids)
            assert.ok(actual.has(id), code + ' missing ' + id);
    }
});

test('unknown impressions never become fabricated zero views', () => {
    const bar = read('ui/components/EngagementBar.qml');
    assert.match(bar, /property var viewCount: null/);
    assert.match(bar, /visible: root\.resolvedViewCount !== null && root\.resolvedViewCount !== undefined/);
    assert.match(read('ui/components/CastItem.qml'),
        /viewCount: root\.modelData\.viewCount === undefined \? null : root\.modelData\.viewCount/);
});

test('newest activity is tinted and grouped actors get a separate nonoverlapping portrait row', () => {
    const qml = read('ui/components/NotificationSurface.qml');
    assert.match(qml, /readonly property bool latestActivity:/);
    assert.match(qml, /notificationRow\.latestActivity \? Theme.surfaceSelected/);
    assert.match(qml, /return Theme\.surfaceSelected/);
    assert.match(qml, /return Theme\.accent/);
    assert.match(qml, /avatarSpacing: root\.compact \? 7 : 10/);
    assert.match(qml, /x: index \* \(actorStack\.avatarDiameter \+ actorStack\.avatarSpacing\)/);
    assert.match(qml, /id: actorStack/);
    assert.match(qml, /model: actorStack\.visible \? actorStack\.count : 0/);
    assert.match(qml, /visible: notificationRow\.visibleActors\.length <= 1/);
});

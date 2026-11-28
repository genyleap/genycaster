import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');

test('Feed notice labels never bind their own height to their implicit height', () => {
    const feed = read('ui/components/feed/FeedSurface.qml');
    for (const [wrapper, label] of [
        ['topicFilterNotice', 'topicFilterText'],
        ['localRecentNotice', 'localRecentText']
    ]) {
        assert.match(feed, new RegExp('Item \\{\\s+id: ' + wrapper + '[\\s\\S]*?height: visible \\? '
            + label + '\\.implicitHeight \\+ Theme\\.space[23] : 0'));
        assert.match(feed, new RegExp('DirectionAwareLabel \\{\\s+id: ' + label + '\\s+anchors\\.left: parent\\.left'));
        const item = feed.slice(feed.indexOf('id: ' + wrapper), feed.indexOf('id: ' + label));
        assert.doesNotMatch(item, /height: visible \? implicitHeight/, 'no self-referential Label geometry');
    }
    assert.match(feed, /implicitHeight: feedHeaderLoader\.implicitHeight \+ topicFilterNotice\.height/);
});

test('Magazine Profile CastItem uses actual painted height and supports rich code blocks', () => {
    const profile = read('ui/components/profile/ProfileSurface.qml');
    const profileMagazine = profile.slice(profile.indexOf('id: profileMagazineView'));
    assert.match(profileMagazine, /delegate: Component \{\s*CastItem \{\s*width: parent\.width[\s\S]*?height: implicitHeight/);
    assert.match(profileMagazine, /sharedMediaGeometry: profileMagazineView/);
    assert.match(profileMagazine, /modelData: \{[\s\S]*?const revision = parent\.revisionKey/);
    assert.match(profileMagazine, /onMovementEnded: \{[\s\S]*?root\.scheduleLoadMore\(\)/);
    const cast = read('ui/components/feed/CastItem.qml');
    assert.match(cast, /implicitHeight: messageRow\.implicitHeight/);
    const code = read('ui/components/content/CodeBlockView.qml');
    assert.match(code, /implicitHeight: Math\.min\(Math\.max/);
});

test('Magazine shows a bounded skeleton for truly visible slots awaiting their delegate', () => {
    const wall = read('ui/components/feed/FeedMasonryView.qml');
    const placeholder = wall.slice(wall.indexOf('id: visiblePlaceholder'),
        wall.indexOf('id: cardLoader'));
    assert.match(placeholder, /active: requested/);
    assert.match(placeholder, /needed: slot\.placed && slot\.intersectsViewport\s*&& !slot\.cardReady/);
    assert.match(placeholder, /visible: slot\.intersectsViewport/);
    assert.match(wall, /y \+ height > view\.contentY && y < view\.contentY \+ view\.height/);
    assert.match(placeholder, /!slot\.cardReady \|\| opacity > 0\.01/);
    assert.match(wall, /readonly property bool contentReady: cardLoader\.status === Loader\.Ready[\s\S]*?paintedHeight > 0/);
    assert.match(wall, /readonly property bool cardReady: cardLoader\.status === Loader\.Ready\s*&& loadedCard !== null && cardPresented/);
    assert.match(placeholder, /opacity: slot\.cardReady \? 0 : 1/);
    assert.match(placeholder, /sourceComponent: Component \{\s*Item \{/);
    assert.match(placeholder, /Superellipse \{\s*anchors\.fill: parent\s*fillColor: Theme\.surfaceRaised/);
    assert.match(placeholder, /CastSkeleton \{[\s\S]*?height: Math\.min\(parent\.height, implicitHeight\)/);
    assert.match(placeholder, /animate: visiblePlaceholder\.visible && !view\.moving/);
    assert.match(wall, /active: slot\.admitted/, 'the real card uses sampled admission, not geometry bindings');
    assert.match(wall, /clip: true/);
    assert.match(wall, /height: Math\.max\(1, cardHeight\)/);
    const slotSource = wall.slice(wall.indexOf('required property real cardHeight'),
        wall.indexOf('id: visiblePlaceholder'));
    assert.doesNotMatch(slotSource, /onHeightChanged: view\.scheduleLayout\(\)/,
        'native numeric geometry owns each card slot height');
});

test('Rich text and animated sprite fallback cannot abandon nested asynchronous incubators', () => {
    const cast = read('ui/components/feed/CastItem.qml');
    const textBlock = cast.slice(cast.indexOf('id: castTextBody'), cast.indexOf('objectName: "castVisualMedia"'));
    assert.match(textBlock, /asynchronous: false/);
    const rich = read('ui/components/content/RichCastRenderer.qml');
    assert.match(rich, /delegate: Loader \{[\s\S]*?asynchronous: false/);
    const inline = read('ui/components/content/RichInlineText.qml');
    const sprite = inline.slice(inline.indexOf('id: spriteEmojiLoader'));
    assert.match(sprite, /asynchronous: false/);
    assert.match(sprite, /active: emojiOverlay\.spriteFallbackArmed && !lottieEmoji\.animationReady/);
});

test('Network body consumers never read a closed QIODevice during cancellation', () => {
    const sync = read('src/transport/qt_network/http.cpp');
    const async = read('src/transport/qt_network/async_http.cpp');
    const probes = read('src/providers/hypersnap/node_selection_controller.cpp');
    assert.match(sync, /cancelled \|\| timedOut \|\| !reply->isOpen\(\) \|\| !reply->isReadable\(\)/);
    assert.match(async, /!active\.reply \|\| active\.timedOut \|\| !active\.reply->isOpen\(\)/);
    assert.match(probes, /reply->error\(\) == QNetworkReply::NoError && reply->isOpen\(\)/);
});

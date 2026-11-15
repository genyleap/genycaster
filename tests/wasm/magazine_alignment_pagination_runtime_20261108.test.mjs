import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";

const read = path => readFileSync(new URL("../../" + path, import.meta.url), "utf8");
const cast = read("ui/components/feed/CastItem.qml");
const wall = read("ui/components/feed/FeedMasonryView.qml");
const feed = read("ui/components/feed/FeedSurface.qml");
const profile = read("ui/components/profile/ProfileSurface.qml");
const controls = read("ui/components/feed/ExplorerLayoutControls.qml");

test("Empty client attribution never paints a circular badge beside the timestamp", () => {
    const header = cast.slice(cast.indexOf('objectName: "castSourceClientBadge"'),
                              cast.indexOf('objectName: "magazineCastTime"'));
    assert.match(header, /visible: sanitizedName\.length > 0\s*&& \(!root\.magazineCard \|\| root\.width >= 390\)/);
    assert.match(cast, /objectName: "magazineCastTime"[\s\S]*?Layout\.alignment: Qt\.AlignVCenter/);
    assert.match(cast, /id: moreButton[\s\S]*?Layout\.alignment: root\.magazineCard \? Qt\.AlignVCenter/);
});

test("Magazine/Timeline segments have the same optical width and symmetric text/icon layout", () => {
    assert.match(controls, /readonly property real segmentWidth: Math\.max\(\s*magazineChoice\.implicitWidth, timelineChoice\.implicitWidth\)/);
    for (const [id, name] of [["magazineChoice", "explorerMagazineChoice"],
                              ["timelineChoice", "explorerTimelineChoice"]]) {
        const item = controls.slice(controls.indexOf("id: " + id),
                                     controls.indexOf('onClicked: root.layoutRequested("'
                                         + (id === "magazineChoice" ? "magazine" : "timeline") + '")'));
        assert.match(item, /Layout\.preferredWidth: segments\.segmentWidth/);
        assert.match(item, /Layout\.alignment: Qt\.AlignVCenter/);
        assert.match(item, /balancedIcon: !root\.condensed/);
    }
    assert.match(controls, /layoutDirection: Theme\.uiDirection/);
    assert.match(controls, /onClicked: root\.layoutRequested\("magazine"\)/);
    assert.match(controls, /onClicked: root\.layoutRequested\("timeline"\)/);
});

test("Rich Cast paint height tracks its implicit text/media geometry through each delegate layer", () => {
    const semantic = read("ui/components/feed/SemanticFeedItem.qml");
    assert.match(cast, /implicitHeight: messageRow\.implicitHeight[\s\S]*?height: implicitHeight/);
    assert.match(cast, /id: messageRow[\s\S]*?anchors\.bottomMargin: Theme\.space1\s*height: implicitHeight/);
    assert.match(semantic, /implicitHeight: loader\.item \? loader\.item\.implicitHeight : 0[\s\S]*?height: implicitHeight/);
    assert.match(semantic, /id: loader[\s\S]*?anchors\.top: parent\.top\s*height: root\.height/);
    const profileMagazine = profile.slice(profile.indexOf("id: profileMagazineView"));
    assert.match(profileMagazine, /delegate: Component \{\s*CastItem \{\s*width: parent\.width[\s\S]*?height: implicitHeight/);
    // During transient async relayout, the slot must block any content
    // from painting into the next card. Height grows from real painted
    // content and is then propagated into native lane geometry.
    assert.match(wall, /clip: true/);
    assert.match(cast, /readonly property real magazinePaintHeight:/);
    assert.match(semantic, /readonly property real magazinePaintHeight:/);
    assert.match(feed, /rowDelegate\.magazineRow \? Number\(item\.magazinePaintHeight \|\| 0\)/);
    assert.match(wall, /function onImplicitHeightChanged\(\) \{ slot\.recordHeight\(\) \}/);
});

test("The profile cover, bio, heatmap and Magazine card columns share one outer reading gutter", () => {
    assert.match(profile, /readonly property int profileContentWidth: Math\.max\(0, Math\.min\(/);
    assert.match(profile, /id: bannerFrame[\s\S]*?x: root\.profileContentX\s*width: root\.profileContentWidth/);
    assert.match(profile, /id: profileMagazineView[\s\S]*?outsideInset: root\.profileContentX/);
    assert.match(wall, /readonly property real usableWidth: Math\.max\(1, width - outsideInset \* 2\)/);
    assert.match(wall, /readonly property real leadingInset: outsideInset/);
    // 3-column wide profile uses precisely the header's 1280px reading width.
    for (const [width, inset, columns] of [[1320, 20, 3], [1010, 12, 2], [660, 16, 1]]) {
        const usableWidth = width - inset * 2;
        const cardWidth = (usableWidth - 12 * (columns - 1)) / columns;
        assert.equal(inset + columns * cardWidth + (columns - 1) * 12, width - inset);
    }
});

test("5- or 10-post pages hydrate underfilled magazine columns without unbounded auto-paging", () => {
    const start = feed.indexOf("    function isNearEnd() {");
    const end = feed.indexOf("    function needsFilteredPrefetch()", start);
    assert.ok(start > 0 && end > start);
    const src = feed.slice(start, end);
    const context = vm.createContext({
        root: {magazineEnabled: true},
        readList: {count: 5, contentHeight: 1500, atYEnd: false, contentY: 0,
                   height: 720, originY: 0},
        magazineView: {shortestColumnBottom: 350}
    });
    vm.runInContext(src, context);
    assert.equal(vm.runInContext("isNearEnd()", context), true,
        "a short lane below the fold is an actual fill-more trigger");
    context.readList.count = 10;
    context.readList.contentHeight = 1900;
    context.magazineView.shortestColumnBottom = 1320;
    assert.equal(vm.runInContext("isNearEnd()", context), false,
        "sufficiently filled lanes should not fetch another page by themselves");
    context.root.magazineEnabled = false;
    context.readList.count = 5;
    context.magazineView.shortestColumnBottom = 120;
    assert.equal(vm.runInContext("isNearEnd()", context), false,
        "timeline retains its original end-of-scroll behavior");

    const native = read("ui/app/social/feed_rows.cpp");
    assert.match(native, /m_shortestBottom = m_geometry\.size\(\) >= m_columns/);
    assert.match(wall, /readonly property real shortestColumnBottom: layoutModel\.shortestColumnBottom/);
    assert.match(feed, /if \(!magazineView\.moving && !scrollWorkThrottle\.running\)/);
    assert.match(feed, /readonly property int automaticPrefetchLimit: 16/);
    assert.match(feed, /readonly property int automaticNoProgressLimit: 2/);
});

test("Page size 5/10 is explicitly supported while 20 stays default, without forcing more card delegates", () => {
    const bridge = read("src/presentation/qt/presentation_bridge.cpp");
    const settings = read("ui/components/settings/SettingsPopup.qml");
    const runtime = read("ui/app/runtime/bindings.cpp");
    assert.match(bridge, /QStringLiteral\("read\/feedPageSize"\), 20\)\.toInt\(\), 1, 100/);
    assert.match(bridge, /void PresentationBridge::setFeedPageSize\(const int value\)[\s\S]*?std::clamp\(value, 1, 100\)/);
    assert.match(settings, /value: presentationBridge\.feedPageSize\s*fromValue: 1\s*toValue: 100/);
    assert.match(runtime, /static_cast<std::size_t>\(bridge\.feedPageSize\(\)\)/);
    assert.match(wall, /active: slot\.admitted/);
    const native = read("ui/app/social/feed_rows.cpp");
    assert.match(wall, /layoutModel\.setViewport\(contentY, height, cacheBuffer, retentionBuffer/);
    assert.match(native, /const qreal enterTop = m_viewportTop - m_enterBuffer/);
    assert.match(native, /const qreal maximum = m_viewportTop \+ m_viewportHeight \+ m_buffer/);
});

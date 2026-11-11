import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const read = name => readFileSync(new URL('../../' + name, import.meta.url), 'utf8');
const notifications = read('ui/components/feed/NotificationSurface.qml');

function qmlFunction(source, name, root) {
    const start = source.indexOf('    function ' + name + '(');
    assert.ok(start >= 0, 'missing ' + name);
    const end = source.indexOf('\n    }', start);
    assert.ok(end > start);
    return vm.runInNewContext('(' + source.slice(start, end + 6).trim() + ')', {root});
}

function groupingRoot() {
    const root = {};
    for (const key of ['asList', 'actorRows', 'cloneNotificationRow',
                       'mergeNotificationActors', 'actorsForMerge',
                       'mergeNotificationInto', 'finalizeNotificationGroup',
                       'groupNotificationRows', 'notificationKey',
                       'notificationImageUrl']) {
        root[key] = qmlFunction(notifications, key, root);
    }
    return root;
}

test('reaction notifications for one cast share one stable row and a truthful actor count',()=>{
    const root=groupingRoot();
    const a={id:'first',kind:'reaction',castId:'0xfeed',
        actorFid:11,actor:'alice',actorDisplayName:'Alice',aggregateCount:1,
        castLikeCount:900, observedAtMs:100};
    const b={id:'second',kind:'reaction',castId:'0xfeed',
        actorFid:12,actor:'bob',actorDisplayName:'Bob',aggregateCount:1,
        castLikeCount:900, observedAtMs:200};
    const unrelated={id:'reply',kind:'reply',castId:'0xother',actorFid:33};
    const result=root.groupNotificationRows([a,unrelated,b]);
    assert.equal(result.length,2,'the same target cast should have one reaction row');
    assert.equal(result[0].aggregateCount,2,
        '900 total cast likes are NOT 900 notification actors');
    assert.equal(result[0].actors.length,2);
    assert.deepEqual(Array.from(result[0].actors,a=>Number(a.fid)),[11,12]);
    assert.equal(result[0].observedAtMs,200);
    assert.equal(result[1],unrelated);
    assert.equal(root.notificationKey(a,0),root.notificationKey(b,5));
});

test('provider-supplied group totals remain visible even with truncated actor portraits',()=>{
    const root=groupingRoot();
    const group={id:'group',kind:'reaction',castId:'0xfeed',
        actors:[{fid:11,username:'alice'},{fid:12,username:'bob'}],
        aggregateCount:18};
    const result=root.groupNotificationRows([group]);
    assert.equal(result[0].aggregateCount,18);
    assert.equal(result[0].actors.length,2);
    const cpp=read('ui/app/runtime/bindings.cpp');
    assert.match(cpp,/std::min<std::size_t>\(item\.actors\.size\(\), 16U\)/);
    assert.doesNotMatch(notifications,/target\.actors\s*=.*\.slice\(0, 8\)/);
});

test('CDN media without extensions shows an image instead of a bare hostname',()=>{
    const root=groupingRoot();
    const cdn='https://imagedelivery.net/opaque-id/image/width=640';
    assert.equal(root.notificationImageUrl(cdn),cdn);
    assert.equal(root.notificationImageUrl('https://example.org/page'),'');
    assert.equal(root.notificationImageUrl('javascript:alert(1)'),'');
    assert.match(notifications,/LinkPreview \{/);
    assert.match(notifications,/onOpenRequested: url => Qt\.openUrlExternally\(url\)/);
});

test('notification portraits route to the exact FID and groups use one counter row',()=>{
    assert.match(notifications,/id: actorPortraitSlot/);
    assert.match(notifications,/actorStack\.mapFromItem\(notificationRow, pos\.x, pos\.y\)/);
    assert.match(notifications,/root\.profileRequested\(fid\)/);
    assert.match(notifications,/id: actorCountBadge/);
    assert.match(notifications,/LocaleFormat\.formatInteger\(actorCountBadge\.groupCount\)/);
});

test('Explorer underline is owned by each selected tab and mirrors naturally',()=>{
    const tabs=read('ui/components/navigation/ContentTabs.qml');
    assert.match(tabs,/id: tabButton[\s\S]*?objectName: "contentTabUnderline"/);
    assert.match(tabs,/anchors\.horizontalCenter: parent\.horizontalCenter/);
    assert.match(tabs,/opacity: tabButton\.selected \? 1 : 0/);
    assert.doesNotMatch(tabs,/itemAt\(root\.selectedIndex\)/);
});

test('Updates are centered, bounded popups with stationary actions',()=>{
    const notice=read('ui/components/settings/UpdateNoticeDrawer.qml');
    assert.match(notice,/Popup \{/);
    assert.match(notice,/y: parent \? Math\.max\(0, \(parent\.height - height\) \/ 2\)/);
    assert.match(notice,/id: updateFixedChrome/);
    assert.match(notice,/anchors\.bottom: updateNoticeStickyActions\.top/);
    assert.doesNotMatch(notice,/edge: Qt\.TopEdge/);
});

test('Settings uses shared responsive navigation and Home has functional activity actions',()=>{
    const settings=read('ui/components/settings/SettingsPopup.qml');
    const home=read('ui/components/home/HomeDashboard.qml');
    assert.match(settings,/component SettingsNavigationTab: Button/);
    assert.equal((settings.match(/delegate: SettingsNavigationTab \{/g)||[]).length,3);
    assert.match(settings,/Math\.min\(920, Math\.max\(0, parent\.width - Theme\.space4 \* 2\)\)/);
    assert.match(home,/component HomeActivityCard: Superellipse/);
    assert.match(home,/root\.notificationUnreadCount > 0/);
    assert.match(home,/onClicked: root\.notificationsRequested\(\)/);
    assert.match(home,/onClicked: root\.exploreRequested\(\)/);
    assert.match(home,/onClicked: root\.composeRequested\(root\.draftText\)/);
    assert.doesNotMatch(home,/interval: 6200/);
});

test('cast text, media and details use a consistent, compact vertical cadence',()=>{
    const rich=read('ui/components/content/RichCastRenderer.qml');
    const detail=read('ui/components/feed/DetailSurface.qml');
    const cast=read('ui/components/feed/CastItem.qml');
    assert.match(rich,/spacing: Theme\.space1/);
    assert.match(detail,/spacing: Theme\.space2[\s\S]*?Contextual conversation toolbar/);
    assert.match(cast,/spacing: root\.mediaFirst \? Theme\.space1 : Theme\.space2/);
});

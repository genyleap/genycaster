import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');
const indicator = read('ui/components/PullToRefresh.qml');
const notification = read('ui/components/NotificationSurface.qml');
const shell = read('ui/components/AppShell.qml');
const wheelMain = read('ui/app/main.cpp');
const wheelFallback = read('ui/app/browser_wheel_fallback.hpp');

test('Chrome wheel bridge consumes handled DOM events once and preserves subpixel ticks', () => {
    assert.match(wheelMain, /window\.__genycasterWheelEvent = event/);
    assert.match(wheelMain, /e\.stopImmediatePropagation\(\)/);
    assert.match(wheelMain, /e\.cancelable\) e\.preventDefault\(\)/);
    assert.match(wheelMain, /static QPointF wheelRemainder\{\}/);
    assert.match(wheelMain, /pixelsForAxis\(-event->deltaY \* scale, wheelRemainder\.ry\(\)\)/);
    assert.match(wheelFallback, /minimum = originValue\.isValid\(\)/);
    assert.match(wheelFallback, /std::clamp\(previous \+ delta, minimum, maximum\)/);
});

test('pull-to-refresh arms only for a real top-origin drag and emits one refresh on release', () => {
    assert.match(indicator, /objectName: "pullToRefresh"/);
    assert.match(indicator, /control\.flickable\.dragging/);
    assert.match(indicator, /control\.flickable\.contentY <= control\.flickable\.originY \+ 4/);
    assert.match(indicator, /pulledDistance >= control\.activationDistance/);
    assert.match(indicator, /const shouldRefresh = control\.armed && control\.refreshEnabled && !control\.busy/);
    assert.match(indicator, /if \(shouldRefresh\)\s+control\.refreshRequested\(\)/);
    assert.match(indicator, /control\.startedAtTop = false/);
    for (const file of ['FeedSurface','ProfileSurface','NotificationSurface']) {
        const source = read('ui/components/' + file + '.qml');
        assert.match(source, /boundsBehavior: Flickable\.DragOverBounds/);
        assert.match(source, /boundsMovement: Flickable\.FollowBoundsBehavior/);
        assert.match(source, file === 'ProfileSurface'
            ? /PullToRefresh \{[\s\S]*?onRefreshRequested: root\.refreshProfileAndAccount\(\)/
            : /PullToRefresh \{[\s\S]*?onRefreshRequested: root\.refreshRequested\(\)/);
    }
    assert.match(shell, /onRefreshRequested: presentationBridge\.openNotifications\(\)/);
    assert.match(shell, /onRefreshRequested: root\.topicBrowseActive && root\.topicBrowseScope==="explorer"[\s\S]*?root\.primaryDestination === "explorer"[\s\S]*?presentationBridge\.refreshLatestFeed\(\)/);
    assert.match(shell, /onRefreshRequested: presentationBridge\.selectProfileFeed\(presentationBridge\.profileFeed\)/);
    assert.match(read('ui/CMakeLists.txt'), /components\/PullToRefresh.qml/);
});

test('notification categories classify only provider-backed channel data, mention and reply', () => {
    const start = notification.indexOf('function mentionsViewer(row) {');
    const end = notification.indexOf('readonly property var visibleRows:', start);
    assert.ok(start >= 0 && end > start);
    const context = vm.createContext({root: {accountProfile: {username: 'compez'}}});
    vm.runInContext(notification.slice(start, end)
        + '\nroot.mentionsViewer = mentionsViewer; root.belongsToCategory = belongsToCategory;', context);
    const matcher = context.root.belongsToCategory;
    const event = (kind, castChannel = '', castText = '') => ({kind, castChannel, castText});
    for (const kind of ['reaction','recast','quote','follow','reply','mention']) {
        assert.equal(matcher(event(kind), 'all'), true);
    }
    assert.equal(matcher(event('reply'), 'mention'), false,
        'a reply without @username belongs in Activity, not Mentions');
    assert.equal(matcher(event('reply', '', 'Hello @compez!'), 'mention'), true);
    assert.equal(matcher(event('reply', '', 'Hello @other'), 'mention'), false);
    assert.equal(matcher(event('reaction', '', 'Hello @compez'), 'mention'), false);
    assert.equal(matcher(event('reaction', '', 'Hello @compezeth'), 'mention'), false);
    assert.equal(matcher(event('reply', '', '@compezeth'), 'mention'), false);
    assert.equal(matcher(event('mention'), 'mention'), true);
    assert.equal(matcher(event('reaction'), 'mention'), false);
    assert.equal(matcher(event('follow'), 'channels'), false);
    assert.equal(matcher(event('mention', 'design'), 'channels'), true);
    assert.equal(matcher(event('reply','music'), 'channels'), true);
    assert.equal(matcher(event('reaction', 'crypto'), 'channels'), true);
    assert.equal(matcher(event('unknown'), 'channels'), false);
    assert.match(notification, /z: root\.showState \? -1 : 0/,
      'empty-state sign-in and retry buttons must remain clickable above the refresh viewport');
    assert.match(notification, /kind: "channels", title: qsTrId\("genycaster\.notifications\.filter_channels"\)/);
    assert.doesNotMatch(notification, /kind: "reactions", title:/);
    for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                          'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const source = read('ui/i18n/qml_' + locale + '.ts');
        assert.equal(source.split('id="genycaster.notifications.filter_channels"').length - 1, 1);
    }
});

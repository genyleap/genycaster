import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('refresh indicator appears in the pulled gap and disappears on release', () => {
    const source = read('ui/components/PullToRefresh.qml');
    assert.match(source, /y: Math\.max\(0, \(offset - height\) \* 0\.5\)/);
    assert.match(source, /visible: startedAtTop && offset >= height/);
    assert.doesNotMatch(source, /visible: \(busy && flickable/);
    assert.match(source, /control\.startedAtTop = false/);
});

test('mobile apps reserve a visible empty state below the destination tabs', () => {
    const apps = read('ui/components/AppsSurface.qml');
    assert.match(apps, /root\.visibleApps\.length === 0[\s\S]*?Math\.max\(280, Math\.min\(400, appsScroller\.height \* 0\.43\)\)/);
    assert.match(apps, /anchors\.centerIn: parent/);
});

test('search handles outside taps and offers explicit clearing of results', () => {
    const palette = read('ui/components/SearchPalette.qml');
    const backdrop = read('ui/components/DrawerBackdrop.qml');
    assert.match(palette, /closePolicy: Popup\.CloseOnEscape \| Popup\.CloseOnPressOutside/);
    assert.match(backdrop, /TapHandler \{[\s\S]*?onTapped: root\.requestOutsideDismiss\(\)/);
    assert.match(palette, /function clearResults\(\) \{[\s\S]*?resultRows\.clear\(\)/);
    assert.match(palette, /root\.suggestionsCleared\(\)/);
    assert.match(palette, /objectName: "clearSearchResults"/);
    assert.match(palette, /onClicked: root\.clearResults\(\)/);
});

test('mobile dock has six destinations and a separate configurable Action Button', () => {
    const nav = read('ui/components/BottomNavigation.qml');
    const shell = read('ui/components/AppShell.qml');
    assert.match(nav, /objectName: "mobileNavigationCapsule"/);
    assert.match(nav, /objectName: "mobilePrimaryActionButton"/);
    assert.doesNotMatch(nav, /objectName: "mobileFooterBackground"/);
    assert.doesNotMatch(nav, /objectName: "mobileContentLowerEdge"/);
    assert.doesNotMatch(nav, /footerColor|footerBackground|contentLowerEdge/);
    for(const id of ['mobileDockHome','mobileDockExplore','mobileDockApps',
                      'mobileBottomBookmarksButton','mobileDockWidgets','mobileDockProfile'])
        assert.match(nav, new RegExp('objectName: "'+id+'"'));
    assert.match(nav, /Layout\.preferredWidth: 52/);
    assert.match(nav, /anchors\.topMargin: Qt\.platform\.os === "android" \? 4 : 8/);
    assert.match(nav, /Theme\.mobileActionColor/);
    assert.match(nav, /Theme\.mobileActionIcon/);
    assert.match(nav, /case "photo": root\.photoRequested\(\)/);
    assert.match(nav, /case "search": root\.searchRequested\(\)/);
    assert.match(nav, /case "settings": root\.settingsRequested\(\)/);
    assert.match(nav, /default: root\.createRequested\(\)/);
    assert.match(shell, /id: mobileBottomNavigation[\s\S]*?height: Qt\.platform\.os === "android" && root\.mobile \? 74 : 88/);
    assert.match(shell, /onPhotoRequested: root\.openComposer\(null, "media"\)/);
    assert.match(shell, /onSearchRequested: searchPalette\.open\(\)/);
    assert.match(shell, /anchors\.left: readingPane\.left\s+anchors\.right: readingPane\.right\s+anchors\.bottom: readingPane\.bottom/);
});

test('mentioned content and search controls have localized labels', () => {
    const ui = read('ui/components/NotificationSurface.qml');
    assert.match(ui, /notificationRow\.kindKey === "mention"[\s\S]*?Layout\.fillWidth: true/);
    assert.match(ui, /genycaster\.notifications\.content_unavailable/);
    for (const locale of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                          'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
        const source = read('ui/i18n/qml_' + locale + '.ts');
        assert.equal(source.split('id="genycaster.notifications.content_unavailable"').length - 1, 1);
        assert.equal(source.split('id="genycaster.search.clear_results"').length - 1, 1);
    }
});

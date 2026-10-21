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
    assert.match(backdrop, /TapHandler \{[\s\S]*?onTapped: root\.backgroundTapped\(\)/);
    assert.match(palette, /function clearResults\(\) \{[\s\S]*?resultRows\.clear\(\)/);
    assert.match(palette, /root\.suggestionsCleared\(\)/);
    assert.match(palette, /objectName: "clearSearchResults"/);
    assert.match(palette, /onClicked: root\.clearResults\(\)/);
});

test('Chrome-inspired mobile dock stays curved and high contrast without live WASM blur', () => {
    const nav = read('ui/components/BottomNavigation.qml');
    const button = read('ui/components/BottomNavButton.qml');
    assert.match(nav, /Qt\.platform\.os !== "wasm"/);
    assert.match(nav, /fillColor: Qt\.rgba\(0\.105, 0\.115, 0\.145, 0\.97\)/);
    assert.match(nav, /curveDepth: height \/ 2/);
    assert.equal(nav.split('darkDock: true').length - 1, 5);
    assert.match(button, /darkDock \? "#8bb2ff"/);
    assert.match(read('ui/components/AppShell.qml'), /id: mobileBottomNavigation[\s\S]*?height: 66/);
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

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

test('mobile footer is full bleed dark, with curved page edge and no floating capsule', () => {
    const nav = read('ui/components/BottomNavigation.qml');
    const shell = read('ui/components/AppShell.qml');
    const button = read('ui/components/BottomNavButton.qml');
    const html = read('ui/wasm/genycaster-shell.html');
    assert.match(nav, /id: footerBackground[\s\S]*?anchors\.fill: parent[\s\S]*?color: root\.footerColor/);
    assert.match(nav, /footerColor: "#202128"/);
    assert.match(nav, /id: contentLowerEdge[\s\S]*?curveBottomLeft: true[\s\S]*?curveBottomRight: true/);
    assert.match(nav, /curveTopLeft: false[\s\S]*?curveTopRight: false/);
    assert.doesNotMatch(nav, /id: dockSurface|glassCapture|MultiEffect|shadowEnabled: true|borderWidth: 1/);
    assert.equal(nav.split('darkDock: true').length - 1, 5);
    assert.match(button, /darkDock \? "#8bb2ff"/);
    assert.match(shell, /id: mobileBottomNavigation[\s\S]*?height: 88/);
    assert.match(shell, /anchors\.left: readingPane\.left\s+anchors\.right: readingPane\.right\s+anchors\.bottom: readingPane\.bottom/);
    assert.doesNotMatch(shell, /anchors\.bottomMargin: Math\.max\(Theme\.space4, Theme\.mobileGutter\)/);
    assert.match(html, /@media\(max-width:700px\)\{html,body\{background:linear-gradient\([\s\S]*?#202128/);
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

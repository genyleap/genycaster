import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

test('Neon Kit shaders are compiled for the Qt resource module', () => {
    const cmake = read('ui/CMakeLists.txt');
    assert.match(cmake, /qt_add_resources\(genycaster genycaster_surface_effects/);
    assert.match(cmake, /PREFIX "\/genycaster\/effects"/);
    for (const effect of ['gaussian', 'neon'])
        assert.match(cmake, new RegExp('shaders/' + effect + '\\.frag\\.qsb'));
    for (const effect of ['gaussian', 'neon'])
        assert.ok(existsSync('ui/shaders/' + effect + '.frag.qsb'));
    assert.match(read('ui/shaders/gaussian.frag'), /for \(int i = 1; i <= 47; i \+= 2\)/);
    assert.match(read('ui/shaders/neon.frag'), /superellipseDistance/);
    assert.match(read('ui/components/visual/NeonHalo.qml'), /neon\.frag\.qsb/);
});

test('shared overlay freezes workspace only and never captures the overlay tree', () => {
    const blur = read('ui/components/visual/DrawerBackdrop.qml');
    assert.match(blur, /property Item sourceItem: null/);
    assert.match(blur, /recursive: false/);
    assert.match(blur, /live: false/);
    assert.match(blur, /hideSource: root\.nativeGaussian/);
    assert.match(blur, /sourceItem: root\.sourceItem/);
    assert.match(blur, /id: horizontalPass/);
    assert.match(blur, /id: intermediate/);
    assert.match(blur, /gaussian\.frag\.qsb/g);
    assert.match(blur, /sourceItem: horizontalPass/);
    assert.match(blur, /root\.useBrowserSnapshot/);
    assert.match(blur, /root\.refreshSnapshot\(\)/);
    assert.doesNotMatch(blur, /sourceItem:\s*(?:Window\.contentItem|Overlay\.overlay)/);
});

test('modal and drawer share frosted scrims without overriding closure policy', () => {
    const modal = read('ui/components/controls/GenyModal.qml');
    const drawer = read('ui/components/controls/GenyDrawer.qml');
    const adaptive = read('ui/components/controls/GenyAdaptiveDialog.qml');
    const wallet = read('ui/components/settings/WalletBrowserPicker.qml');
    const emoji = read('ui/components/composer/EmojiPicker.qml');
    for (const source of [modal, drawer, adaptive, wallet, emoji])
        assert.match(source, /Overlay\.modal: DrawerBackdrop/);
    assert.match(modal, /control\.closePolicy & Popup\.CloseOnPressOutside/);
    assert.match(modal, /parent !== Overlay\.overlay/);
    assert.match(drawer, /addScrim: control\.dimBackground \|\| control\.blurSourceItem !== null/);
    assert.match(wallet, /closePolicy: Popup\.NoAutoClose/);
    assert.match(wallet, /Intentionally no onBackgroundTapped/);
    assert.match(wallet, /snapshotBlurOnBrowser: true/);
});

test('Neon is a persistent, power-aware, reduced-motion aware user preference', () => {
    const theme = read('ui/theme/Theme.qml');
    const settings = read('ui/components/settings/SettingsPopup.qml');
    const plate = read('ui/components/controls/PopupSurface.qml');
    const halo = read('ui/components/visual/NeonHalo.qml');
    assert.match(theme, /category: "NeonSurfaces"/);
    assert.match(theme, /readonly property bool neonEnabled: neonPreferences\.enabled && !powerSave/);
    assert.match(theme, /function setNeonEnabled\(enabled\)/);
    assert.match(settings, /checked: Theme\.neonPreferences\.enabled/);
    assert.match(settings, /onToggled: Theme\.setNeonEnabled\(checked\)/);
    assert.match(plate, /NeonHalo \{/);
    assert.match(halo, /Theme\.reduceMotion \? 0/);
    assert.match(halo, /blocking: false/);
    assert.match(halo, /steadyActivity:/);
});

test('Neon preferences are translated in all supported QML catalogs', () => {
    for (const locale of ['ar_SA', 'de_DE', 'en_US', 'es_ES', 'fa_IR', 'fr_FR',
                          'hi_IN', 'ja_JP', 'ko_KR', 'pt_BR', 'ru_RU', 'zh_CN']) {
        const xml = read('ui/i18n/qml_' + locale + '.ts');
        for (const key of ['neon_effects', 'neon_effects_description']) {
            const match = xml.match(new RegExp('<message id="genycaster\\.settings\\.' + key +
                  '">([\\s\\S]*?)</message>'));
            assert.ok(match, locale + ': missing ' + key);
            assert.match(match[1], /<translation>[^<]+<\/translation>/);
        }
    }
});

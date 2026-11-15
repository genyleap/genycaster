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
    const material = read('ui/components/visual/NeonSurface.qml');
    const statePlate = read('ui/components/visual/NeonStatePlate.qml');
    assert.match(theme, /category: "NeonSurfaces"/);
    assert.match(theme, /readonly property bool neonEnabled: neonPreferences\.enabled/);
    assert.match(theme, /readonly property bool neonMotionEnabled: neonEnabled && !reduceMotion && !powerSave/);
    assert.match(theme, /function setNeonEnabled\(enabled\)/);
    assert.match(settings, /checked: Theme\.neonPreferences\.enabled/);
    assert.match(settings, /onToggled: Theme\.setNeonEnabled\(checked\)/);
    assert.match(plate, /NeonSurface \{/);
    assert.match(material, /sourceComponent: NeonHalo \{/);
    assert.match(statePlate, /NeonSurface \{/);
    assert.match(halo, /running: root\.activityActive && root\.visible && Theme\.neonMotionEnabled/);
    assert.match(halo, /readonly property bool tracking: Theme\.neonEnabled && \(active \|\| hovered\)/);
    assert.match(halo, /blocking: false/);
    assert.match(halo, /steadyActivity:/);
});


test('Neon is present on visible common controls, selected tabs and dashboard cards', () => {
    const theme = read('ui/theme/Theme.qml');
    const action = read('ui/components/controls/ActionButton.qml');
    const tab = read('ui/components/controls/GenyTabButton.qml');
    const popup = read('ui/components/controls/PopupSurface.qml');
    const choice = read('ui/components/setup/SetupActionCard.qml');
    const stats = read('ui/components/home/StatsKpi.qml');
    const pulse = read('ui/components/feed/FeedPulseCard.qml');
    const compose = read('ui/components/navigation/FloatingComposeButton.qml');
    const compactMobile = read('ui/components/navigation/BottomNavButton.qml');
    const mobileDock = read('ui/components/navigation/BottomNavigation.qml');
    const desktopNav = read('ui/components/navigation/NavItem.qml');
    const material = read('ui/components/visual/NeonSurface.qml');
    const plate = read('ui/components/visual/NeonStatePlate.qml');
    const field = read('ui/components/controls/BidiTextField.qml');
    const combo = read('ui/components/controls/GenyComboBox.qml');
    const switchControl = read('ui/components/controls/GenySwitch.qml');
    for (const component of [action, popup, choice, stats, pulse,
                             compose, compactMobile, mobileDock, field, switchControl]) {
        assert.match(component, /NeonSurface \{/);
    }
    for (const component of [tab, desktopNav, combo]) {
        assert.match(component, /NeonStatePlate \{/);
    }
    assert.match(material, /property bool ambientNeon: false/);
    assert.match(material, /property bool interactive: false/);
    assert.match(material, /property bool neonBusy: false/);
    assert.match(material, /sourceComponent: NeonHalo \{/);
    assert.match(material, /cornerPower: material\.exponent/);
    assert.match(material, /borderInset: material\.borderWidth \/ 2/);
    assert.match(material, /Theme\.neonEnabled/);
    // Turning the effect off never disables paint/focus state animations.
    assert.match(material, /enabled: material\.animatePaint && !Theme\.reduceMotion/);
    assert.match(theme, /readonly property int materialSurfaceDuration: reduceMotion \? 0 : 105/);
    assert.match(plate, /opacity: plate\.selected \? 1 : 0/);
    assert.match(plate, /plate\.hovered && !plate\.pressed/);
    assert.match(plate, /fillColor: "transparent"/);
    assert.match(action, /ambientNeon: control\.prominent \|\| control\.selected/);
    assert.match(action, /externalHovered: control\.hovered && !control\.mobileViewport/);
    assert.match(choice, /ambientNeon: root\.accountShowcase/);
    assert.match(stats, /ambientNeon: true/);
    assert.match(pulse, /neonBusy: root\.loading/);
    assert.match(compose, /ambientNeon: true/);
    assert.match(mobileDock, /ambientNeon: destination\.active/);
    assert.match(desktopNav, /hovered: control\.hovered/);
    assert.match(field, /ambientNeon: root\.activeFocus/);
    assert.match(combo, /ambientNeon: control\.popup\.visible/);
    assert.match(switchControl, /ambientNeon: control\.checked/);
    const halo = read('ui/components/visual/NeonHalo.qml');
    assert.match(halo, /function nearestBorderPoint\(/);
    assert.match(halo, /for \(let step = 0; step < 18; \+\+step\)/);
    assert.match(halo, /property point sampledPointer/);
    assert.match(halo, /Qt\.callLater\(samplePointer\)/);
    const shader = read('ui/shaders/neon.frag');
    assert.match(shader, /small inwards/);
    assert.match(shader, /mix\(localized, 0\.57, steadyActivity\)/);
});

test('Neon activity never shadows public busy state on data components', () => {
    const material = read('ui/components/visual/NeonSurface.qml');
    const pulse = read('ui/components/feed/FeedPulseCard.qml');
    const popup = read('ui/components/controls/PopupSurface.qml');
    const button = read('ui/components/controls/ActionButton.qml');
    const setup = read('ui/components/setup/SetupActionCard.qml');
    assert.match(material, /property bool neonBusy: false/);
    assert.doesNotMatch(material, /property bool busy:/);
    assert.match(material, /activity: material\.neonBusy/);
    assert.match(pulse, /neonBusy: root\.loading/);
    assert.match(pulse, /property bool busy: false/);
    assert.match(pulse, /readonly property bool loading: root\.busy &&/);
    assert.match(button, /neonBusy: control\.busy/);
    assert.match(setup, /neonBusy: root\.busy/);
    assert.doesNotMatch(popup, /property bool neonBusy:/);
    assert.doesNotMatch(popup, /busy: neonBusy/);
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

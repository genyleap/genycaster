import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(path, 'utf8');

// Regression based on the actual 2026-11-15 desktop walkthrough:
// dashboard 0–11s, Explorer magazine 12–24s, Appearance drawer 26–36s.
test('Dashboard composer and four quick-access cards have Kit material', () => {
    const dashboard = read('ui/components/home/HomeDashboard.qml');
    const composer = read('ui/components/home/HomeComposer.qml');
    assert.match(dashboard, /delegate: NeonCard \{[\s\S]*?id: shortcut/);
    assert.match(dashboard, /externalHovered: actionMouse\.containsMouse/);
    assert.match(dashboard, /externalPointerX: actionMouse\.mouseX/);
    assert.match(dashboard, /component HomeActivityCard: NeonCard \{/);
    assert.match(dashboard, /id: desktopComposerSurface[\s\S]*?ambientNeon: true/);
    assert.match(dashboard, /NeonSurface \{[\s\S]*?id: mobileComposerSurface/);
    assert.match(composer, /^NeonSurface \{/m);
    assert.match(composer, /externalHovered: composerHover\.hovered/);
});

test('Right-hand Calendar, Assets, Feed and Market share a card material', () => {
    const rail = read('ui/components/navigation/ContextRail.qml');
    const pulse = read('ui/components/feed/FeedPulseCard.qml');
    assert.match(rail, /component WidgetCard: NeonCard \{/);
    assert.match(rail, /default property alias content: body\.data/);
    assert.match(rail, /id: assetFront[\s\S]*?ambientNeon: true/);
    assert.match(pulse, /^NeonSurface \{/m);
    assert.match(pulse, /ambientNeon: true/);
    for(const file of ['MarketKpi','MarketTrend','StatsChart','StatsBreakdown']) {
        const source = read('ui/components/home/' + file + '.qml');
        assert.match(source, /^NeonCard \{/m, file);
        assert.match(source, /ambientNeon: true/, file);
    }
});

test('Magazine Cast cards react to the local pointer without changing timeline posts', () => {
    const cast = read('ui/components/feed/CastItem.qml');
    assert.match(cast, /id: messageSurface[\s\S]*?id: magazineCardPointer/);
    assert.match(cast, /interactive: root\.magazineCard && !root\.scrollMoving/);
    assert.match(cast, /ambientNeon: root\.magazineCard/);
    assert.match(cast, /externalHovered: magazineCardPointer\.hovered/);
    // Timeline stays a non-painting Item: a retained Neon surface is only
    // visible in Magazine, avoiding per-row GPU shader allocations.
    const start = cast.indexOf('id: magazineCardSurface');
    const end = cast.indexOf('ColumnLayout {\n                id: messageContent', start);
    assert.ok(start >= 0 && end > start);
    const chrome = cast.slice(start, end);
    assert.match(cast, /Item\s*\{\s*id: messageSurface/);
    assert.match(chrome, /visible: root\.magazineCard/);
    assert.match(chrome, /fillColor: root\.magazineCard \? Theme\.surfaceRaised : "transparent"/);
    assert.match(chrome, /borderWidth: root\.magazineCard \? Theme\.strokeHairline : 0/);
});

test('Appearance drawer includes the visible setting and color picker panels', () => {
    const settings = read('ui/components/settings/SettingsPopup.qml');
    assert.match(settings, /component SettingsProfileCard: NeonSurface \{/);
    for (const row of ['energyRow','avatarMotionRow','emojiMotionRow','reduceMotionRow',
                       'neonEffectsRow']) {
        const pattern = new RegExp('NeonSurface \\{[\\s\\S]{0,450}?'
                                + 'implicitHeight: '+row+'\\.implicitHeight');
        assert.match(settings, pattern, row);
    }
    // Custom accent is now a compact sibling of the palette grid; it still
    // uses the shared material without the old three stacked preview surfaces.
    // The accent panel is a quiet retained surface: keep it in the palette
    // grid without an always-on shader or a second full-size preview.
    const accentIndex = settings.indexOf('id: customAccentCard');
    const accentStart = settings.lastIndexOf('NeonSurface {', accentIndex);
    const accentEnd = settings.indexOf('ColumnLayout {', accentIndex);
    assert.ok(accentStart >= 0 && accentEnd > accentIndex);
    const accentChrome = settings.slice(accentStart, accentEnd);
    assert.match(accentChrome, /id: customAccentCard/);
    assert.match(accentChrome, /interactive: false/);
    assert.match(accentChrome, /ambientNeon: false/);
    assert.match(accentChrome, /implicitHeight: customColorContent\.implicitHeight \+ Theme\.space3 \* 2/);
    assert.match(accentChrome, /Layout\.preferredWidth: colorThemeLayout\.columns === 2/);
    assert.match(accentChrome, /borderColor: appearanceController\.colorThemeId === "custom"/);
    for (const name of ['DisplayScaleControl','PanelWidthControl']) {
        const source = read('ui/components/settings/'+name+'.qml');
        assert.match(source, /^NeonCard \{/m, name);
    }
});

test('Bounded repeaters avoid permanent shaders for idle action buttons', () => {
    const action = read('ui/components/controls/ActionButton.qml');
    const material = read('ui/components/visual/NeonSurface.qml');
    const bookmarks = read('ui/components/profile/BookmarksSurface.qml');
    const notifications = read('ui/components/feed/NotificationSurface.qml');
    assert.match(action, /deferredNeon: control\.quiet && !control\.selected/);
    assert.match(material, /property bool deferredNeon: false/);
    assert.match(material, /property real hoverLifetime: pointerActive \? 1 : 0/);
    assert.match(material, /material\.hoverLifetime > 0\.001/);
    assert.match(bookmarks, /delegate: NeonSurface \{[\s\S]*?deferredNeon: true/);
    assert.match(notifications, /externalHovered: rowHover\.hovered/);
    assert.match(notifications, /deferredNeon: true/);
});

test('Shared material also reaches mini apps, profile and network status', () => {
    const apps = read('ui/components/apps/AppsSurface.qml');
    const profile = read('ui/components/profile/ProfileTrustCard.qml');
    const status = read('ui/components/shell/SystemStatusCard.qml');
    assert.match(apps, /id: miniAppDesktopStoreRow|objectName: "miniAppDesktopStoreRow"/);
    assert.match(apps, /objectName: "miniAppDesktopStoreRow"[\s\S]{0,500}NeonSurface \{/);
    assert.match(profile, /^NeonSurface \{/m);
    assert.match(status, /^NeonSurface \{/m);
    assert.match(status, /neonBusy: root\.busy && !root\.runtimeReady/);
});

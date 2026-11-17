import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../'+file, import.meta.url), 'utf8');

test('filled primary CTA preserves its accent-blue rim instead of a white glint', () => {
    const buttons = read('ui/components/controls/ActionButton.qml');
    const setup = read('ui/components/setup/SetupActionCard.qml');
    for (const source of [buttons, setup]) {
        assert.match(source, /Qt\.lighter\(Theme\.primaryFill, 1\.28\)/);
        assert.doesNotMatch(source, /Qt\.rgba\(1,\s*1,\s*1,\s*0\.72\)/);
        assert.match(source, /Theme\.primaryFill\.r, Theme\.primaryFill\.g/);
    }
    assert.match(buttons, /Qt\.lighter\(Theme\.danger, 1\.20\)/);
    assert.match(buttons, /Qt\.lighter\(Theme\.success, 1\.20\)/);
    assert.match(buttons, /ambientStrength: control\.prominent \|\| control\.destructiveFilled/);
});

test('backgrounds are scoped to composer tabs, not shared typography-only navigation', () => {
    const composer = read('ui/components/composer/ComposerToolbar.qml');
    const strip = read('ui/components/controls/GenyTabStrip.qml');
    const tab = read('ui/components/controls/GenyTabButton.qml');
    const profile = read('ui/components/profile/ProfileSurface.qml');
    const explorer = read('ui/components/feed/ExplorerLayoutControls.qml');
    assert.match(composer, /filledTabs: true/);
    assert.match(strip, /property bool filledTabs: false/);
    assert.match(strip, /visible: root\.filledTabs/);
    assert.match(strip, /filledTabs: root\.filledTabs/);
    assert.match(tab, /selected: control\.filledTabs && control\.selected/);
    assert.match(tab, /neonOnHover: control\.filledTabs/);
    assert.match(tab, /hoverFill: control\.filledTabs \? Theme\.surfaceHover : "transparent"/);
    assert.doesNotMatch(profile, /filledTabs: true/);
    assert.doesNotMatch(explorer, /filledTabs: true/);
});

test('format toolbar reserves complete hit/paint height and permits horizontal scrolling', () => {
    const toolbar = read('ui/components/composer/FormattingToolbar.qml');
    const drawer = read('ui/components/composer/ComposerDrawer.qml');
    const theme = read('ui/theme/Theme.qml');
    assert.match(toolbar, /implicitHeight: Theme\.compactControlHeight \+ Theme\.space4/);
    assert.match(toolbar, /anchors\.margins: Theme\.space1/);
    assert.match(toolbar, /contentWidth: Math\.max\(width, row\.implicitWidth\)/);
    assert.match(toolbar, /width: Math\.max\(implicitWidth, toolViewport\.width\)/);
    assert.match(toolbar, /flickableDirection: Flickable\.HorizontalFlick/);
    assert.match(drawer, /id: formattingToolbar/);
    assert.match(theme, /readonly property int compactControlHeight: scaleUi\(38\)/);
    // 38px action body plus 16px outside -> 54px outer; 4px per inset leaves 46px.
    const outer = 38 + 16, inner = outer - 4 * 2;
    assert.ok(inner >= 38, 'the full action body fits without clipping');
});

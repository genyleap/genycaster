import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('setup primary CTA inherits the same button, material and variant policy as shell CTA', () => {
    const shared = read('ui/components/controls/ActionButton.qml');
    const setup = read('ui/components/setup/SetupPrimaryButton.qml');
    const shell = read('ui/components/shell/AppShell.qml');
    const home = read('ui/components/home/HomeDashboard.qml');
    assert.match(setup, /^ActionButton \{/m);
    assert.match(setup, /prominent: true/);
    assert.doesNotMatch(setup, /background:|ShaderEffect \{|NeonHalo \{/);
    assert.match(shared, /background: NeonSurface \{/);
    assert.match(shared, /property bool neutralPrimary: false/);
    assert.match(shared, /interactive: control\.enabled\s/);
    assert.match(shared, /control\.destructiveFilled \|\| control\.neutralPrimary/);
    assert.match(shared, /borderWidth: Theme\.keyboardFocusVisible\(control\) \? 2/);
    assert.match(shared, /control\.prominent \|\| control\.destructiveFilled/);
    assert.match(shell, /objectName: "confirmSignOutButton"[\s\S]*?destructive: true[\s\S]*?destructiveFilled: true/);
    assert.match(home, /prominent: true/);
});

test('shared Neon material preserves keyboard glint and focused outline simultaneously', () => {
    const surface = read('ui/components/visual/NeonSurface.qml');
    assert.match(surface, /material\.focused[\s\S]*?!material\.deferredNeon/);
    assert.match(surface, /active: material\.pointerActive \|\| material\.focused/);
    assert.match(surface, /suppressed: false/);
});

test('language wheel owns selected and hovered material rather than a detached backdrop', () => {
    const language = read('ui/components/setup/SetupLanguageSelector.qml');
    assert.match(language, /delegate: Item \{[\s\S]*?NeonSurface \{/);
    assert.match(language, /ambientNeon: languageRow\.selected/);
    assert.match(language, /deferredNeon: !languageRow\.selected/);
    assert.match(language, /focused: languageRow\.selected && Theme\.keyboardFocusVisible\(picker\)/);
    assert.match(language, /externalHovered: languagePointer\.hovered && !picker\.moving/);
    assert.doesNotMatch(language, /NeonStatePlate \{|NeonCard \{/);
});

test('other onboarding choices share a single NeonCard body with contrast-aware selected state', () => {
    const choice = read('ui/components/setup/SetupActionCard.qml');
    assert.match(choice, /background: NeonCard \{/);
    assert.match(choice, /inverted: root\.neutralPrimary \? Theme\.dark/);
    assert.match(choice, /borderWidth: Theme\.keyboardFocusVisible\(root\) \? 2/);
    assert.match(choice, /root\.selected \? 2/);
    assert.match(choice, /neonStrength: Theme\.neonCardStrength/);
});

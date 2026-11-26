import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = path => readFileSync(path, 'utf8');

// Source-backed coverage of 2026-11-15 12:20:36 screen recording (93.4s).
// We intentionally distinguish interactive card/frame boundaries from tiny
// decorative Superellipse primitives, which should not allocate shaders.
test('The semantic NeonCard is registered and owns only the shared material', () => {
    const card = read('ui/components/visual/NeonCard.qml');
    const cmake = read('ui/CMakeLists.txt');
    const paths = read('ui/components/source_paths.json');
    assert.match(card, /^NeonSurface \{/m);
    assert.match(card, /interactive: true/);
    assert.match(card, /ambientNeon: true/);
    assert.match(card, /neonStrength: Theme\.neonCardStrength/);
    assert.doesNotMatch(card, /NeonHalo \{|ShaderEffect \{/);
    assert.match(cmake, /components\/visual\/NeonCard\.qml/);
    assert.equal(JSON.parse(paths)['NeonCard.qml'], 'visual/NeonCard.qml');
});

test('Welcome and language wheel have actual pointer-tracking surfaces', () => {
    const primary = read('ui/components/setup/SetupPrimaryButton.qml');
    const language = read('ui/components/setup/SetupLanguageSelector.qml');
    // Setup uses the same ActionButton material as the app's primary CTA.
    assert.match(primary, /^ActionButton \{/m);
    assert.match(primary, /prominent: true/);
    assert.doesNotMatch(primary, /background: NeonSurface \{/);
    assert.match(language, /id: languagePointer/);
    assert.match(language, /NeonSurface \{/);
    assert.match(language, /ambientNeon: languageRow\.selected/);
    assert.match(language, /externalHovered: languagePointer\.hovered && !picker\.moving/);
    assert.match(language, /onMovementEnded: root\.choose\(currentIndex\)/);
});

test('Every SetupActionCard supports hovering including recovery, provider and legal rows', () => {
    const card = read('ui/components/setup/SetupActionCard.qml');
    assert.match(card, /background: NeonCard \{/);
    assert.match(card, /interactive: root\.enabled && !root\.unavailable/);
    assert.match(card, /root\.setupEmphasis \|\| root\.selected/);
    assert.match(card, /externalHovered: root\.hovered/);
    assert.match(card, /externalPointerX: cardPointer\.hovered/);
    assert.match(card, /deferredNeon: !root\.accountShowcase/);
    assert.match(card, /focused: Theme\.keyboardFocusVisible\(root\)/);
    // Existing actions preserve their enabled/checkable state and callbacks.
    assert.match(card, /property bool unavailable: false/);
    assert.match(card, /Accessible\.checked: root\.selected/);
});

test('Legal consent, appearance options and platform accent are semantic materials', () => {
    const source = read('ui/components/setup/InitialSetup.qml');
    assert.match(source, /id: legalSurface[\s\S]{0,220}ambientNeon: true/);
    assert.match(source, /background: NeonSurface \{[\s\S]{0,260}interactive: consentCheck\.enabled/);
    assert.match(source, /background: NeonCard \{[\s\S]{0,260}interactive: appearanceChoice\.enabled/);
    assert.match(source, /background: NeonCard \{[\s\S]{0,260}interactive: setupPlatformColor\.enabled/);
    assert.match(source, /id: completionBadge[\s\S]{0,130}ambientNeon: true/);
});

test('Recovery uses focused, transient fields without changing phrase security', () => {
    const setup = read('ui/components/setup/InitialSetup.qml');
    const check = read('ui/components/controls/GenyCheckRow.qml');
    assert.match(setup, /id: recoveryWordPointer/);
    assert.match(setup, /background: NeonSurface \{[\s\S]{0,300}deferredNeon: true/);
    assert.match(setup, /ambientNeon: wordField\.editingFocus/);
    assert.match(setup, /externalHovered: wordField\.hovered/);
    assert.match(setup, /inputMethodHints: Qt\.ImhSensitiveData/);
    assert.match(setup, /nativeRecoveryGrid\.clearAll\(\)/);
    assert.match(setup, /nativeCustodyWallet\.restoreForRegistration\(words\)/);
    assert.match(setup, /nativeRecoveryWordsDisplay/);
    assert.match(check, /background: NeonSurface \{/);
    assert.match(check, /deferredNeon: true/);
    assert.match(check, /checkable: true/);
});

test('Unsegmented tabs and Settings navigation provide transient glints', () => {
    const tab = read('ui/components/controls/GenyTabButton.qml');
    const state = read('ui/components/visual/NeonStatePlate.qml');
    const settings = read('ui/components/settings/SettingsPopup.qml');
    const nav = read('ui/components/navigation/NavItem.qml');
    assert.match(tab, /hovered: control\.hovered/);
    assert.match(tab, /pressed: control\.down/);
    assert.match(state, /deferredNeon: true/);
    assert.match(state, /externalHovered: plate\.hovered && plate\.neonOnHover/);
    assert.match(state, /ambientNeon: false/);
    assert.match(settings, /background: NeonStatePlate \{[\s\S]{0,200}selected: navTab\.selected/);
    assert.match(settings, /hovered: navTab\.hovered/);
    assert.match(nav, /neonOnHover: false/);
});

test('Composer keeps panel materials outside the editable text', () => {
    const composer = read('ui/components/composer/ComposerDrawer.qml');
    const insert = read('ui/components/composer/ComposerInsertPanel.qml');
    const upload = read('ui/components/composer/MediaUploadPanel.qml');
    const tools = read('ui/components/composer/FormattingToolbar.qml');
    assert.match(insert, /^NeonCard \{/m);
    assert.match(upload, /^NeonCard \{/m);
    assert.match(tools, /^NeonSurface \{/m);
    assert.match(composer, /id: replyContextCard[\s\S]{0,150}ambientNeon: true/);
    assert.match(composer, /interactive: !root\.compact/);
    assert.match(composer, /ambientNeon: threadEditor\.editingFocus/);
    assert.match(composer, /id: channelSearch/);
    assert.match(composer, /background: PopupSurface \{/);
    assert.match(composer, /background: null/); // editor maintains native focus and IME.
});

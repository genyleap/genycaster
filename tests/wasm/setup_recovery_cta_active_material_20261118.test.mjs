import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = p => readFileSync(new URL('../../' + p, import.meta.url), 'utf8');

test('recovery action is distinctly active while provider choices remain secondary', () => {
    const page = read('ui/components/setup/InitialSetup.qml');
    const signIn = page.slice(page.indexOf('id: signInStage'), page.indexOf('id: verifyingStage'));
    assert.match(signIn, /SetupActionCard \{[\s\S]*?setupEmphasis: true[\s\S]*?title: qsTrId\("genycaster.setup.design_recovery_title"\)/);
    assert.match(signIn, /enabled: !root.authenticationInProgress && !root.recoverySubmitPending/);
    assert.match(signIn, /SetupActionCard \{\s*objectName: "setupExistingHyperSnap"/);
    assert.match(signIn, /SetupActionCard \{\s*objectName: "setupExistingFarcaster"/);
    assert.doesNotMatch(signIn, /objectName: "setupBrowserRecoveryProtocolChoice"/);
});

test('enabled recovery action uses luminous brand fill, contrasting glyph and chevron', () => {
    const card = read('ui/components/setup/SetupActionCard.qml');
    assert.match(card, /root.setupEmphasis \? Theme.accentInk/);
    assert.match(card, /root.setupEmphasis\s*\?\s*\(root.down \? Theme.primaryPressed[\s\S]*?Theme.primaryFillHover : Theme.primaryFill\)/);
    assert.match(card, /root.primary \|\| root.setupEmphasis\s*\?\s*Qt.lighter\(Theme.primaryFill, 1.28\)/);
    assert.match(card, /root.setupEmphasis \? root.foreground/);
    assert.match(card, /root.primary \|\| root.setupEmphasis \? !Theme.dark : false/);
    assert.match(card, /root.setupEmphasis && !root.visuallyDisabled[\s\S]*?Theme.primaryFill\.r, Theme.primaryFill\.g/);
    assert.match(card, /elevation: \(root.primary \|\| root.setupEmphasis\) && !root.visuallyDisabled/);
});

test('actual disabled recovery action remains muted rather than suggesting it is clickable', () => {
    const card = read('ui/components/setup/SetupActionCard.qml');
    assert.match(card, /readonly property bool visuallyDisabled: !enabled && !busy/);
    assert.match(card, /root.unavailable \|\| root.visuallyDisabled\s*\? Theme.inkMuted/);
    assert.match(card, /neonColor: root.visuallyDisabled \|\| root.unavailable \? "transparent"/);
    assert.match(card, /fillColor: root.unavailable \? Theme.setupActionUnavailableSurface[\s\S]*?: root.visuallyDisabled \? Theme.surfaceMuted/);
    assert.match(card, /ambientNeon: root.enabled/);
    assert.match(card, /interactive: root.enabled && !root.unavailable/);
});

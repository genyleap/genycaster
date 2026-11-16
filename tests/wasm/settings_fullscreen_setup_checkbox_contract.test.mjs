import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Settings covers the entire application on desktop and mobile without capturing the workspace', () => {
    const settings = read('ui/components/settings/SettingsPopup.qml');
    const shell = read('ui/components/shell/AppShell.qml');
    const drawer = read('ui/components/controls/GenyDrawer.qml');
    assert.match(settings, /bottomSheet: mobilePresentation/);
    assert.match(settings, /fullScreen: true/);
    assert.match(settings, /blurSourceItem: null/);
    assert.match(settings, /dimBackground: false/);
    assert.match(settings, /efficientBackdropBlur: false/);
    assert.match(settings, /width: parent \? parent\.width : 880/);
    assert.match(settings, /height: parent \? parent\.height : 820/);
    assert.match(settings, /readonly property real desktopContentInset:[\s\S]*?Math\.max\(root\.contentPadding, \(root\.width - Theme\.scaleUi\(1200\)\) \/ 2\)/);
    assert.match(settings, /leftPadding: desktopContentInset/);
    assert.match(settings, /rightPadding: desktopContentInset/);
    assert.match(settings, /onCloseRequested: root\.close\(\)/);
    assert.match(settings, /readonly property real navigationInset/);
    const loadedSettings = shell.split('LocaleSettingsPopup {')[1].split('onCustomAccentStudioRequested')[0];
    assert.doesNotMatch(loadedSettings, /blurSourceItem:\s*shellContent/);
    assert.match(drawer, /dim: control\.blurSourceItem !== null \|\| control\.dimBackground/);
    assert.match(drawer, /curveDepth: control\.fullScreen \? 0/);
    assert.match(drawer, /neonAmbient: !control\.fullScreen/);
});

test('All recovery checkboxes share a stable Light/Dark background and explicit checked appearance', () => {
    const control = read('ui/components/controls/GenyCheckRow.qml');
    const setup = read('ui/components/setup/InitialSetup.qml');
    assert.match(control, /Button \{/);
    assert.match(control, /checkable: true/);
    assert.match(control, /Accessible\.role: Accessible\.CheckBox/);
    assert.match(control, /Accessible\.checked: checked/);
    assert.doesNotMatch(control, /onClicked:\s*.*checked\s*=/);
    assert.match(control, /fillColor: control\.down \? Theme\.surfacePressed[\s\S]*?: control\.checked \? Theme\.surfaceSelected[\s\S]*?: control\.hovered \? Theme\.surfaceHover : Theme\.surfaceMuted/);
    assert.match(control, /borderColor: Theme\.keyboardFocusVisible\(control\) \? Theme\.focus[\s\S]*?: control\.checked \? Theme\.accent : Theme\.borderMuted/);
    assert.match(control, /fillColor: control\.checked \? Theme\.accent : Theme\.surface/);
    assert.match(control, /color: Theme\.contrastInk\(Theme\.accent\)/);
    assert.match(control, /Layout\.fillWidth: true/);
    assert.match(control, /wrapMode: Text\.WordWrap/);
    for (const id of [
        'walletBackupAcknowledgment', 'neynarTransferConsent',
        'nativeRegistrationConsent', 'revealWords', 'deviceSignerConsent'
    ]) {
        assert.match(setup, new RegExp('GenyCheckRow \\{\\s*id: '+id+'\\b'));
    }
    // Defaults belong to the checkable Button. A constant QML checked:false
    // binding can reset the state or break when Qt processes user toggles.
    const recovery = setup.slice(setup.indexOf('id: revealWords'), setup.indexOf('SetupPrimaryButton {', setup.indexOf('id: revealWords')));
    assert.doesNotMatch(recovery, /checked:\s*false/);
    assert.match(recovery, /onVisibleChanged: if \(!visible\) checked = false/);
    assert.match(setup, /deviceSignerConsent\.checked/);
    assert.match(setup, /nativeRecoveryGrid\.clearAll\(\)/);
});

test('Legal consent uses the same control palette while preserving its persisted contract', () => {
    const setup = read('ui/components/setup/InitialSetup.qml');
    const legal = setup.slice(setup.indexOf('id: consentCheck'), setup.indexOf('Component {\n        id: accountStage'));
    assert.match(legal, /onClicked: root\.consentChecked = !root\.consentChecked/);
    assert.match(legal, /Accessible\.checked: root\.consentChecked/);
    assert.match(legal, /fillColor: root\.consentChecked \? Theme\.accent : Theme\.surface/);
    assert.match(legal, /borderColor: root\.consentChecked \? Theme\.accent : Theme\.borderStrong/);
    assert.match(legal, /root\.consentChecked \? Theme\.surfaceSelected/);
    assert.match(legal, /fillColor: consentCheck\.down \? Theme\.surfacePressed/);
});

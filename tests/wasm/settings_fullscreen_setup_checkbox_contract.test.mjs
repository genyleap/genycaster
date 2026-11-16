import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Settings is a centered content-fitted modal on desktop and a bottom sheet on mobile', () => {
    const settings = read('ui/components/settings/SettingsPopup.qml');
    const shell = read('ui/components/shell/AppShell.qml');
    const adaptive = read('ui/components/controls/GenyAdaptiveDialog.qml');
    assert.match(settings, /GenyAdaptiveDialog \{/);
    assert.match(settings, /mobileFullScreen: true/);
    assert.match(settings, /showHandle: false/);
    assert.match(settings, /blurSourceItem: null/);
    assert.match(settings, /efficientBackdropBlur: true/);
    // Inactive settings sections must not allocate or update theme-bound QSG items.
    for (const id of ['appearancePage','localePage','marketPage','readSettingsPage',
                      'updatesPage','recoveryPage','nodePage'])
        assert.match(settings, new RegExp('id: ' + id + '\\b'));
    assert.match(settings, /active: \(!root\.mobilePresentation \|\| !root\.mobileIndexOpen\) && root\.currentSection === 0/);
    assert.match(settings, /page && page\.contentItem/);
    assert.match(settings, /readonly property int contentColumns:[\s\S]*?width - navigationColumnWidth - Theme\.space6/);
    assert.match(settings, /dimBackground: !mobilePresentation/);
    assert.match(settings, /popupZ: 0/);
    assert.match(settings, /width: parent \? \(mobilePresentation \? parent\.width/);
    assert.match(settings, /Math\.min\(Theme\.scaleUi\(1380\), Math\.max\(0, parent\.width - Theme\.space4 \* 2\)\)/);
    assert.match(settings, /height: parent \? \(mobilePresentation \? parent\.height/);
    assert.match(settings, /Math\.min\(Theme\.scaleUi\(860\), Math\.max\(0, parent\.height - Theme\.space4 \* 2\)\)/);
    assert.doesNotMatch(settings, /desktopContentInset|leftPadding: desktopContentInset|rightPadding: desktopContentInset/);
    assert.match(settings, /contentPadding: mobilePresentation \? Theme\.mobileGutter : Theme\.space4/);
    assert.match(settings, /onCloseRequested: root\.close\(\)/);
    assert.match(settings, /readonly property real navigationInset/);
    assert.match(settings, /mobileBottomPaddingExtra: Math\.max\(Theme\.space3, root\.navigationInset\)/);
    assert.match(settings, /onDidOpen: \{/);
    assert.match(settings, /onDidClose: root\.mobileIndexOpen = true/);
    assert.match(adaptive, /GenyModal \{[\s\S]*?id: desktopDialog/);
    assert.match(adaptive, /GenyDrawer \{[\s\S]*?id: mobileDrawer/);
    assert.match(adaptive, /bottomSheet: true/);
    assert.match(adaptive, /fullScreen: control\.mobileFullScreen/);
    assert.match(adaptive, /blurSourceItem: control\.blurSourceItem/);
    assert.match(adaptive, /efficientNativeBlur: control\.efficientBackdropBlur/);
    assert.match(adaptive, /efficientBackdropBlur: control\.efficientBackdropBlur/);
    assert.match(adaptive, /contentItem: Item \{\s*id: desktopContentHost/);
    assert.match(adaptive, /contentItem: Item \{\s*id: mobileContentHost/);
    assert.match(adaptive, /control\.attachContent\(desktopContentHost\)/);
    assert.match(adaptive, /control\.attachContent\(mobileContentHost\)/);
    const loadedSettings = shell.split('LocaleSettingsPopup {')[1].split('onCustomAccentStudioRequested')[0];
    assert.match(loadedSettings, /blurSourceItem: mobilePresentation \? null : shellContent/);
    assert.doesNotMatch(loadedSettings, /blurSourceItem:\s*shellContent/);
    const frost = read('ui/components/visual/DrawerBackdrop.qml');
    assert.match(frost, /interval: Math\.max\(360, Theme\.materialSurfaceDuration \+ 220\)/);
    assert.match(frost, /Math\.min\(0\.48, 1440 \/ Math\.max\(1, root\.width, root\.height\)\)/);
    assert.match(frost, /sourceItem: root\.nativeGaussian \? horizontalPass : null/);
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

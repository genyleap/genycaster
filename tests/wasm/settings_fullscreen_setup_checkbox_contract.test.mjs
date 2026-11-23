import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('Settings fills the desktop viewport and the mobile inspector remains full-height', () => {
    const settings = read('ui/components/settings/SettingsPopup.qml');
    const shell = read('ui/components/shell/AppShell.qml');
    const adaptive = read('ui/components/controls/GenyAdaptiveDialog.qml');
    assert.match(settings, /GenyAdaptiveDialog \{/);
    assert.match(settings, /mobileFullScreen: true/);
    assert.match(settings, /desktopFullScreen: true/);
    assert.match(settings, /showHandle: false/);
    assert.match(settings, /blurSourceItem: null/);
    assert.doesNotMatch(settings, /efficientBackdropBlur: true/);
    // Inactive settings sections must not allocate or update theme-bound QSG items.
    for (const id of ['appearancePage','localePage','marketPage','readSettingsPage',
                      'updatesPage','recoveryPage','nodePage'])
        assert.match(settings, new RegExp('id: ' + id + '\\b'));
    assert.match(settings, /active: \(!root\.mobilePresentation \|\| !root\.mobileIndexOpen\) && root\.currentSection === 0/);
    assert.match(settings, /page && page\.contentItem/);
    assert.match(settings, /readonly property int contentColumns: !mobilePresentation && settingsPages\.width >=/);
    const workspace = settings.slice(settings.indexOf('objectName: "settingsWorkspace"'), settings.indexOf('id: settingsPages'));
    assert.match(workspace, /Layout\.fillWidth: true\s*Layout\.fillHeight: true/);
    assert.doesNotMatch(workspace, /Layout\.maximumWidth|Layout\.alignment: Qt\.AlignHCenter/);
    assert.match(settings, /component SettingsAction: ActionButton \{\s*Layout\.fillWidth: root\.mobilePresentation/);
    for (const page of ['appearanceScroll', 'readSettingsScroll', 'updatesScroll', 'recoverySettingsScroll'])
        assert.match(settings, new RegExp('PreferenceGrid \\{\\s*width: ' + page + '\\.availableWidth'));
    assert.match(settings, /dimBackground: false/);
    assert.match(settings, /popupZ: 0/);
    assert.match(settings, /width: parent \? parent\.width : 1180/);
    assert.match(settings, /height: parent \? parent\.height : 780/);
    assert.doesNotMatch(settings, /Math\.min\(Theme\.scaleUi\(1380\)/);
    assert.doesNotMatch(settings, /Math\.min\(Theme\.scaleUi\(860\)/);
    assert.doesNotMatch(settings, /desktopContentInset|leftPadding: desktopContentInset|rightPadding: desktopContentInset/);
    assert.match(settings, /contentPadding: mobilePresentation \? Theme\.mobileGutter : Theme\.space4/);
    assert.match(settings, /onCloseRequested: root\.close\(\)/);
    assert.match(settings, /readonly property real navigationInset/);
    assert.match(settings, /mobileBottomPaddingExtra: Math\.max\(Theme\.space3, root\.navigationInset\)/);
    assert.match(settings, /onDidOpen: \{/);
    assert.match(settings, /onDidClose: root\.mobileIndexOpen = true/);
    assert.match(adaptive, /GenyModal \{[\s\S]*?id: desktopDialog/);
    assert.match(adaptive, /fullScreen: control\.desktopFullScreen/);
    assert.match(adaptive, /width: control\.desktopFullScreen \? \(parent \? parent\.width : control\.width\)/);
    assert.match(adaptive, /height: control\.desktopFullScreen \? \(parent \? parent\.height : control\.height\)/);
    assert.match(adaptive, /sourceItem: control\.desktopFullScreen \? null : control\.blurSourceItem/);
    assert.match(adaptive, /addScrim: !control\.desktopFullScreen && control\.dimBackground/);
    const modal = read('ui/components/controls/GenyModal.qml');
    assert.match(modal, /property bool fullScreen: false/);
    assert.match(modal, /curveDepth: control\.fullScreen \? 0 : Theme\.curveModal/);
    assert.match(modal, /neonAmbient: !control\.fullScreen/);
    assert.match(modal, /duration: control\.fullScreen \? 0 : Theme\.motionStandard/);
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
    assert.match(loadedSettings, /blurSourceItem: null/);
    assert.doesNotMatch(loadedSettings, /blurSourceItem:\s*shellContent/);
    const frost = read('ui/components/visual/DrawerBackdrop.qml');
    assert.match(frost, /interval: Math\.max\(360, Theme\.materialSurfaceDuration \+ 220\)/);
    assert.match(frost, /Math\.min\(0\.48, 1440 \/ Math\.max\(1, root\.width, root\.height\)\)/);
    assert.match(frost, /sourceItem: root\.nativeGaussian \? horizontalPass : null/);
    assert.match(frost, /captureUpdateQueued: false/);
    assert.match(frost, /Qt\.callLater\(root\.performSnapshotUpdate\)/);
    const theme = read('ui/theme/Theme.qml');
    const surfaces = read('ui/components/visual/NeonSurface.qml');
    const plates = read('ui/components/visual/NeonStatePlate.qml');
    assert.match(theme, /function beginPaletteSwitch\(\)/);
    assert.match(theme, /paletteSwitchInProgress: false/);
    assert.equal((surfaces.match(/!Theme\.paletteSwitchInProgress/g) || []).length, 2);
    assert.equal((plates.match(/!Theme\.paletteSwitchInProgress/g) || []).length, 2);
    assert.match(settings, /activeFocusOnTab: true/);
    assert.match(settings, /Accessible\.onPressAction: themePreset\.selectPreset\(\)/);
    assert.match(settings, /borderColor: themePreset\.activeFocus/);
    assert.match(settings, /id: colorThemeLayout/);
    assert.match(settings, /columns: appearanceScroll\.availableWidth >= Theme\.scaleUi\(850\) \? 2 : 1/);
    assert.match(settings, /id: palettePresets/);
    assert.match(settings, /columns: appearanceScroll\.availableWidth >= 520 \? 4/);
    assert.match(settings, /objectName: "textSelectionSegment"/);
    assert.match(settings, /Layout\.maximumWidth: Theme\.scaleUi\(360\)/);
    assert.match(settings, /id: customAccentCard/);
    assert.match(settings, /implicitHeight: customColorContent\.implicitHeight \+ Theme\.space3 \* 2/);
    assert.match(settings, /borderWidth: themePreset\.selected/);
    assert.doesNotMatch(settings, /selected: themePreset\.selected\s*\n\s*focused:/);
    assert.match(settings, /onTapped: themePreset\.selectPreset\(\)/);
});

test('remaining recovery checkboxes share stable Light/Dark appearance', () => {
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
        'revealWords'
    ]) {
        assert.match(setup, new RegExp('GenyCheckRow \\{\\s*id: '+id+'\\b'));
    }
    // Defaults belong to the checkable Button. A constant QML checked:false
    // binding can reset the state or break when Qt processes user toggles.
    const recovery = setup.slice(setup.indexOf('id: revealWords'), setup.indexOf('SetupPrimaryButton {', setup.indexOf('id: revealWords')));
    assert.doesNotMatch(recovery, /checked:\s*false/);
    assert.match(recovery, /onVisibleChanged: if \(!visible\) checked = false/);
    assert.match(setup, /objectName: "nativeRecoveryAutomaticSignerNotice"/);
    assert.doesNotMatch(setup,/objectName: "nativeRecoveryDeviceSignerConsent"/);
    assert.match(setup, /nativeRecoveryGrid\.clearAll\(\)/);
});

test('Legal consent uses the same control palette while preserving its persisted contract', () => {
    const setup = read('ui/components/setup/InitialSetup.qml');
    const legal = setup.slice(setup.indexOf('id: consentCheck'), setup.indexOf('Component {\n        id: accountStage'));
    assert.match(legal, /onClicked: root\.consentChecked = !root\.consentChecked/);
    assert.match(legal, /Accessible\.checked: root\.consentChecked/);
    assert.match(legal, /fillColor: root\.consentChecked \? Theme\.accent : Theme\.surface/);
    assert.match(legal, /borderColor: root\.consentChecked \? Theme\.accent : Theme\.borderStrong/);
    assert.match(legal, /root\.consentChecked \? Theme\.accent : Theme\.surface/);
    assert.match(legal, /fillColor: consentCheck\.down \? Theme\.surfacePressed/);
});

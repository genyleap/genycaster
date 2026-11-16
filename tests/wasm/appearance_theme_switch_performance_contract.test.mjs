import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path, import.meta.url),'utf8');

test('Qt appearance properties have dedicated NOTIFY signals instead of repainting all roles',()=>{
  const header=read('src/presentation/qt/appearance_controller.hpp');
  const source=read('src/presentation/qt/appearance_controller.cpp');
  for(const [field,signal] of Object.entries({
    themeMode:'themeModeChanged', colorThemeId:'colorThemeIdChanged',
    customAccentColor:'customAccentColorChanged', effectiveDark:'effectiveDarkChanged',
    reduceMotion:'reduceMotionChanged', fontScale:'fontScaleChanged',
    interfaceScale:'interfaceScaleChanged'
  })){
    assert.match(header,new RegExp('Q_PROPERTY\\([^\\n]*\\b'+field+
      '\\b[^\\n]*NOTIFY '+signal+'\\)'));
    assert.ok(header.includes('void '+signal+'();'));
  }
  assert.match(source,/setColorThemeId\([\s\S]*?emit colorThemeIdChanged\(\);[\s\S]*?emit appearanceChanged\(\);/);
  assert.match(source,/refreshEffectiveMode\([\s\S]*?emit effectiveDarkChanged\(\);/);
  assert.match(source,/resetAppearance\([\s\S]*?if \(presetChanged\) \{\s*emit colorThemeIdChanged\(\);/);
});

test('Settings palette never recreates all eight model delegates when dark mode changes',()=>{
  const qml=read('ui/components/settings/SettingsPopup.qml');
  assert.match(qml,/model: \["platform", "geny", "ocean", "violet",\s*"ember", "rose", "mint", "mono"\]/);
  assert.match(qml,/required property string modelData/);
  assert.match(qml,/readonly property color presetSwatch:/);
  assert.match(qml,/readonly property string presetLabel:/);
  assert.match(qml,/color: themePreset\.presetSwatch/);
  assert.match(qml,/text: themePreset\.presetLabel/);
  assert.match(qml,/onTapped: themePreset\.selectPreset\(\)/);
  assert.doesNotMatch(qml,/\{ id: "ocean", label:.*Theme\.swatchOcean/);
  assert.match(qml,/id: colorThemeLayout/);
  assert.match(qml,/id: palettePresets/);
  assert.doesNotMatch(qml,/selected: themePreset\.selected\s*\n\s*focused:/);
  assert.match(qml,/GenyAdaptiveDialog \{/);
  assert.match(qml,/mobileFullScreen: true/);
  assert.match(qml,/desktopFullScreen: true/);
  assert.match(qml,/blurSourceItem: null/);
  assert.match(qml,/dimBackground: false/);
  assert.doesNotMatch(qml,/efficientBackdropBlur: true/);
});

test('Qt appearance inspector uses opt-in optimized snapshot blur, other drawers retain the Kit Gaussian',()=>{
  const drawer=read('ui/components/controls/GenyDrawer.qml');
  const backdrop=read('ui/components/visual/DrawerBackdrop.qml');
  const shader=read('ui/shaders/gaussian.frag');
  assert.match(drawer,/property bool efficientBackdropBlur: false/);
  assert.equal((drawer.match(/efficientNativeBlur: control\.efficientBackdropBlur/g)||[]).length,2);
  assert.equal((drawer.match(/sourceItem: control\.fullScreen && control\.efficientBackdropBlur/g)||[]).length,2);
  assert.match(backdrop,/property bool efficientNativeBlur: false/);
  assert.match(backdrop,/nativeGaussian: root\.nativeBlurActive && !root\.efficientNativeBlur/);
  assert.match(backdrop,/optimizedNativeBlur: root\.nativeBlurActive && root\.efficientNativeBlur/);
  assert.match(backdrop,/nativeCaptureScale: root\.efficientNativeBlur[\s\S]*?Math\.min\(0\.48/);
  assert.match(backdrop,/hideSource: root\.nativeBlurActive/);
  assert.match(backdrop,/sourceItem: root\.sourceItem/);
  assert.match(backdrop,/live: false/);
  assert.match(backdrop,/blur: root\.optimizedNativeBlur \? 0\.66 : root\.blurStrength/);
  assert.match(backdrop,/visible: root\.active && \(root\.useBrowserSnapshot \|\| root\.optimizedNativeBlur\)/);
  assert.match(backdrop,/id: paletteSnapshotRefresh[\s\S]*?root\.refreshSnapshot\(\)/);
  assert.match(backdrop,/function onDarkChanged\(\)/);
  assert.match(backdrop,/function onColorThemeIdChanged\(\)/);
  assert.match(backdrop,/function onCustomAccentChanged\(\)/);
  assert.match(shader,/for \(int i = 1; i <= 47; i \+= 2\)/);
});

test('WebAssembly chrome and MiniApp media hosts are not updated on unrelated palette changes',()=>{
  const main=read('ui/app/main.cpp');
  assert.match(main,/AppearanceController::effectiveDarkChanged,[\s\S]*?updateBrowserChromeTheme/);
  assert.match(main,/AppearanceController::reduceMotionChanged, &miniAppHost/);
  assert.match(main,/AppearanceController::reduceMotionChanged, &mediaWebHost/);
  assert.doesNotMatch(main,/AppearanceController::appearanceChanged/);
});

test('native Qt media smoke verifies real QSignalSpy notification isolation',()=>{
  const smoke=read('tests/ui/media_scroll_tests.cpp');
  assert.match(smoke,/QSignalSpy presetChanged\{&appearance, &AppearanceController::colorThemeIdChanged\}/);
  assert.match(smoke,/Accent selection notifies only colorThemeId/);
  assert.match(smoke,/Selecting the current palette does not emit redundant repaint signals/);
  assert.match(smoke,/Reset publishes the changed appearance properties independently/);
});

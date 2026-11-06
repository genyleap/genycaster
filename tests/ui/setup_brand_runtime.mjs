// Exercises the production Setup rail with existing QSB assets; never builds.
// Usage: node tests/ui/setup_brand_runtime.mjs /path/to/qmltestrunner [artifact-dir]
// Requires a GPU-backed Qt platform (Cocoa/Metal on macOS).
import { readFileSync, writeFileSync, mkdtempSync, mkdirSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';

const runner = process.argv[2];
assert.ok(runner, 'Pass an installed Qt qmltestrunner path');
const artifacts = process.argv[3] ? resolve(process.argv[3]) : '';
if (artifacts) mkdirSync(artifacts, { recursive: true });
const source = readFileSync(new URL('../../ui/components/InitialSetup.qml', import.meta.url), 'utf8');
const start = source.indexOf('        Rectangle {\n            id: desktopBrandPane');
const end = source.indexOf('\n        // The exact same eight-stage indicator', start);
assert.ok(start >= 0 && end > start);
const pane = source.slice(start, end).replaceAll('qrc:/qt/qml/Farcaster/Client/shaders/',
    new URL('../../ui/shaders/', import.meta.url).href);
const directory = mkdtempSync(join(tmpdir(), 'genycaster-setup-brand-'));
try {
    writeFileSync(join(directory, 'qmldir'), `singleton Theme 1.0 Theme.qml
singleton LocaleFormat 1.0 LocaleFormat.qml
`);
    writeFileSync(join(directory, 'Theme.qml'), `pragma Singleton
import QtQuick
QtObject {
 property bool dark: false
 property bool reduceMotion: false
 property bool powerSave: false
 property color accent: "#3266fa"
 property string latinFontFamily: "Arial"
}`);
    writeFileSync(join(directory, 'LocaleFormat.qml'), `pragma Singleton
import QtQml
QtObject { function localizeDigits(v) { return String(v) } }`);
    const prefix = `import QtQuick
import QtQuick.Window
import QtQuick.Effects
import QtQuick.Shapes
import QtTest
Item {
 id: root
 width: 1482; height: 860
 property bool phone: false
 property int currentStep: 0
 property int progressCount: 8
 property string artifactDirectory: ARTIFACT_DIRECTORY
`.replace('ARTIFACT_DIRECTORY', JSON.stringify(artifacts));
    writeFileSync(join(directory, 'tst_Studio.qml'), prefix + pane + `
 TestCase {
  name: "SetupBrandStudio"
  when: windowShown
  function initTestCase() { root.Window.window.requestActivate() }
  function test_capture() {
   desktopBrandPane.autoOrbitEnabled = false
   desktopBrandPane.orbitYaw = 0
   for (const dark of [false, true]) {
    Theme.dark = dark
    wait(200)
    if (artifactDirectory.length > 0)
     grabImage(desktopBrandPane).save(artifactDirectory + "/" + (dark ? "dark" : "light") + ".png")
   }
  }
  function test_motion_lifecycle() {
   Theme.reduceMotion = false
   Theme.powerSave = false
   desktopBrandPane.autoOrbitEnabled = true
   tryCompare(desktopBrandPane, "motionAllowed", true)
   const yaw = desktopBrandPane.orbitYaw
   tryVerify(function() { return desktopBrandPane.orbitYaw !== yaw })
   for (const guard of ["powerSave", "reduceMotion"]) {
    Theme[guard] = true
    const paused = desktopBrandPane.orbitYaw
    root.currentStep++
    wait(100)
    compare(desktopBrandPane.orbitYaw, paused)
    compare(desktopBrandPane.tapImpulse, 0)
    Theme[guard] = false
   }
   root.currentStep++
   wait(80)
   verify(desktopBrandPane.tapImpulse > 0)
   root.visible = false
   const hiddenYaw = desktopBrandPane.orbitYaw
   wait(100)
   compare(desktopBrandPane.orbitYaw, hiddenYaw)
   compare(desktopBrandPane.tapImpulse, 0)
   root.visible = true
  }
  function test_reset_and_zoom() {
   Theme.reduceMotion = true
   desktopBrandPane.dragYaw = 120
   desktopBrandPane.dragPitch = 60
   desktopBrandPane.changeZoom(100)
   compare(desktopBrandPane.viewZoom, 1.4)
   desktopBrandPane.changeZoom(-100)
   compare(desktopBrandPane.viewZoom, 0.78)
   desktopBrandPane.resetView()
   compare(desktopBrandPane.dragYaw, -18)
   compare(desktopBrandPane.dragPitch, -5)
   compare(desktopBrandPane.viewZoom, 1)
  }
 }
}
`);
    const result = spawnSync(runner, ['-input', directory], {
        encoding: 'utf8', timeout: 30_000,
        env: { ...process.env,
            ...(process.platform === 'darwin' ? { QT_QPA_PLATFORM: 'cocoa', QSG_RHI_BACKEND: 'metal' } : {}),
            QT_QUICK_BACKEND: '', QT_QUICK_CONTROLS_STYLE: 'Basic',
            QT_FATAL_WARNINGS: '1', QML_DISABLE_DISK_CACHE: '1' },
    });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    if (result.error) throw result.error;
    assert.equal(result.status, 0, 'Setup studio runtime regressions (signal: ' + result.signal + ')');
} finally {
    rmSync(directory, { recursive: true, force: true });
}

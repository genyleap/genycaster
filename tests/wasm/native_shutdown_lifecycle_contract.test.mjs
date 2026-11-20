import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const read = path => readFileSync(resolve(root, path), "utf8");

test("native shutdown owns QML teardown before C++ runtime/controller stack unwinding", () => {
    const main = read("ui/app/main.cpp");
    const teardown = read("ui/app/bootstrap/teardown.cpp");
    const segment = main.slice(main.lastIndexOf("const int exitCode = application.exec();"));
    assert.match(segment, /const int exitCode = application\.exec\(\);\s*#if !defined\(Q_OS_WASM\)/);
    const stopped = segment.indexOf("shutdownScope.stop();");
    const joined = segment.indexOf("bootstrap.join();");
    const released = segment.indexOf("genycaster::ui::bootstrap::releaseQmlRoots(engine, mainWindow);");
    const returned = segment.indexOf("return exitCode;");
    assert.ok(stopped >= 0 && joined > stopped
        && released > joined && returned > released,
        "the main thread must stop producers, join workers and explicitly release the scene graph");

    const incubate = teardown.indexOf('controller->incubateFor(');
    const detachIncubator = teardown.indexOf('engine.setIncubationController(nullptr);');
    const sceneCleanup = teardown.indexOf('child->inherits("QQuickOverlay")');
    const sceneDelete = teardown.indexOf('delete item.data();');
    const popupCleanup = teardown.indexOf('obj->inherits("QQuickPopup")');
    const popupDelete = teardown.indexOf('delete popup;');
    const overlayCleanup = teardown.indexOf('obj->inherits("QQuickOverlay")');
    const overlayDelete = teardown.indexOf('delete overlay.data();');
    const roots = teardown.indexOf("engine.rootObjects()");
    const destroy = teardown.indexOf("delete root;");
    const drain = teardown.lastIndexOf("QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);");
    const collect = teardown.indexOf("engine.collectGarbage();");
    assert.ok(incubate >= 0 && detachIncubator > incubate && sceneCleanup > detachIncubator
        && sceneDelete > sceneCleanup
        && popupCleanup > sceneDelete && popupDelete > popupCleanup
        && overlayCleanup > popupDelete && overlayDelete > overlayCleanup
        && roots > overlayDelete && destroy > roots
        && collect > destroy && drain > collect,
        "destroy QML owners, remaining Popups, Overlay, and only then QML windows");
    assert.match(teardown, /for \(QObject\* const root : engine\.rootObjects\(\)\)/);
    assert.match(teardown, /QPointer<QObject>/);
    assert.doesNotMatch(teardown, /child->setParentItem\(nullptr\)/,
        "Qt 6.12 macOS crashes when visual teardown disconnects attached overlays");
    assert.doesNotMatch(teardown, /mainWindow->hide\(\)/,
        "hiding Cocoa before QQuickOverlay destruction invalidates its window association");
    assert.doesNotMatch(teardown,/mainWindow->releaseResources\(\)/,
        "releasing the scene graph ahead of Qt Quick overlay teardown caused a macOS crash");
    assert.doesNotMatch(teardown,/setPersistentGraphics\(false\)|setPersistentSceneGraph\(false\)/);
    assert.match(main, /#include <QEvent>/);
    assert.doesNotMatch(segment, /#if defined\(Q_OS_ANDROID\)/);
    assert.doesNotMatch(segment, /qInstallMessageHandler|QT_LOGGING_RULES|QT_FATAL_WARNINGS=0/);
});

test("QML debugger stays a Debug-only tool; release diagnostics are not hidden", () => {
    const cmake = read("ui/CMakeLists.txt");
    const rootCmake = read("CMakeLists.txt");
    const main = read("ui/app/main.cpp");
    assert.doesNotMatch(main, /qInstallMessageHandler|QT_LOGGING_RULES|QT_NO_WARNING_OUTPUT/);
    assert.doesNotMatch(cmake + rootCmake, /-Wno-|QT_NO_WARNING_OUTPUT|QT_LOGGING_RULES/);
});

test("README restores colorful C++26 feature badge without placing version in marketing copy", () => {
    const readme = read("README.md");
    assert.match(readme, /https:\/\/img\.shields\.io\/badge\/C%2B%2B%20Version-26-blue\.svg/);
    assert.match(readme, /https:\/\/img\.shields\.io\/badge\/Qt-6\.12-brightgreen\.svg/);
    assert.match(readme, /https:\/\/img\.shields\.io\/badge\/License-GPLv3-blue\.svg/);
    assert.match(readme, /<p>One shared Modern C\+\+ core\./);
    assert.doesNotMatch(readme, /<p>One shared C\+\+26 core/);
});

test("NixOS flake remains at the repository root and CI scripts stay separate", () => {
    const flake = read("flake.nix");
    const workflow = read(".github/workflows/ci.yml");
    const readme = read("README.md");
    assert.match(flake, /packages = forAll/);
    assert.match(flake, /mainProgram = "Genycaster"/);
    assert.match(workflow, /nix build "\.#packages\./);
    assert.match(readme, /root \x60flake\.nix\x60 is the NixOS package entrypoint/);
    assert.match(readme, /The \x60ci\/\x60 directory holds source validation/);
});

test("popup-host QML bindings stay null-safe during native scene destruction", () => {
    const adaptive = read("ui/components/controls/GenyAdaptiveDialog.qml");
    const tasks = read("ui/components/apps/TaskManagerPopup.qml");
    const shell = read("ui/components/shell/AppShell.qml");
    const composer = read("ui/components/composer/ComposerDrawer.qml");
    const video = read("ui/components/media/GenyVideoPlayer.qml");
    const profile = read("ui/components/profile/ProfileSurface.qml");
    const multi = read("ui/components/controls/GenyMultiSelect.qml");
    const combo = read("ui/components/controls/GenyComboBox.qml");
    const filter = read("ui/components/controls/FilterDropdown.qml");
    const stats = read("ui/components/home/StatsPanel.qml");
    const edit = read("ui/components/controls/BidiTextArea.qml");

    for (const qml of [adaptive, tasks])
        assert.match(qml, /\(desktopDialog && desktopDialog\.opened\)/);
    assert.match(shell, /\(composerDrawer && composerDrawer\.opened\)/);
    assert.match(shell, /widgetsOpen: !!contextDrawer && contextDrawer\.visible/);
    assert.match(shell, /accountScroll \? accountScroll\.availableWidth : 0/);
    assert.match(shell, /nativeRecoveryScroll \? nativeRecoveryScroll\.availableWidth : 0/);
    assert.match(composer, /topicFilterNeedle: topicSearch/);
    assert.doesNotMatch(composer, /topicSearch\.text\.trim\(\)/);
    assert.match(video, /\(qualityPopup && qualityPopup\.opened\)/);
    assert.match(video, /x: Math\.max\(0, \(parent \? parent\.width : 0\) - width - Theme\.space4\)/);
    assert.doesNotMatch(video, /anchors\.(?:top|right): fullscreenHost \? fullscreenHost\.(?:top|right) : null/);
    assert.match(profile, /!profileStatusEditor \|\| profileStatusEditor\.selectedMood/);
    assert.match(multi, /readonly property bool pickerVisible: !!picker && picker\.visible/);
    for (const qml of [combo, filter])
        assert.match(qml, /readonly property bool popupVisible: !!control\.popup && control\.popup\.visible/);
    assert.match(stats, /width: periodMenu \? periodMenu\.width/);
    assert.match(edit, /if \(selectionBar\) selectionBar\.close\(\)/);
    assert.doesNotMatch(edit, /(?<!\) )selectionBar\.close\(\)/);
});

import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const read = path => readFileSync(resolve(root, path), "utf8");

test("native shutdown owns QML teardown before C++ runtime/controller stack unwinding", () => {
    const main = read("ui/app/main.cpp");
    const segment = main.slice(main.lastIndexOf("const int exitCode = application.exec();"));
    assert.match(segment, /const int exitCode = application\.exec\(\);\s*#if !defined\(__EMSCRIPTEN__\)/);
    const stopped = segment.indexOf("shutdownScope.stop();");
    const joined = segment.indexOf("bootstrap.worker.join();");
    const release = segment.indexOf("mainWindow->releaseResources();");
    const roots = segment.indexOf("engine.rootObjects()");
    const destroy = segment.indexOf("delete rootObject;");
    const drain = segment.indexOf("QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);");
    const collect = segment.indexOf("engine.collectGarbage();");
    const returns = segment.indexOf("return exitCode;");
    assert.ok(stopped >= 0 && joined > stopped && release > joined
        && roots > release && destroy > roots && drain > destroy
        && collect > drain && returns > collect,
        "cleanup order must stop asynchronous producers, delete QML roots, then drain Qt deletions");
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

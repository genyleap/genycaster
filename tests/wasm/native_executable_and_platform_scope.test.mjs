import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync, mkdtempSync, mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const source = name => readFileSync(path.join(root, name), "utf8");

test("native executable output uses Genycaster while Qt targets and web assets keep their identities", () => {
    const cmake = source("ui/CMakeLists.txt");
    assert.match(cmake, /qt_add_executable\(genycaster\b/);
    assert.match(cmake,
        /if\(NOT ANDROID AND NOT EMSCRIPTEN\)\s+set_target_properties\(genycaster PROPERTIES OUTPUT_NAME "Genycaster"\)\s+endif\(\)/);
    assert.match(cmake, /qt_add_resources\(genycaster genycaster_app_icon/);
    const macPackage = source("properties/macOS/Configure.cmake");
    const iosPackage = source("properties/iOS/Configure.cmake");
    assert.match(cmake, /properties\/ConfigureTarget\.cmake/);
    assert.match(macPackage, /MACOSX_BUNDLE_BUNDLE_NAME "Genycaster"/);
    assert.match(iosPackage, /MACOSX_BUNDLE_BUNDLE_NAME "Genycaster"/);
    assert.match(macPackage, /configure\.cmake/);

    const workflow = source(".github/workflows/ci.yml");
    assert.match(workflow, /build\/native\/bin\/Genycaster\b/);
    assert.match(workflow, /bin\/Genycaster\.app/);
    assert.match(workflow, /Contents\/MacOS\/Genycaster/);
    assert.match(workflow, /Print :CFBundleName/);
    assert.match(workflow, /Print :CFBundleExecutable/);
    assert.match(workflow, /bin\/Genycaster\.dmg/);
    assert.match(workflow, /bin\/Genycaster\.exe/);
    assert.match(workflow, /dist\/app\/Genycaster\.exe/);
    assert.match(workflow, /-name 'Genycaster\.app'/);
    assert.match(workflow, /"\$APP\/Genycaster"/);
    assert.match(workflow, /result\/bin\/Genycaster/);
    assert.doesNotMatch(workflow,
        /build\/native\/bin\/genycaster(?:\.app|\.dmg|\.exe|\b)|result\/bin\/genycaster\b|Contents\/MacOS\/genycaster/);

    const desktop = source("properties/Linux/genycaster.desktop");
    assert.match(desktop, /^Name=GenyCaster$/m);
    assert.match(desktop, /^Exec=Genycaster$/m);
    assert.match(desktop, /^Icon=genycaster$/m);

    const nix = source("flake.nix");
    assert.match(nix, /install -Dm755 bin\/Genycaster "\$out\/bin\/Genycaster"/);
    assert.match(nix, /mainProgram = "Genycaster"/);

    const verifier = source("ci/verify_platform_package.py");
    assert.match(verifier, /root = "Genycaster\.app"/);
    assert.match(verifier, /root \+ "\/Genycaster" in names/);
    assert.match(verifier, /name == "Genycaster\.exe"/);
    assert.match(verifier, /"genycaster\.wasm"/);
    assert.match(verifier, /"genycaster\.js"/);
});

test("WebAssembly text selection is described as a browser canvas boundary, not iOS-only", () => {
    const cpp = source("ui/app/settings/selection.cpp");
    assert.match(cpp, /^#if defined\(Q_OS_WASM\)$/m);
    assert.doesNotMatch(cpp, /__EMSCRIPTEN__/);
    assert.match(cpp, /Browser-native selection operates on DOM text/);
    assert.doesNotMatch(cpp, /Native iOS Safari selection/);

    const emoji = source("ui/app/browser/emoji_recovery.hpp");
    assert.match(emoji, /A Qt WebAssembly input adapter/);
    assert.doesNotMatch(emoji, /iOS Safari \/ Qt WebAssembly/);
});

test("legacy macOS bundle directory is normalized without losing executable data", (t) => {
    const cmake = spawnSync("cmake", ["--version"], {encoding: "utf8"});
    if (cmake.error || cmake.status !== 0) {
        t.skip("CMake is unavailable in this test environment");
        return;
    }
    const temporary = mkdtempSync(path.join(tmpdir(), "genycaster-bundle-case-"));
    try {
        const rootDir = path.join(temporary, "bin");
        const oldBundle = path.join(rootDir, "genycaster.app");
        const newBundle = path.join(rootDir, "Genycaster.app");
        const sourceBinary = path.join(oldBundle, "Contents", "MacOS", "Genycaster");
        mkdirSync(path.dirname(sourceBinary), {recursive: true});
        writeFileSync(sourceBinary, "application-binary-content");
        const runMigration = () => spawnSync(
            "cmake", [
                "-DGENYCASTER_BUNDLE_PATH=" + newBundle,
                "-P", path.join(root, "cmake/platform/macOS/configure.cmake")
            ],
            {encoding: "utf8"}
        );
        const first = runMigration();
        assert.equal(first.status, 0, first.stderr || first.stdout);
        assert.deepEqual(readdirSync(rootDir), ["Genycaster.app"]);
        assert.equal(readFileSync(path.join(newBundle, "Contents", "MacOS", "Genycaster"), "utf8"),
                     "application-binary-content");
        const second = runMigration();
        assert.equal(second.status, 0, second.stderr || second.stdout);
        assert.deepEqual(readdirSync(rootDir), ["Genycaster.app"]);
    } finally {
        rmSync(temporary, {recursive: true, force: true});
    }
});

test("our Apple Mini App code avoids deprecated NSBox border types and keeps warnings enabled", () => {
    const objc = source("ui/app/miniapp/browser_macos.mm");
    assert.match(objc, /GenyMiniAppLoadingSurface : NSView/);
    assert.match(objc, /viewDidChangeEffectiveAppearance/);
    assert.doesNotMatch(objc, /borderType\s*=/);
    assert.doesNotMatch(objc, /#pragma clang diagnostic ignored/);
    assert.doesNotMatch(objc, /<AppKit\/AppKit\.h>|<WebKit\/WebKit\.h>/);
    assert.match(objc, /<AppKit\/NSView\.h>/);
    assert.match(objc, /<WebKit\/WKWebView\.h>/);
    // NavigationDelegate and WebViewConfiguration forward-declare these
    // classes; reading their properties requires the concrete SDK headers.
    for (const className of ["WKPreferences", "WKNavigationAction", "WKNavigationResponse"]) {
        assert.match(objc, new RegExp("#import <WebKit/" + className + "\\.h>"));
    }
    assert.doesNotMatch(source("ui/CMakeLists.txt"), /-Wno-deprecated-declarations/);
    const sdkBridge = source("cmake/platform/macOS/configure.cmake");
    assert.match(source("ui/CMakeLists.txt"),
        /include\("\$\{CMAKE_SOURCE_DIR\}\/cmake\/platform\/macOS\/configure\.cmake"\)/);
    assert.match(sdkBridge, /OBJCXX_STANDARD 26/);
    assert.match(sdkBridge, /OBJCXX_STANDARD_REQUIRED ON/);
    assert.match(sdkBridge, /OBJCXX_EXTENSIONS OFF/);
    assert.match(sdkBridge, /xcrun --sdk macosx --find clang\+\+/);
    assert.match(sdkBridge, /check_objcxx_source_compiles/);
    assert.match(sdkBridge, /GENYCASTER_MACOS_OBJCXX26_SDK_COMPATIBLE/);
    assert.match(sdkBridge, /CMAKE_OBJCXX_COMPILER_LOADED/);
    assert.match(sdkBridge, /set\(CMAKE_OBJCXX_COMPILER.*?CACHE FILEPATH/s);
    assert.match(sdkBridge, /FORCE\)/);
    assert.match(sdkBridge, /get_filename_component\(_genycasterExpectedObjcxx/);
    assert.match(sdkBridge, /Clear CMake Configuration/);
    assert.match(sdkBridge, /-fobjc-arc/);
    assert.match(source("ui/CMakeLists.txt"),
        /target_compile_features\(farcaster_miniapp_runtime PUBLIC cxx_std_26\)/);
    assert.doesNotMatch(sdkBridge, /OBJCXX_STANDARD 23|-Wno-|#pragma clang diagnostic ignored/);
});

test("Mini App FID comparison uses exact integers rather than lossy doubles", () => {
    const runtime = source("ui/app/miniapp/runtime.cpp");
    assert.match(runtime, /contextFid\s*=\s*session->context\.value/);
    assert.match(runtime, /toInteger\(-1\)/);
    assert.match(runtime, /contextFid < 0/);
    assert.match(runtime, /static_cast<qulonglong>\(contextFid\)/);
    const recovery = source("src/platform/custody_recovery.cpp");
    assert.match(recovery, /static_cast<unsigned char>\(character\)/);
    const controller = source("ui/app/identity/connections.cpp");
    assert.doesNotMatch(controller, /\[this,reply,requestGeneration,viewer,currentTopic,offset,pageSize\]/);
});

test("Linux AppImage delta process never enters a WebAssembly build", () => {
    const implementation = source("ui/app/update/controller.cpp");
    const header = source("ui/app/update/controller.hpp");
    const linuxOnly = "#if defined(Q_OS_LINUX) && !defined(Q_OS_WASM)";
    assert.match(implementation, /#if defined\(Q_OS_LINUX\) && !defined\(Q_OS_WASM\)\s*#include <QProcess>\s*#endif/);
    assert.match(implementation, /UpdateController::~UpdateController\(\)[\s\S]*?#if defined\(Q_OS_LINUX\) && !defined\(Q_OS_WASM\)\s*if \(m_deltaProcess\)/);
    assert.match(header, /#if defined\(Q_OS_LINUX\) && !defined\(Q_OS_WASM\)\s*QPointer<QProcess> m_deltaProcess;\s*#endif/);
    assert.match(implementation, /#if defined\(Q_OS_LINUX\) && !defined\(Q_OS_WASM\)[\s\S]*?auto\* const delta = new QProcess\(this\)/);
    assert.ok(implementation.includes(linuxOnly));
});

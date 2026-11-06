import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
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

    const workflow = source(".github/workflows/ci.yml");
    assert.match(workflow, /build\/native\/bin\/Genycaster\b/);
    assert.match(workflow, /bin\/Genycaster\.app/);
    assert.match(workflow, /Contents\/MacOS\/Genycaster/);
    assert.match(workflow, /bin\/Genycaster\.dmg/);
    assert.match(workflow, /bin\/Genycaster\.exe/);
    assert.match(workflow, /dist\/app\/Genycaster\.exe/);
    assert.match(workflow, /-name 'Genycaster\.app'/);
    assert.match(workflow, /"\$APP\/Genycaster"/);
    assert.match(workflow, /result\/bin\/Genycaster/);
    assert.doesNotMatch(workflow,
        /build\/native\/bin\/genycaster(?:\.app|\.dmg|\.exe|\b)|result\/bin\/genycaster\b|Contents\/MacOS\/genycaster/);

    const desktop = source("ui/platform/linux/genycaster.desktop");
    assert.match(desktop, /^Name=GenyCaster$/m);
    assert.match(desktop, /^Exec=Genycaster$/m);
    assert.match(desktop, /^Icon=genycaster$/m);

    const nix = source("flake.nix");
    assert.match(nix, /install -Dm755 bin\/Genycaster "\$out\/bin\/Genycaster"/);
    assert.match(nix, /mainProgram = "Genycaster"/);

    const verifier = source("ci/verify_platform_package.py");
    assert.match(verifier, /root = "Genycaster\.app"/);
    assert.match(verifier, /root \+ "\/Genycaster" in names/);
    assert.match(verifier, /"Genycaster\.exe" in exact_names/);
    assert.match(verifier, /"genycaster\.wasm"/);
    assert.match(verifier, /"genycaster\.js"/);
});

test("WebAssembly text selection is described as a browser canvas boundary, not iOS-only", () => {
    const cpp = source("ui/app/text_selection_settings.cpp");
    assert.match(cpp, /#if defined\(Q_OS_WASM\) \|\| defined\(__EMSCRIPTEN__\)/);
    assert.match(cpp, /Browser-native selection operates on DOM text/);
    assert.doesNotMatch(cpp, /Native iOS Safari selection/);

    const emoji = source("ui/app/browser_emoji_recovery.hpp");
    assert.match(emoji, /A Qt WebAssembly input adapter/);
    assert.doesNotMatch(emoji, /iOS Safari \/ Qt WebAssembly/);
});

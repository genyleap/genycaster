import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const read = (rel) => readFileSync(path.join(root, rel), "utf8");

test("Qt application source layout is platform-neutral and ships no demo data", () => {
    assert.equal(existsSync(path.join(root, "ui/mobile")), false);
    assert.equal(existsSync(path.join(root, "ui/demo")), false);
    assert.equal(existsSync(path.join(root, "ui/components/shell/Main.qml")), true);
    assert.equal(existsSync(path.join(root, "ui/app/main.cpp")), true);

    const cmake = read("ui/CMakeLists.txt");
    assert.match(cmake, /components\/shell\/Main\.qml/);
    assert.match(cmake, /if\(qmlFile STREQUAL "components\/shell\/Main.qml"\)/);
    assert.match(cmake, /QT_RESOURCE_ALIAS "\$\{qmlResourceAlias\}"/);
    assert.doesNotMatch(cmake, /demo\/DemoSocialData|mobile\/Main\.qml/);
});

test("native UI tests belong to tests/ui and their source includes resolve", () => {
    assert.equal(existsSync(path.join(root, "ui/tests")), false);
    assert.doesNotMatch(read("ui/CMakeLists.txt"), /\badd_test\s*\(/);
    assert.doesNotMatch(read("ui/CMakeLists.txt"), /\b(?:qt_)?add_executable\s*\(\s*\w+_tests\b/);
    assert.match(read("tests/CMakeLists.txt"), /if\(FARCASTER_BUILD_UI\)\s+add_subdirectory\(ui\)/);
    assert.match(read("tests/ui/CMakeLists.txt"), /add_executable\(genycaster_background_shutdown_tests/);
    const sourceDir = path.join(root, "tests/ui");
    const testSources = readdirSync(sourceDir).filter((name) => name.endsWith(".cpp"));
    let checkedIncludes = 0;
    for (const name of testSources) {
        const source = read(path.join("tests/ui", name));
        for (const match of source.matchAll(/^\s*#include\s+"(\.\.\/[^"]+)"/gm)) {
            const resolved = path.resolve(sourceDir, match[1]);
            assert.equal(existsSync(resolved), true, `${name}: missing ${match[1]}`);
            ++checkedIncludes;
        }
    }
    assert.ok(checkedIncludes >= 8, "expected native UI test headers");
    const iconRasterCmake = read("tests/icon_raster/CMakeLists.txt");
    assert.match(iconRasterCmake, /\.\.\/\.\.\/ui\/app/);
    assert.doesNotMatch(iconRasterCmake, /\.\.\/\.\.\/ui\/mobile/);
});

test("sparkline renders in QML while numeric geometry is generated in C++", () => {
    const qml = read("ui/components/content/TokenSparkline.qml");
    const cmake = read("ui/CMakeLists.txt");
    const header = read("ui/render/sparkline.hpp");
    const source = read("ui/render/sparkline.cpp");
    assert.match(qml, /TokenSparklineGeometry\.compute\(/);
    assert.match(qml, /PathPolyline/);
    assert.doesNotMatch(qml, /\bfunction\s+(?:finiteValues|decimatedValues|smoothedValues|buildPoints|buildAreaPoints)\b/);
    assert.match(cmake, /render\/sparkline\.cpp/);
    assert.match(cmake, /render\/sparkline\.hpp/);
    assert.match(header, /QML_SINGLETON/);
    assert.match(header, /Q_INVOKABLE QVariantMap compute/);
    assert.match(source, /finiteSamples|decimate/);
});

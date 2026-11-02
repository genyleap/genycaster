import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

const qml = readFileSync(new URL("../../ui/mobile/Main.qml", import.meta.url), "utf8");

const extract = (start, end) => {
    const a = qml.indexOf(start);
    const b = qml.indexOf(end, a + start.length);
    assert.ok(a >= 0 && b > a, "QML sizing expression exists");
    return qml.slice(a + start.length, b).trim();
};
const nativeDesktopExpr = extract(
    "    readonly property bool nativeDesktop: ",
    "\n    width: "
);
const widthExpr = extract("    width: ", "\n    height: ");
const heightExpr = extract("    height: ", "\n    // Browsers can expose");
const nativeDesktop = new Function("Qt", "return (" + nativeDesktopExpr + ")");
const initialWidth = new Function("nativeDesktop", "Screen", "return (" + widthExpr + ")");
const initialHeight = new Function("nativeDesktop", "Screen", "return (" + heightExpr + ")");

test("native Windows, macOS and Linux launch with desktop layout", () => {
    for (const os of ["windows", "osx", "linux"]) {
        assert.equal(nativeDesktop({ platform: { os } }), true);
        const width = initialWidth(true, { desktopAvailableWidth: 1920 });
        const height = initialHeight(true, { desktopAvailableHeight: 1080 });
        assert.deepEqual([width, height], [1280, 800]);
        assert.ok(width > 720, "not mistaken for mobile shell at launch");
    }
});

test("window fits the available desktop geometry without changing user resize", () => {
    for (const width of [800, 1024, 1280, 1366, 1440, 1920, 3840]) {
        const initial = initialWidth(true, { desktopAvailableWidth: width });
        assert.ok(initial <= width - 64 && initial >= 360);
    }
    for (const height of [720, 768, 900, 1080, 2160]) {
        const initial = initialHeight(true, { desktopAvailableHeight: height });
        assert.ok(initial <= height - 64 && initial >= 640);
    }
    assert.doesNotMatch(qml, /onWidthChanged\s*:|onHeightChanged\s*:/);
});

test("phones and browser keep their previous platform-controlled viewport", () => {
    for (const os of ["android", "ios", "wasm"]) {
        assert.equal(nativeDesktop({ platform: { os } }), false);
        assert.equal(initialWidth(false, { desktopAvailableWidth: 1920 }), 430);
        assert.equal(initialHeight(false, { desktopAvailableHeight: 1080 }), 860);
    }
    assert.match(qml, /minimumWidth: Qt\.platform\.os === "wasm" \? 0 : 360/);
    assert.match(qml, /minimumHeight: Qt\.platform\.os === "wasm" \? 0 : 640/);
});

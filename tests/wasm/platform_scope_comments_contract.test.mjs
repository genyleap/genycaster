import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const read = (relative) => readFileSync(path.join(root, relative), "utf8");

test("WebAssembly text selection describes the browser DOM/Qt canvas boundary", () => {
    const implementation = read("ui/app/settings/selection.cpp");
    const section = implementation.match(
        /bool TextSelectionSettings::nativeAvailable\(\) const noexcept\s*\{([\s\S]*?)\n\}/,
    )?.[1];
    assert.ok(section, "text selection availability implementation must exist");
    assert.match(section, /return true;/);
    assert.doesNotMatch(section, /__EMSCRIPTEN__/);
    assert.match(section, /selection operates on DOM text/);
    assert.doesNotMatch(section, /\biOS\b|\bmacOS\b|\bSafari\b/);
    assert.match(read("ui/app/settings/selection.hpp"), /browser DOM selection/);
});

test("browser input and async network cancellation comments describe shared behavior", () => {
    const emoji = read("ui/app/browser/emoji_recovery.hpp");
    const header = emoji.slice(0, emoji.indexOf("#if defined(Q_OS_WASM)"));
    assert.match(header, /Qt WebAssembly input adapter/);
    assert.doesNotMatch(header, /\biOS\b|\bmacOS\b|\bSafari\b/);

    const activity = read("ui/app/social/activity.cpp");
    assert.match(activity, /QNetworkReply::abort\(\) can emit finished\(\) inline on Qt network backends/);
    const bridge = read("src/presentation/qt/presentation_bridge.cpp");
    assert.match(bridge, /Qt's virtual clipboard may remain local to the WASM/);
    assert.match(bridge, /browser Clipboard API/i);
});

test("real platform-specific implementation boundaries retain their names", () => {
    assert.match(read("src/platform/signer_keychain_boundary.hpp"), /Apple Security\/CoreFoundation/);
    assert.match(read("ui/app/miniapp/host.cpp"), /native macOS WKWebView/);
    assert.match(read("src/providers/emoji/emoji_asset_validator.cpp"), /Android NDK 27/);
});

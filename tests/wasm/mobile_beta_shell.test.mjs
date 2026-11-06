import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const read = filename => readFileSync(new URL("../../"+filename,import.meta.url),"utf8");
test("public app identity stays independent from the injected release label",()=>{
    const cmake=read("CMakeLists.txt");
    const pack=read("ci/package_wasm_assets.py");
    const html=read("ui/wasm/genycaster-shell.html");
    const manifest=JSON.parse(read("properties/WebAssembly/genycaster.webmanifest"));
    assert.match(cmake,/VERSION 1\.0\.0/);
    assert.equal(manifest.name,"Genycaster");
    assert.match(html,/<title>@@GENYCASTER_PRODUCT_NAME@@<\/title>/);
    assert.match(html,/class="boot-beta">BETA<\/span>/);
    assert.match(pack, /product_placeholder = "@@GENYCASTER_PRODUCT_NAME@@"/);
    assert.match(pack, /release_placeholder = "@@GENYCASTER_RELEASE_VERSION@@"/);
});

test("iOS safe-area background and PWA metadata use the app appearance",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const js=read("ui/wasm/genycaster-loader.js");
    const cpp=readGenycasterMain();
    const manifest=JSON.parse(read("properties/WebAssembly/genycaster.webmanifest"));
    assert.match(html,/viewport-fit=cover/);
    assert.match(html,/<meta name="theme-color" content="#0b0c0f">/);
    assert.doesNotMatch(html,/<meta name="apple-mobile-web-app-status-bar-style"/);
    assert.match(html,/background:var\(--app-chrome\)/);
    assert.equal(manifest.theme_color,"#0b0c0f");
    assert.match(js,/__genycasterSetBrowserChromeTheme=applyBrowserChromeTheme/);
    assert.match(cpp,/updateBrowserChromeTheme/);
    assert.match(cpp,/AppearanceController::appearanceChanged/);
    assert.match(cpp,/#if defined\(__EMSCRIPTEN__\)/);
});

test("phone Home is focused on three active actions and a locked Wallet",()=>{
 const home=read("ui/components/HomeDashboard.qml");
 assert.match(home,/component HubWorkspace: ColumnLayout/);
 assert.match(home,/columns: 2/);
 assert.match(home,/"homeShortcut_" \+ modelData.destination/);
 assert.match(home,/destination: "wallet"/);
 assert.doesNotMatch(home,/destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(home,/GenyTokenBanner \{/);
});

test("Boot Beta badge and footer remain in separate non-overlapping rows",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const loader=read("ui/wasm/genycaster-loader.js");
    assert.match(html,/<span class="boot-beta">BETA<\/span>/);
    assert.doesNotMatch(html,/id="boot-note"/);
    assert.doesNotMatch(html,/id="boot-version"/);
    assert.match(html,/class="boot-credit"><div>Powered by <strong>Genyleap<\/strong> · WASM Engine<\/div><div class="boot-build" id="boot-build-credit"/);
    assert.match(html,/\.boot-credit\{[^}]*flex-direction:column/);
    assert.match(html,/\.boot-beta\{[^}]*text-align:center/);
    for(const label of ["Installing Software…","Updating Software…","Initializing Runtime…","Opening Genycaster…"])
        assert.ok(loader.includes(label),label);
    assert.match(loader,/if\(creditBuild\)creditBuild\.textContent=buildLabel/);
    assert.match(html,/#qt-screen canvas\{touch-action:none/);
});

test("Feed and profile allow only drag-triggered top refresh; empty state scrolls after header",()=>{
    const feed=read("ui/components/FeedSurface.qml");
    const profile=read("ui/components/ProfileSurface.qml");
    const empty=read("ui/components/EmptyState.qml");
    for(const source of [feed,profile]) {
        assert.match(source,/boundsBehavior: Flickable\.DragOverBounds/);
        assert.match(source,/boundsMovement: Flickable\.FollowBoundsBehavior/);
        assert.match(source,/PullToRefresh \{/);
    }
    const footer=feed.split("footer: Item {")[1].split("id: newPostsBanner")[0];
    assert.match(footer,/id: feedEmptyState/);
    assert.match(footer,/height: stableRows\.count === 0/);
    assert.match(footer,/anchors\.fill: parent/);
    assert.match(feed,/headerPositioning: root\.compact \|\| root\.activeFeed === "channel"/);
    assert.equal((feed.match(/\bEmptyState \{/g)||[]).length,1);
    assert.match(empty,/implicitHeight: stateContent\.implicitHeight/);
    assert.doesNotMatch(profile,/verticalOvershoot/);
});

import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/mobile/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const read = filename => readFileSync(new URL("../../"+filename,import.meta.url),"utf8");
test("Beta 1.0 is the public identity across the web shell and Qt headers",()=>{
    const cmake=read("CMakeLists.txt");
    const pack=read("tools/package_wasm_assets.py");
    const html=read("ui/wasm/genycaster-shell.html");
    const manifest=JSON.parse(read("ui/wasm/genycaster.webmanifest"));
    assert.match(cmake,/VERSION 1\.0\.0/);
    assert.equal(manifest.name,"Genycaster Beta 1.0");
    assert.match(html,/<title>Genycaster Beta 1\.0/);
    assert.match(html,/class="boot-beta">Beta 1\.0/);
    assert.match(read("ui/components/AppHeader.qml"),/genycaster\.release\.beta_1_0/);
    assert.match(read("ui/components/SideNavigation.qml"),/genycaster\.release\.beta_1_0/);
    assert.match(read("ui/components/InitialSetup.qml"),/genycaster\.release\.beta_1_0/);
    assert.match(pack,/args\.release_label/);
    assert.match(read("ui/CMakeLists.txt"),/--release-label "Beta 1\.0"/);
});

test("iOS safe-area background and PWA metadata use the app appearance",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const js=read("ui/wasm/genycaster-loader.js");
    const cpp=readGenycasterMain();
    const manifest=JSON.parse(read("ui/wasm/genycaster.webmanifest"));
    assert.match(html,/viewport-fit=cover/);
    assert.match(html,/<meta name="theme-color" content="#0b0c0f">/);
    assert.match(html,/apple-mobile-web-app-status-bar-style/);
    assert.match(html,/background:var\(--app-chrome\)/);
    assert.equal(manifest.theme_color,"#0b0c0f");
    assert.match(js,/__genycasterSetBrowserChromeTheme=applyBrowserChromeTheme/);
    assert.match(cpp,/updateBrowserChromeTheme/);
    assert.match(cpp,/AppearanceController::appearanceChanged/);
    assert.match(cpp,/#if defined\(__EMSCRIPTEN__\)/);
});

test("phone Home is an uncluttered hub, not duplicate navigation cards",()=>{
    const home=read("ui/components/HomeDashboard.qml");
    assert.match(home,/component HubWorkspace: ColumnLayout/);
    assert.match(home,/columns: 2/);
    assert.match(home,/"homeLocked_" \+ modelData.key/);
    assert.match(home,/key: "wallet"/);
    assert.match(home,/columns: 3/);
    assert.match(home,/"homeShortcut_" \+ modelData.destination/);
    assert.match(home,/destination: "explorer"/);
    assert.match(home,/destination: "search"/);
    assert.match(home,/destination: "apps"/);
    assert.match(home,/destination: "trends"/);
    assert.doesNotMatch(home,/component ActionTile|homeRecentCastPreview|CastIdeaSection/);
    assert.match(home,/Layout.leftMargin: Theme.space3/);
});

test("Boot Beta badge and footer remain in separate non-overlapping rows",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const loader=read("ui/wasm/genycaster-loader.js");
    assert.match(html,/<sup class="boot-beta">Beta 1\.0<\/sup>/);
    assert.doesNotMatch(html,/id="boot-note"/);
    assert.doesNotMatch(html,/id="boot-version"/);
    assert.match(html,/class="boot-credit"><div>Powered by <strong>Genyleap<\/strong> · WASM Engine<\/div><div class="boot-build" id="boot-build-credit"/);
    assert.match(html,/\.boot-credit\{[^}]*flex-direction:column/);
    assert.match(html,/\.boot-beta\{[^}]*transform:translateY/);
    for(const label of ["Preparing Genycaster…","Starting Genycaster…","Opening Genycaster…"])
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
    assert.match(feed,/headerPositioning: root\.compact \|\| stableRows\.count === 0/);
    assert.equal((feed.match(/\bEmptyState \{/g)||[]).length,1);
    assert.match(empty,/implicitHeight: stateContent\.implicitHeight/);
    assert.doesNotMatch(profile,/verticalOvershoot/);
});

import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

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
    const cpp=read("ui/mobile/main.cpp");
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

test("phone home screen is a genuine two-column grid, not three huge stacked cards",()=>{
    const home=read("ui/components/HomeDashboard.qml");
    const mobile=home.split("Flickable {\n        id: mobileFlick\n        visible: root.compact")[1];
    assert.ok(mobile,"mobile home surface must exist");
    const tileGrid=mobile.split("GridLayout {")[1].split("Superellipse {\n                Layout.fillWidth")[0];
    assert.match(tileGrid,/columns: 2/);
    assert.equal((tileGrid.match(/ActionTile \{/g)||[]).length,4);
    for(const route of ["exploreRequested","composeRequested","appsRequested","searchRequested"])
        assert.match(tileGrid,new RegExp("root\\."+route+"\\("));
    assert.match(home,/Bare glyph: no colored square/);
    assert.doesNotMatch(home.split("component ActionTile:")[1].split("MouseArea {")[0],
        /fillColor: Theme\.primarySurface/);
    assert.match(mobile,/Layout\.leftMargin: Theme\.space3/);
});


test("Boot Beta badge and footer remain in separate non-overlapping rows",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const loader=read("ui/wasm/genycaster-loader.js");
    assert.match(html,/<sup class="boot-beta">Beta 1\.0<\/sup>/);
    assert.match(html,/class="boot-note" id="boot-note"/);
    assert.doesNotMatch(html,/id="boot-version"/);
    assert.match(html,/class="boot-credit"><div>Powered by <strong>Genyleap<\/strong> · WASM Engine<\/div><div class="boot-build" id="boot-build-credit"/);
    assert.match(html,/\.boot-credit\{[^}]*flex-direction:column/);
    assert.match(html,/\.boot-beta\{[^}]*transform:translateY/);
    for(const label of ["Explore conversations","Connect with communities","Create and share on Farcaster"])
        assert.ok(loader.includes(label),label);
    assert.match(loader,/if\(creditBuild\)creditBuild\.textContent=buildLabel/);
    assert.match(html,/#qt-screen canvas\{touch-action:none/);
});

test("Feed and profile do not rubber-band beyond their content; empty state scrolls after header",()=>{
    const feed=read("ui/components/FeedSurface.qml");
    const profile=read("ui/components/ProfileSurface.qml");
    const empty=read("ui/components/EmptyState.qml");
    for(const source of [feed,profile]) {
        assert.match(source,/boundsBehavior: Flickable\.StopAtBounds/);
        assert.match(source,/boundsMovement: Flickable\.StopAtBounds/);
        assert.doesNotMatch(source,/Flickable\.DragOverBounds/);
        assert.doesNotMatch(source,/Flickable\.FollowBoundsBehavior/);
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

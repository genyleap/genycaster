import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime/bindings.cpp', import.meta.url), 'utf8');
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

test("native download chip stays collapsible and architecture popup owns every row",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const css=read("ui/wasm/native-downloads.css");
    const js=read("ui/wasm/native-downloads.js");
    assert.match(html,/id="native-downloads-dock" data-expanded="false"/);
    assert.match(html,/id="native-downloads-dock-toggle"/);
    assert.match(html,/id="native-downloads-trigger"[^>]*aria-haspopup="dialog"/);
    assert.match(js,/dockToggle\?\.addEventListener\("click"/);
    assert.match(js,/trigger\.hidden = !expanded/);
    assert.match(css,/#native-downloads-dock \{/);
    assert.match(css,/#native-downloads-dock #native-downloads-trigger\[hidden\]/);
    assert.match(css,/#native-downloads-dialog > #native-downloads-options \{/);
    assert.match(css,/background:var\(--nd-raised\)!important/);
    assert.match(js,/naturalHeight = Math\.min\(240, 12 \+ rows\.length \* 47\)/);
    assert.doesNotMatch(js,/\["#native-downloads-options", 19\]/);
    assert.doesNotMatch(js,/mountShape\(option, 17\)/);
});

test("download dock stays at top in a continuous Superellipse away from floating actions",()=>{
    const css=read("ui/wasm/native-downloads.css");
    const js=read("ui/wasm/native-downloads.js");
    const html=read("ui/wasm/genycaster-shell.html");
    for(const part of [
      "#native-downloads-dock.nd-squircle",
      "top:calc(86px + env(safe-area-inset-top,0px))",
      "bottom:auto",
      ".nd-popup-outline path"
    ]) assert.ok(css.includes(part),part);
    for(const part of [
      '["#native-downloads-dock", 22]',
      '["#native-downloads-dock-toggle", 17]',
      'menuOutlinePath.setAttribute("d",',
      'superellipseClipPolygon(triggerBox.width, panelHeight, 20, false)'
    ]) assert.ok(js.includes(part),part);
    assert.ok(html.includes('id="native-downloads-dock-toggle"'));
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
    assert.match(cpp,/AppearanceController::effectiveDarkChanged/);
    assert.match(cpp,/#if defined\(Q_OS_WASM\)/);
});

test("phone Home keeps four live quick-access destinations without an inert wallet tile",()=>{
 const home=read("ui/components/home/HomeDashboard.qml");
 assert.match(home,/component HubWorkspace: ColumnLayout/);
 assert.match(home,/columns: 2/);
 assert.match(home,/"homeShortcut_" \+ modelData.destination/);
 for (const destination of ["explorer","channels","apps","pro"])
   assert.match(home,new RegExp('destination: "'+destination+'"'));
 assert.doesNotMatch(home,/destination: "wallet"|destination: "trends"|key: "agent"|key: "studio"/);
 assert.match(home,/HomeActivityCard \{/);
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
    const feed=read("ui/components/feed/FeedSurface.qml");
    const profile=read("ui/components/profile/ProfileSurface.qml");
    const empty=read("ui/components/controls/EmptyState.qml");
    for(const source of [feed,profile]) {
        assert.match(source,/boundsBehavior: Flickable\.DragOverBounds/);
        assert.match(source,/boundsMovement: Flickable\.FollowBoundsBehavior/);
        assert.match(source,/PullToRefresh \{/);
    }
    const footer=feed.split("id: feedFooterComponent")[1].split("id: newPostsBanner")[0];
    assert.match(footer,/id: feedEmptyState/);
    assert.match(footer,/height: stableRows\.count === 0/);
    assert.match(footer,/anchors\.fill: parent/);
    assert.match(feed,/headerPositioning: ListView\.InlineHeader/);
    assert.equal((feed.match(/\bEmptyState \{/g)||[]).length,1);
    assert.match(empty,/implicitHeight: stateContent\.implicitHeight/);
    assert.doesNotMatch(profile,/verticalOvershoot/);
});

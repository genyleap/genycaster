import test from "node:test";
import assert from "node:assert/strict";
import vm from "node:vm";
import { readFileSync } from "node:fs";

const read = path => readFileSync(new URL("../../" + path, import.meta.url), "utf8");
const feed = read("ui/components/feed/FeedSurface.qml");
const shell = read("ui/components/shell/AppShell.qml");
const detail = read("ui/components/feed/DetailSurface.qml");
const cast = read("ui/components/feed/CastItem.qml");
const menu = read("ui/components/feed/CastActionsMenu.qml");
const cmake = read("ui/CMakeLists.txt");

function extractFunction(source, name) {
    const start = source.indexOf("    function " + name + "(");
    assert.ok(start >= 0, "missing " + name);
    const opening = source.indexOf("{", start);
    let depth = 0;
    for (let i = opening; i < source.length; ++i) {
        if (source[i] === "{") ++depth;
        else if (source[i] === "}" && --depth === 0)
            return source.slice(start, i + 1).trim();
    }
    throw new Error("unbalanced function " + name);
}

test("conversation header exposes the same lazy, accessible Cast menu as feed cards", () => {
    assert.match(cmake, /components\/feed\/CastActionsMenu\.qml/);
    assert.match(cast, /CastActionsMenu \{/);
    assert.match(detail, /objectName: "detailCastMoreButton"/);
    assert.match(detail, /iconGlyph: Theme\.iconMore/);
    assert.match(detail, /text: qsTrId\("genycaster\.cast\.more_options"\)/);
    for (const locale of ["ar_SA","de_DE","en_US","es_ES","fa_IR","fr_FR",
                          "hi_IN","ja_JP","ko_KR","pt_BR","ru_RU","zh_CN"])
        assert.match(read("ui/i18n/qml_" + locale + ".ts"),
                     /id="genycaster\.cast\.more_options"/);
    assert.match(detail, /onClicked: root\.openDetailMoreMenu\(detailMoreButton\)/);
    assert.match(detail, /detailCastMenuLoader\.item\.openAt\(anchor, root\.contentRtl\)/);
    assert.match(detail, /showConversationAction: false/);
    assert.match(cast, /castMenuLoader\.item\.openAt\(moreButton, root\.contentRtl\)/);
    assert.match(menu, /objectName: "castMenu"/);
    assert.match(menu, /visible: root\.showConversationAction && Boolean\(root\.castData\.castId\)/);
    for (const action of ["open_author_profile","copy_text","copy_link","copy_cast_id","delete"])
        assert.ok(menu.includes("genycaster.cast." + action), action);
    assert.match(menu, /readonly property bool canDelete: Number\(root\.castData\.authorFid/);
    assert.match(menu, /readonly property bool canDeleteNow: root\.canDelete && root\.interactionsAvailable/);
    assert.match(detail, /onRemoveRequested: cast => root\.removeCastRequested\(cast\)/);
});

test("the hidden Explorer does not change into a profile model or remove its header", () => {
    assert.match(shell, /socialMode: presentationBridge\.activeView === "followers"\s*\|\| presentationBridge\.activeView === "following"/);
    assert.match(shell, /headerContent: presentationBridge\.activeView === "followers"\s*\|\| presentationBridge\.activeView === "following" \? null/);
    assert.match(shell, /active: surfaceRequested \|\| item !== null/);
    assert.match(shell, /onBackRequested: root\.returnFromCastDetail\(\)/);
    assert.doesNotMatch(shell, /socialMode: presentationBridge\.activeView !== "feed"/);
});

test("Back restores selected Cast identity and its original viewport offset", () => {
    const rows = [{stableKey:"cast:0xaaa"}, {stableKey:"cast:0xbbb"}, {stableKey:"cast:0xccc"}];
    let currentItem = {y:1250};
    const stableRows = {count:rows.length,get:index=>rows[index],
        indexForKey:(key,ignoreCase=false)=>rows.findIndex(row=>ignoreCase
            ? row.stableKey.toLowerCase()===key.toLowerCase() : row.stableKey===key)};
    const readList = {
        contentY:1150,
        forceLayout(){},
        itemAtIndex(index){return index===1?currentItem:null;},
        positionViewAtIndex(){throw new Error("retained item must not snap to top");}
    };
    const castReturnRestoreTimer = {restart(){}};
    const ListView = {Beginning:0};
    const context = vm.createContext({readList,stableRows,castReturnRestoreTimer,ListView});
    const remember = vm.runInContext("(" + extractFunction(feed,"rememberCastForReturn") + ")",context);
    const restore = vm.runInContext("(" + extractFunction(feed,"restoreCastOnReturn") + ")",context);
    const root = {
        activeFeed:"recent",presentationMode:"for-you",socialMode:false,
        presentationActive:true,applyingViewportRestore:false,
        returnCastKey:"",returnCastScope:"",returnCastOffset:0,
        returnCastY:0,returnCastAttempts:0,returnCastPending:false
    };
    context.root = root;
    remember.call(root,"0xBBB");
    assert.equal(root.returnCastKey,"cast:0xbbb");
    assert.equal(root.returnCastOffset,100);
    assert.equal(root.returnCastPending,true);
    root.presentationActive=false;
    readList.contentY=0; // offscreen layout changed while viewing a thread
    currentItem={y:1550};
    restore.call(root);
    assert.equal(readList.contentY,0,"never modify an inactive viewport");
    root.presentationActive=true;
    restore.call(root);
    assert.equal(readList.contentY,1450);
    assert.equal(currentItem.y-readList.contentY,100);
    assert.equal(root.returnCastPending,false);
    assert.equal(root.applyingViewportRestore,false);
});

test("an explicit feed/category change does not restore a Cast from a stale scope", () => {
    const readList={contentY:400,forceLayout(){},itemAtIndex(){return null}};
    const stableRows={count:1,get(){return {stableKey:"cast:0xaaa"}}};
    const context=vm.createContext({readList,stableRows,castReturnRestoreTimer:{restart(){}},ListView:{Beginning:0}});
    const restore=vm.runInContext("(" + extractFunction(feed,"restoreCastOnReturn") + ")",context);
    const root={
        activeFeed:"following",presentationMode:"for-you",socialMode:false,
        presentationActive:true,returnCastKey:"cast:0xaaa",
        returnCastScope:"recent:for-you:false",returnCastPending:true
    };
    context.root = root;
    restore.call(root);
    assert.equal(readList.contentY,400);
    assert.equal(root.returnCastPending,false);
    assert.match(feed,/onOpenRequested: castId => \{\s*root\.rememberCastForReturn\(castId\)\s*root\.castRequested\(castId\)/);
    assert.match(feed,/function scrollToBeginning\(\) \{\s*root\.returnCastPending = false/);
});

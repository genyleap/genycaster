import test from "node:test";
import assert from "node:assert/strict";
import vm from "node:vm";
import {readFileSync} from "node:fs";

const read=p=>readFileSync(new URL("../../"+p,import.meta.url),"utf8");

test("reply and quote have distinct publication targets and taxonomy policies",()=>{
    const composer=read("ui/components/ComposerDrawer.qml");
    const app=read("ui/components/AppShell.qml");
    assert.match(composer,/readonly property string composeIntent: root\.quotedCast !== null \? "quote"/);
    assert.match(composer,/: root\.parentCast !== null \? "reply" : "new"/);
    assert.match(composer,/readonly property bool replyMode: composeIntent === "reply"/);
    assert.match(composer,/readonly property bool quoteMode: composeIntent === "quote"/);
    assert.match(composer,/readonly property bool showTaxonomyControls: composeIntent !== "reply"/);
    assert.match(composer,/objectName: "mobileCastContext"[\s\S]*?visible: root\.compact && root\.editorMode === "visual"\s+&& root\.showTaxonomyControls/);
    assert.match(composer,/if \(!root\.opened \|\| !channelPicker\.opened \|\| !root\.showTaxonomyControls\) return/);
    assert.match(composer,/onClicked: if \(root\.showTaxonomyControls\) channelPicker\.open\(\)/);
    assert.match(composer,/else if \(root\.replyMode\)[\s\S]*?root\.replyRequested/);
    assert.match(composer,/root\.showTaxonomyControls \? root\.selectedParentUrl : ""/);
    assert.match(app,/function openQuoteComposer\(cast\)/);
    assert.match(app,/composerDrawer\.quotedCast = initialAction === "quote" \? \(quoteCast \|\| null\) : null/);
    assert.match(app,/presentationBridge\.publishPreparedReply\(/);
    assert.match(app,/presentationBridge\.publishPreparedCast\(/);
    assert.match(app,/composerDrawer\.parentCast = root\.composerParent\s+\/\/ An earlier Quote[\s\S]*?composerDrawer\.quotedCast = null/);
    assert.match(app,/onAbandoned:[\s\S]*?composerDrawer\.quotedCast = null/);
});

test("iPhone reply composer keeps context above the editor with the keyboard",()=>{
    const composer=read("ui/components/ComposerDrawer.qml");
    assert.match(composer,/fullScreen: root\.compact/);
    assert.match(composer,/height: parent \? \(root\.compact \? parent\.height/);
    assert.match(composer,/Qt\.inputMethod\.visible/);
    assert.match(composer,/root\.parent\.height < root\.expandedViewportHeight - 125/);
    assert.match(composer,/visible: root\.replyMode \|\| root\.quoteMode/);
    assert.match(composer,/implicitHeight: replyContextContent\.implicitHeight[\s\S]*?root\.compact && root\.keyboardVisible/);
    assert.match(composer,/maximumLineCount: root\.compact && root\.keyboardVisible \? 1/);
    assert.match(composer,/root\.compact && root\.replyMode && !root\.threadMode[\s\S]*?flick\.contentY = 0/);
    assert.match(composer,/onOpened: \{[\s\S]*?composerScroll\.contentItem\.contentY = 0/);
    assert.match(composer,/root\.replyMode[\s\S]*?Math\.max\(118, Math\.min\(192, composerScroll\.height \* 0\.36\)\)/);
    assert.match(composer,/visible: !root\.compact \|\| !root\.keyboardVisible/);
});

test("clearing a quote draft retains its referenced Cast",()=>{
    const composer=read("ui/components/ComposerDrawer.qml");
    const clear=composer.slice(composer.indexOf("function clearAllComposerContent()"),
                               composer.indexOf("function startNextMediaUpload()"));
    assert.match(clear,/richComposer\.newDocument\(\)/);
    assert.match(clear,/if \(root\.quoteMode && root\.quotedCast\)\s+richComposer\.insertCastReference/);
    assert.doesNotMatch(clear,/root\.quotedCast = null/);
    assert.match(composer,/if \(root\.initialAction === "quote" && root\.quotedCast\)/);
});

test("browser status chrome starts in previous appearance or OS appearance",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const inline=html.match(/<script>\s*(\(function \(\) \{[\s\S]*?\}\)\(\);)\s*<\/script>/);
    assert.ok(inline);
    for (const [stored,osDark,expected] of [
        ["light",true,"light"],["dark",false,"dark"],
        [null,true,"dark"],[null,false,"light"]
    ]) {
        const colors=[];
        const state={scheme:""};
        const scope={
            window:{matchMedia:()=>({matches:osDark})},
            localStorage:{getItem:()=>stored},
            document:{
                documentElement:{setAttribute:(_,value)=>{state.scheme=value;}},
                querySelector:()=>({setAttribute:(_,value)=>{colors.push(value);}})
            }
        };
        vm.runInNewContext(inline[1],scope);
        assert.equal(state.scheme,expected);
        assert.equal(colors.at(-1),expected==="light"?"#f1f1f3":"#0b0c0f");
    }
    const loader=read("ui/wasm/genycaster-loader.js");
    assert.match(loader,/genycaster\.browser\.chrome\.v1/);
    assert.match(loader,/getAttribute\?\.\("data-chrome-theme"\) !== "light", false/);
    assert.match(html,/#ios-status-chrome\{[^}]*background-color:var\(--app-chrome\)/);
});

import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

const read = name => readFileSync(new URL("../../"+name,import.meta.url),"utf8");

test("Like uses a consistent semantic heart with red selection in every Cast",()=>{
    const bar=read("ui/components/EngagementBar.qml");
    assert.match(bar,/readonly property string displayedLikeIcon: Theme\.iconHeart/);
    assert.match(bar,/iconGlyph: root\.displayedLikeIcon/);
    assert.match(bar,/iconFilled: root\.effectiveLikedByMe/);
    assert.match(bar,/activeColor: Theme\.negative/);
    assert.match(bar,/activeColor: Theme\.positive/);
    assert.doesNotMatch(bar,/contextualLikeIcon: presentationBridge\.contextualReactionIcon/);
});

test("Home greeting displays authenticated account avatar and DNA for guests",()=>{
    const home=read("ui/components/HomeDashboard.qml");
    const shell=read("ui/components/AppShell.qml");
    assert.match(shell,/avatarUrl: presentationBridge\.accountAvatarUrl/);
    assert.match(shell,/authenticated: presentationBridge\.isAuthenticated/);
    assert.match(home,/avatarSize: 58/);
    assert.match(home,/avatarSize: 52/);
    assert.equal((home.match(/sourceUrl: root\.avatarUrl/g)||[]).length,2);
    assert.equal((home.match(/visible: root\.authenticated && root\.avatarUrl\.length > 0/g)||[]).length,2);
    assert.equal((home.match(/visible: !root\.authenticated \|\| root\.avatarUrl\.length === 0/g)||[]).length,2);
});

test("Mini App artwork supports manifest aliases and a visible offline fallback",()=>{
    const catalog=read("ui/mobile/miniapp_catalog_controller.cpp");
    for(const key of ["iconUrl","icon","logoUrl","logo","thumbnailUrl","imageUrl","splashImageUrl"])
        assert.ok(catalog.includes('QStringLiteral("'+key+'")'),key);
    assert.match(catalog,/parsedHome\.resolved\(candidate\)/);
    assert.match(catalog,/publicImageSource\(iconUrl\)/);
    const apps=read("ui/components/AppsSurface.qml");
    assert.match(apps,/id: miniAppArtwork/);
    assert.match(apps,/forceAutomaticDownload: true/);
    assert.match(apps,/visible: !miniAppArtwork\.ready/);
    assert.match(apps,/text: Theme\.iconApps/);
});

test("Welcome has no orbital animation, previews official mentions, and localizes step count",()=>{
    const selector=read("ui/components/SetupLanguageSelector.qml");
    const setup=read("ui/components/InitialSetup.qml");
    assert.doesNotMatch(selector,/id: orbit|NumberAnimation on rotation|Animation\.Infinite/);
    assert.match(selector,/@genyleap/);
    assert.match(selector,/@compez\.eth/);
    assert.doesNotMatch(selector,/"@geny"/);
    assert.match(setup,/LocaleFormat\.formatInteger\(root\.wizardSteps\.indexOf\(root\.currentStep\) \+ 1\)/);
    assert.match(setup,/LocaleFormat\.formatInteger\(root\.wizardSteps\.length\)/);
});

test("Browser tab and installed app icon use actual brand DNA and a clean version title",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const icon=read("ui/wasm/genycaster-icon.svg");
    const manifest=JSON.parse(read("ui/wasm/genycaster.webmanifest"));
    assert.match(html,/<title>Genycaster Beta 1\.0<\/title>/);
    assert.doesNotMatch(html,/Secure social client/);
    assert.equal(manifest.description,"Genycaster Beta 1.0");
    assert.match(icon,/viewBox="0 0 192 192"/);
    assert.match(icon,/translate\(24 24\) scale\(6\)/);
    assert.match(icon,/fill="#ffffff"/);
    const touch=readFileSync(new URL("../../ui/wasm/apple-touch-icon.png",import.meta.url));
    const touchPre=readFileSync(new URL("../../ui/wasm/apple-touch-icon-precomposed.png",import.meta.url));
    assert.ok(touch.equals(touchPre));
    assert.equal(touch.subarray(1,4).toString(),"PNG");
    assert.match(read("ui/wasm/genycaster-sw.js"),/genycaster-static-v5/);
});

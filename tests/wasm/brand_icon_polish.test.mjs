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
    assert.match(home,/avatarSize: 62/);
    assert.match(home,/avatarSize: root\.androidCompact[\s\S]*?root\.densePhone \? 56 : 76/);
    assert.equal((home.match(/sourceUrl: root\.avatarUrl/g)||[]).length,2);
    assert.equal((home.match(/visible: root\.authenticated && root\.avatarUrl\.length > 0/g)||[]).length,2);
    assert.equal((home.match(/visible: !root\.authenticated \|\| root\.avatarUrl\.length === 0/g)||[]).length,2);
});

test("Mini App artwork supports manifest aliases and a visible offline fallback",()=>{
    const catalog=read("ui/mobile/miniapp_catalog_controller.cpp");
    for(const key of ["iconUrl","icon","logoUrl","logo","thumbnailUrl","imageUrl","splashImageUrl"])
        assert.ok(catalog.includes('QStringLiteral("'+key+'")'),key);
    assert.match(catalog,/parsedHome\.resolved\(candidate\)/);
    assert.match(catalog,/publicMiniAppImageSource\(iconUrl, homeUrl\)/);
    const apps=read("ui/components/AppsSurface.qml");
    assert.match(apps,/id: storeIcon/);
    assert.match(apps,/forceAutomaticDownload: true/);
    assert.match(apps,/MiniAppArtPlaceholder \{/);
    const placeholder=read("ui/components/MiniAppArtPlaceholder.qml");
    assert.match(placeholder,/visible: !root\.hasSource \|\| root\.loadFailed/);
    assert.match(placeholder,/text: Theme\.iconApps/);
});

test("Welcome uses a focused language picker without promotional preview, and localizes step count",()=>{
    const selector=read("ui/components/SetupLanguageSelector.qml");
    const setup=read("ui/components/InitialSetup.qml");
    assert.doesNotMatch(selector,/id: orbit|NumberAnimation on rotation|Animation\.Infinite/);
    assert.match(selector,/objectName: "setupLanguageWheel"/);
    assert.doesNotMatch(selector,/@genyleap|@compez\.eth|id: previewTab/);
    assert.match(setup,/wizardSteps: \[0,1,2,3,4,5,6,7\]/);
    assert.match(setup,/sourceComponent: root\.currentStep === 0 \? welcomeStage/);
});

test("Browser tab and installed app icon use actual brand DNA and a clean version title",()=>{
    const html=read("ui/wasm/genycaster-shell.html");
    const icon=read("ui/wasm/genycaster-icon.svg");
    const manifest=JSON.parse(read("ui/wasm/genycaster.webmanifest"));
    assert.match(html,/<title>Genycaster Beta 1\.0<\/title>/);
    assert.doesNotMatch(html,/Secure social client/);
    assert.equal(manifest.description,"Genycaster Beta 1.0");
    assert.match(icon,/viewBox="48 48 416 416"/);
    assert.ok(html.includes("genycaster-icon.svg?v=brand-6"));
    assert.match(icon,/GenyCaster icon/);
    assert.match(icon,/fill="#FFFFFF"/);
    assert.match(html,/genycaster-wordmark-dark\.svg/);
    assert.match(html,/genycaster-boot-icon\.svg/);
    const wordmark=read("ui/wasm/genycaster-wordmark-dark.svg");
    assert.match(wordmark,/fill="#FFFFFF"/);
    assert.match(wordmark,/fill="#B5B5B5"/);
    for(const asset of ["favicon.ico","favicon-16x16.png","favicon-32x32.png",
        "favicon-48x48.png","genycaster-192.png","genycaster-512.png"]){
        assert.match(html+read("ui/wasm/genycaster-sw.js")+read("ci/package_wasm_assets.py"),
            new RegExp(asset.replace(".","\\.")));
        const bytes=readFileSync(new URL("../../ui/wasm/"+asset,import.meta.url));
        assert.ok(bytes.length>100,asset);
        if(asset.endsWith(".png")){
            assert.equal(bytes.subarray(1,4).toString(),"PNG");
            const size=Number((asset.match(/(16|32|48|192|512)/)||[])[0]);
            assert.equal(bytes.readUInt32BE(16),size);
            assert.equal(bytes.readUInt32BE(20),size);
        }else{
            assert.equal(bytes.readUInt16LE(0),0);
            assert.equal(bytes.readUInt16LE(2),1);
        }
    }
    assert.equal(manifest.icons.filter(i=>i.type==="image/png").length,2);
    const touch=readFileSync(new URL("../../ui/wasm/apple-touch-icon.png",import.meta.url));
    const touchPre=readFileSync(new URL("../../ui/wasm/apple-touch-icon-precomposed.png",import.meta.url));
    assert.ok(touch.equals(touchPre));
    assert.equal(touch.subarray(1,4).toString(),"PNG");
    assert.match(read("ui/wasm/genycaster-sw.js"),/genycaster-static-v11/);
});

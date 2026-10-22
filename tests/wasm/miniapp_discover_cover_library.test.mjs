import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

const read = file => readFileSync(new URL("../../"+file,import.meta.url),"utf8");

test("Neynar manifest cover is parsed separately from square app icon",()=>{
    const cpp=read("ui/mobile/miniapp_catalog_controller.cpp");
    assert.match(cpp,/miniAppManifestObject\(row\)/);
    assert.match(cpp,/manifest\.value\(QStringLiteral\("frame"\)\)\.toObject\(\), coverKeys/);
    for(const key of ["hero_image_url","og_image_url","splash_image_url"])
        assert.ok(cpp.includes('QStringLiteral("'+key+'")'),key);
    assert.match(cpp,/app\.value\(QStringLiteral\("screenshot_urls"\)\)\.toArray\(\)/);
    assert.match(cpp,/\{QStringLiteral\("coverSource"\), publicImageSource\(coverUrl\)\}/);
    assert.match(cpp,/\{QStringLiteral\("iconSource"\), publicImageSource\(iconUrl\)\}/);
    assert.match(cpp,/const QUrl candidate\{coverUrl, QUrl::StrictMode\}/);
    assert.match(cpp,/parsedHome\.resolved\(candidate\)/);
    assert.match(cpp,/url\.scheme\(\) != QStringLiteral\("https"\)/);
});

test("Discover cards use true covers without stretching icons as hero art",()=>{
    const view=read("ui/components/AppsSurface.qml");
    const hero=read("ui/components/MiniAppFeaturedCarousel.qml");
    assert.match(view,/const illustrated = \[\]/);
    assert.match(view,/String\(app\.coverSource \|\| ""\)/);
    assert.match(view,/heading: qsTr\("Discover"\)/);
    assert.match(view,/objectName: "miniAppLibraryShelves"/);
    assert.match(view,/miniAppCatalog\.recordRecentUse\(app\)/);
    assert.match(hero,/source: String\(featuredCell\.app\.coverSource \|\| ""\)/);
    assert.match(hero,/source: String\(featuredCell\.app\.iconSource \|\| ""\)/);
    assert.match(hero,/clipToSuperellipse: false/);
    assert.match(hero,/GradientStop \{ position: 1\.0/);
    assert.match(hero,/signal openRequested\(var app\)/);
    assert.match(hero,/signal saveRequested\(var app\)/);
    assert.doesNotMatch(hero,/https?:\/\/[^"]+/);
});

test("recent apps only represent real launch history and remain bounded",()=>{
    const cpp=read("ui/mobile/miniapp_catalog_controller.cpp");
    const hpp=read("ui/mobile/miniapp_catalog_controller.hpp");
    const view=read("ui/components/AppsSurface.qml");
    assert.match(hpp,/Q_PROPERTY\(QVariantList recentApps READ recentApps NOTIFY recentAppsChanged FINAL\)/);
    assert.match(hpp,/Q_INVOKABLE void recordRecentUse\(const QVariantMap& app\)/);
    assert.match(cpp,/recentMiniAppLimit = 20/);
    assert.match(cpp,/loadRecent\(\)/);
    assert.match(cpp,/recentApps\.prepend\(std::move\(row\)\)/);
    assert.match(cpp,/settings\.setValue\(QString::fromLatin1\(recentMiniAppsSettingsKey\), recentApps\)/);
    assert.match(view,/miniAppCatalog\.recordRecentUse\(app\)/);
    const first=view.indexOf("miniAppCatalog.recordRecentUse(app)");
    assert.ok(first>view.indexOf("if (Number(presentationBridge.accountFid"));
    assert.match(view,/recentApps: miniAppCatalog\.recentApps/);
    assert.match(view,/savedApps: miniAppCatalog\.myApps/);
});

test("mobile App library uses thumbnail shelf and Saved grid; desktop remains intact",()=>{
    const view=read("ui/components/AppsSurface.qml");
    const library=read("ui/components/MiniAppLibraryShelves.qml");
    const cmake=read("ui/CMakeLists.txt");
    assert.match(view,/visible: root\.compact && root\.farcasterSection === "my"/);
    assert.match(view,/visible: !root\.compact \|\| root\.farcasterSection !== "my"/);
    assert.match(view,/visible: root\.compact && root\.section === "farcaster"/);
    assert.match(view,/Layout\.preferredHeight: root\.compact \? 48 : 64/);
    assert.match(library,/text: qsTr\("Recently used"\)/);
    assert.match(library,/orientation: ListView\.Horizontal/);
    assert.match(library,/root\.recentApps && root\.recentApps\.length > 0/);
    assert.match(library,/text: qsTrId\("genycaster\.apps\.saved"\)/);
    assert.match(library,/columns: root\.savedColumns/);
    assert.match(library,/model: root\.savedApps \|\| \[\]/);
    assert.match(library,/forceAutomaticDownload: true/);
    for(const component of ["MiniAppLibraryShelves","MiniAppFeaturedCarousel"])
        assert.ok(cmake.includes("components/"+component+".qml"));
});

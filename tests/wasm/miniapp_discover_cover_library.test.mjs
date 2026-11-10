import { componentPath } from './ui_component_paths.mjs';
import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";

const read = file => readFileSync(new URL("../../"+file,import.meta.url),"utf8");

test("Neynar manifest cover is parsed separately from square app icon",()=>{
    const cpp=read("ui/app/miniapp/catalog.cpp");
    assert.match(cpp,/miniAppManifestObject\(row\)/);
    assert.match(cpp,/manifest\.value\(QStringLiteral\("frame"\)\)\.toObject\(\), coverKeys/);
    for(const key of ["hero_image_url","og_image_url","splash_image_url"])
        assert.ok(cpp.includes('QStringLiteral("'+key+'")'),key);
    assert.match(cpp,/app\.value\(QStringLiteral\("screenshot_urls"\)\)\.toArray\(\)/);
    assert.match(cpp,/\{QStringLiteral\("coverSource"\), publicMiniAppImageSource\(coverUrl, homeUrl\)\}/);
    assert.match(cpp,/\{QStringLiteral\("iconSource"\), publicMiniAppImageSource\(iconUrl, homeUrl\)\}/);
    assert.match(cpp,/const QUrl candidate\{coverUrl, QUrl::StrictMode\}/);
    assert.match(cpp,/parsedHome\.resolved\(candidate\)/);
    assert.match(cpp,/url\.scheme\(\) != QStringLiteral\("https"\)/);
});

test("Discover cards use true covers without stretching icons as hero art",()=>{
    const view=read("ui/components/apps/AppsSurface.qml");
    const hero=read("ui/components/apps/FeaturedCarousel.qml");
    assert.match(view,/const illustrated = \[\]/);
    assert.match(view,/String\(app\.coverSource \|\| ""\)/);
    assert.match(view,/heading: qsTrId\("genycaster.apps.discover"\)/);
    assert.match(view,/objectName: "miniAppLibraryShelves"/);
    assert.match(view,/miniAppCatalog\.recordRecentUse\(root\.pendingLaunchApp\)/);
    assert.match(hero,/source: String\(featuredCell\.app\.coverSource \|\| ""\)/);
    assert.match(hero,/source: String\(featuredCell\.app\.iconSource \|\| ""\)/);
    assert.match(hero,/clipToSuperellipse: true/);
    assert.match(hero,/GradientStop \{ position: 1\.0/);
    assert.match(hero,/signal openRequested\(var app\)/);
    assert.match(hero,/signal saveRequested\(var app\)/);
    assert.doesNotMatch(hero,/https?:\/\/[^"]+/);
});

test("recent apps only represent real launch history and remain bounded",()=>{
    const cpp=read("ui/app/miniapp/catalog.cpp");
    const hpp=read("ui/app/miniapp/catalog.hpp");
    const view=read("ui/components/apps/AppsSurface.qml");
    assert.match(hpp,/Q_PROPERTY\(QVariantList recentApps READ recentApps NOTIFY recentAppsChanged FINAL\)/);
    assert.match(hpp,/Q_INVOKABLE void recordRecentUse\(const QVariantMap& app\)/);
    assert.match(cpp,/recentMiniAppLimit = 20/);
    assert.match(cpp,/loadRecent\(\)/);
    assert.match(cpp,/recentApps\.prepend\(std::move\(row\)\)/);
    assert.match(cpp,/settings\.setValue\(QString::fromLatin1\(recentMiniAppsSettingsKey\), recentApps\)/);
    assert.match(view,/miniAppCatalog\.recordRecentUse\(root\.pendingLaunchApp\)/);
    const first=view.indexOf("miniAppCatalog.recordRecentUse(root.pendingLaunchApp)");
    assert.ok(first>view.indexOf("function onExternalLaunchSucceeded()"));
    assert.match(view,/if \(Number\(presentationBridge\.accountFid \|\| 0\) <= 0\)/);
    assert.match(view,/recentApps: miniAppCatalog\.recentApps/);
    assert.match(view,/savedApps: miniAppCatalog\.myApps/);
});

test("mobile App library uses horizontal Recents and borderless saved rows",()=>{
    const view=read("ui/components/apps/AppsSurface.qml");
    const library=read("ui/components/apps/LibraryShelves.qml");
    const cmake=read("ui/CMakeLists.txt");
    assert.match(view,/visible: root\.compact && root\.farcasterSection === "my"/);
    assert.match(view,/visible: !root\.compact \|\| root\.farcasterSection !== "my"/);
    assert.match(view,/visible: root\.compact && root\.section === "farcaster"/);
    assert.match(view,/Layout\.preferredHeight: root\.compact \? 48 : 48/);
    assert.match(library,/text: qsTrId\("genycaster.apps.recently_used"\)/);
    assert.match(library,/orientation: ListView\.Horizontal/);
    assert.match(library,/root\.recentApps && root\.recentApps\.length > 0/);
    assert.match(library,/text: qsTrId\("genycaster\.apps\.saved"\)/);
    assert.match(library,/columns: root\.savedColumns/);
    assert.match(library,/readonly property int savedColumns: 1/);
    assert.match(library,/objectName: "savedMiniAppOpenButton"/);
    assert.match(library,/model: root\.savedApps \|\| \[\]/);
    assert.match(library,/forceAutomaticDownload: true/);
    for(const component of ["MiniAppLibraryShelves","MiniAppFeaturedCarousel"])
        assert.ok(cmake.includes("components/"+componentPath(component)+".qml"));
});

test("mobile Explorer shows dense rows and explicit Open",()=>{
 const apps=read("ui/components/apps/AppsSurface.qml");
 const library=read("ui/components/apps/LibraryShelves.qml");
 assert.match(apps,/function showAppDetails\(app\)/);
 assert.match(apps,/miniAppDetailDrawer\.open\(\)/);
 assert.match(apps,/id: miniAppStoreTile[\s\S]*?id: storeIcon/);
 assert.match(apps,/targetColumns: root\.compact\s*\? 1/);
 assert.match(apps,/cellHeight: root\.compact\s*\? \(width < Theme\.scaleUi\(540\)\s*\? Theme\.scaleUi\(148\) : Theme\.scaleUi\(118\)\)/);
 assert.match(apps,/id: storeRow[\s\S]*?visible: !root\.compact/);
 assert.match(apps,/objectName: "miniAppDetailDrawer"/);
 assert.match(apps,/root\.showAppDetails\(cardCell\.modelData\)/);
 assert.match(apps,/onOpenRequested: function\(app\) \{ root\.showAppDetails\(app\) \}/);
 assert.match(apps,/miniAppDetailDrawer\.close\(\)\s+root\.openApp\(app\)/);
 assert.match(library,/TapHandler \{ onTapped: root\.openRequested\(savedCell\.app\) \}/);
});

test("Discover excludes entries without a real banner instead of showing empty feature cards",()=>{
 const apps=read("ui/components/apps/AppsSurface.qml");
 assert.match(apps,/return illustrated\.slice\(0, 8\)/);
 assert.doesNotMatch(apps,/illustrated\.concat\(other\)/);
 assert.match(apps,/root\.discoveryShelfApps\.length > 0/);
});
test("mini app image admission is independent of scroll and uses a bounded adjacent cache",()=>{
 const apps=read("ui/components/apps/AppsSurface.qml");
 const hero=read("ui/components/apps/FeaturedCarousel.qml");
 const saved=read("ui/components/apps/LibraryShelves.qml");
 assert.match(apps,/id: storeIcon[\s\S]*?deferNewLoadsWhileScrolling: false[\s\S]*?framePacedPresentation: false/);
 assert.match(saved,/deferNewLoadsWhileScrolling: false/);
 assert.match(hero,/cacheBuffer: root\.cardWidth \* 2 \+ spacing \* 2/);
 assert.match(hero,/loadDelayMs: Math\.min\(160, featuredCell\.index \* 35\)/);
 assert.match(hero,/deferNewLoadsWhileScrolling: false/);
 assert.match(hero,/requestedSourceWidth: Math\.min\(640/);
});
test("mobile browsing modes are slim text tabs, not two large full-width action buttons",()=>{
 const apps=read("ui/components/apps/AppsSurface.qml");
 assert.match(apps,/id: miniAppModeTabs[\s\S]*?Layout\.preferredHeight: 34/);
 assert.match(apps,/delegate: Button \{[\s\S]*?Layout\.preferredHeight: 34/);
 assert.match(apps,/modeTab\.selected \? Theme\.ink : Theme\.inkMuted/);
 assert.match(apps,/Accessible\.role: Accessible\.PageTab/);
 assert.doesNotMatch(apps,/prominent: root\.explorerMode === String\(modelData\.id\)/);
});

test('provider-neutral Mini App paging preserves existing results and rich author metadata',()=>{
 const cpp=read("ui/app/miniapp/catalog.cpp");
 const hpp=read("ui/app/miniapp/catalog.hpp");
 const view=read("ui/components/apps/AppsSurface.qml");
 for(const part of [
    'Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged FINAL)',
    'Q_INVOKABLE void loadMore()'
 ])assert.ok(hpp.includes(part),part);
 assert.match(cpp,/nextCursor = next\.toObject\(\)\.value\(QStringLiteral\("cursor"\)\)\.toString\(\)/);
 assert.match(cpp,/void loadMore\(\)[\s\S]*?parameters\.addQueryItem\(QStringLiteral\("cursor"\), cursor\)/);
 assert.match(cpp,/for \(const QVariant& value : result\.rows\)[\s\S]*?catalogueSnapshot\.push_back/);
 assert.match(view,/onContentYChanged:[\s\S]*?miniAppCatalog\.loadMore\(\)/);
 assert.match(view,/objectName: "miniAppLoadMore"/);
 for(const item of ['authorUsername','authorAvatar','category','tags','screenshots']){
   assert.ok(cpp.includes('{QStringLiteral("'+item+'")'),item);
 }
 assert.match(view,/text: qsTrId\("genycaster.apps.developer"\)/);
 assert.match(view,/text: qsTrId\("genycaster.apps.screenshots"\)/);
 assert.match(view,/model: root\.selectedMiniApp\.screenshots \|\| \[\]/);
 assert.match(view,/id: detailScroll/);
});

test('feature carousel counter follows visible position, arrows and touch use one index',()=>{
 const hero=read("ui/components/apps/FeaturedCarousel.qml");
 assert.match(hero,/property int shownIndex: 0/);
 assert.match(hero,/function navigateTo\(index\)/);
 assert.match(hero,/root\.navigateTo\(root\.shownIndex - 1\)/);
 assert.match(hero,/root\.navigateTo\(root\.shownIndex \+ 1\)/);
 assert.match(hero,/onContentXChanged:[\s\S]*?root\.shownIndex = Math\.max/);
 assert.match(hero,/onDragStarted: slideAnimation\.stop\(\)/);
 assert.match(hero,/property: "contentX"[\s\S]*?Theme\.motionStandard/);
 assert.doesNotMatch(hero,/positionViewAtIndex\(/);
});
test('WASM uses browser-cacheable first-party raster proxy, native still uses image provider',()=>{
 const cpp=read("ui/app/miniapp/catalog.cpp");
 assert.match(cpp,/#if defined\(Q_OS_WASM\)[\s\S]*?genycaster::deployment::browserGateway[\s\S]*?\/v1\/browser\/images/);
 assert.match(cpp,/query\.addQueryItem\(QStringLiteral\("url"\), url\.toString\(QUrl::FullyEncoded\)\)/);
 assert.match(cpp,/#else\s+const QByteArray encoded/);
});

test('failed pagination keeps the cursor for manual retry and prevents automatic request cascades',()=>{
 const cpp=read('ui/app/miniapp/catalog.cpp');
 const apps=read('ui/components/apps/AppsSurface.qml');
 assert.match(cpp,/pendingPageCursor = cursor/);
 assert.match(cpp,/setNextCursor\(pageToken\)/);
 assert.match(cpp,/Tap Load more to retry/);
 assert.match(cpp,/loadingPage = false;\s+pendingPageCursor\.clear\(\);/);
 assert.match(apps,/dragging \|\| flicking/);
 assert.match(apps,/now - root\.lastAutomaticPageMs > 2500/);
 assert.match(apps,/miniAppCatalog\.errorText\.length === 0/);
});
test('manifest-scoped images work for Mini Apps saved before image-proxy upgrade',()=>{
 const cpp=read('ui/app/miniapp/catalog.cpp');
 assert.match(cpp,/publicMiniAppImageSource\(const QString& imageUrl, const QString& homeUrl\)/);
 assert.match(cpp,/query\.addQueryItem\(QStringLiteral\("miniapp"\), homeUrl\)/);
 assert.match(cpp,/const auto restoreScope = \[&homeUrl\]/);
 assert.match(cpp,/QStringLiteral\("image"\)/);
 assert.match(cpp,/genycaster::deployment::browserGateway/);
});

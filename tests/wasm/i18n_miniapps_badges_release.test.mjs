import { componentPath } from './ui_component_paths.mjs';
import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
const read = path => readFileSync(new URL("../../"+path, import.meta.url),"utf8");

const required=[
 "genycaster.apps.all_topics","genycaster.apps.open","genycaster.apps.opening",
 "genycaster.apps.resume","genycaster.apps.load_more","genycaster.apps.category_label",
 "genycaster.apps.recently_used","genycaster.apps.category.social",
 "genycaster.apps.category.games","genycaster.apps.category.finance",
 "genycaster.apps.category.defi","genycaster.apps.category.education",
 "genycaster.cast.source_client_accessible","genycaster.update.available",
 "genycaster.update.no_notes","genycaster.update.current"
];

test("all 12 Qt language catalogs include real translations for the new UI",()=>{
    const locales=readdirSync(new URL("../../ui/i18n/",import.meta.url))
        .filter(name=>name.startsWith("qml_") && name.endsWith(".ts"));
    assert.equal(locales.length,12);
    for(const locale of locales){
        const catalog=read("ui/i18n/"+locale);
        for(const id of required){
            assert.ok(catalog.includes('<message id="'+id+'">'),locale+" missing "+id);
        }
        assert.ok(catalog.includes('<translation>'),locale);
    }
    const fa=read("ui/i18n/qml_fa_IR.ts");
    assert.ok(fa.includes("<translation>دسته‌بندی</translation>"));
    assert.ok(fa.includes("<translation>بازی‌ها</translation>"));
    assert.ok(fa.includes("<translation>همهٔ موضوعات</translation>"));
});

test("Apps category localization preserves canonical provider IDs while translating labels reactively",()=>{
    const apps=read("ui/components/apps/AppsSurface.qml");
    const fmt=read("ui/components/controls/LocaleFormat.qml");
    const saved=read("ui/components/apps/LibraryShelves.qml");
    assert.ok(apps.includes('LocaleFormat.observeRevision()'));
    assert.ok(apps.includes('title: LocaleFormat.formatMiniAppCategory(category)'));
    assert.ok(apps.includes('String(app.category || "").trim() === root.topicFilter'));
    assert.ok(apps.includes('qsTrId("genycaster.apps.all_topics")'));
    assert.ok(fmt.includes("function formatMiniAppCategory(value)"));
    assert.ok(fmt.includes("root.observeRevision()"));
    assert.ok(fmt.includes('qsTrId("genycaster.apps.category.social")'));
    assert.ok(fmt.includes('default: return raw'));
    assert.ok(saved.includes("LocaleFormat.formatMiniAppCategory(savedCell.app.category)"));
    assert.ok(apps.includes("LocaleFormat.formatMiniAppCategory(cardCell.modelData.category)"));
    for (const view of [apps,saved,read("ui/components/apps/FeaturedCarousel.qml")])
        assert.ok(!view.includes('qsTr("'),"unregistered qsTr in "+view.slice(0,30));
});

test("client attribution is evidence-based, localized and shared by normal/reply cast UI",()=>{
    const badge=read("ui/components/feed/SourceClientBadge.qml");
    const cast=read("ui/components/feed/CastItem.qml");
    const reply=read("ui/components/feed/ThreadReplyItem.qml");
    const bindings=read("ui/app/runtime/bindings.cpp");
    const codec=read("src/providers/hypersnap/codec.cpp");
    assert.ok(badge.includes("visible: sanitizedName.length > 0"));
    assert.ok(badge.includes('qsTrId("genycaster.cast.source_client_accessible")'));
    assert.ok(cast.includes('clientName: String(root.modelData.sourceClient || "")'));
    assert.ok(reply.includes('clientName: String(root.reply.sourceClient || "")'));
    assert.ok(bindings.includes('QStringLiteral("sourceClient")'));
    assert.ok(codec.includes('findValue(castObject, "app")'));
    assert.ok(codec.includes('cast.sourceClient = std::move(*name)'));
    assert.ok(!codec.includes('cast.sourceClient = "Farcaster"'));
});

test("Qt live language selection updates the release drawer language across WASM",()=>{
    const main=read("ui/app/main.cpp");
    const loader=read("ui/wasm/genycaster-loader.js");
    const native=read("ui/components/settings/UpdateNoticeDrawer.qml");
    assert.ok(main.includes('window.__genycasterSetUpdateLanguage(tag)'));
    assert.ok(loader.includes("window.__genycasterSetUpdateLanguage=setUpdateLanguage"));
    assert.ok(loader.includes("if(latestReleaseDetails)fillReleaseDetails(latestReleaseDetails)"));
    assert.ok(native.includes('qsTrId("genycaster.update.available")'));
    assert.ok(native.includes('qsTrId("genycaster.update.no_notes")'));
    assert.ok(native.includes('if (Qt.platform.os === "wasm")'));
});

test("update drawer reuses the repository icon and success tokens across WASM and native",()=>{
    const shell=read("ui/wasm/genycaster-shell.html");
    const cmake=read("ui/CMakeLists.txt");
    const pack=read("ci/package_wasm_assets.py");
    const serviceWorker=read("ui/wasm/genycaster-sw.js");
    const native=read("ui/components/settings/UpdateNoticeDrawer.qml");
    assert.match(shell,/class="update-glyph"[^>]*><span class="update-symbol"/);
    assert.doesNotMatch(shell,/class="update-glyph"[^>]*><svg/);
    assert.match(shell,/mask:url\("\.\/genycaster-update-icon\.svg"\)/);
    assert.match(shell,/--update-success:#5DD9A0/);
    assert.match(shell,/--update-success:#23885C/);
    assert.match(shell,/#genycaster-update-now\{background:var\(--update-success\)/);
    assert.match(cmake,/assets\/icons\/genyleap\/icons\/actions\/repost\.svg/);
    assert.match(pack,/genycaster-update-icon\.svg/);
    assert.match(serviceWorker,/genycaster-update-icon\.svg/);
    assert.match(native,/fillColor: Theme\.successSurface/);
    assert.match(native,/color: Theme\.success/);
    assert.match(native,/positive: true/);
});

test("all 12 language packs translate current miniapp taxonomy and provider caption",()=>{
    const localeFiles=readdirSync(new URL("../../ui/i18n/",import.meta.url))
        .filter(name=>name.startsWith("qml_") && name.endsWith(".ts"));
    const terms=["art_creativity","developer_tools","food_drink","lifestyle","photography",
        "travel","onchain","ai","memes","video","business"];
    for(const lang of localeFiles){
        const ts=read("ui/i18n/"+lang);
        for(const cat of terms)
            assert.ok(ts.includes('<message id="genycaster.apps.category.'+cat+'">'),lang+" "+cat);
        for(const caption of ["catalogue_public","catalogue_farcaster","catalogue_neynar_docs"])
            assert.ok(ts.includes('<message id="genycaster.apps.'+caption+'">'),lang+" "+caption);
    }
    const fa=read("ui/i18n/qml_fa_IR.ts");
    assert.match(fa,/<translation>هنر و خلاقیت<\/translation>/);
    assert.match(fa,/<translation>فهرست عمومی مینی‌اپ‌های جنی‌کستر<\/translation>/);
    const categories=read("ui/components/controls/LocaleFormat.qml");
    assert.ok(categories.includes('case "art creativity"'));
    assert.ok(categories.includes('qsTrId("genycaster.apps.category.art_creativity")'));
    const apps=read("ui/components/apps/AppsSurface.qml");
    assert.match(apps,/miniAppCatalog\.providerLabel === "Genycaster public Mini Apps catalogue"/);
    assert.match(apps,/qsTrId\("genycaster.apps.catalogue_public"\)/);
    assert.match(apps,/String\(app.category \|\| ""\)\.trim\(\) === root.topicFilter/);
});

test("foreign Mini App titles and descriptions align to the locale without reordering the text",()=>{
    for(const name of ["AppsSurface","MiniAppLibraryShelves","MiniAppFeaturedCarousel"]){
        const qml=read("ui/components/" + componentPath(name)+".qml");
        const alignment="horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft";
        const minCount=name==="AppsSurface"?9:name==="MiniAppLibraryShelves"?3:2;
        assert.ok(qml.split(alignment).length-1>=minCount,name+" edge alignment");
        assert.ok(qml.includes("textFormat: Text.PlainText"),name+" plain text safety");
        assert.ok(qml.includes("String("),name+" preserves developer-provided wording");
        assert.doesNotMatch(qml,/text:\s*qsTr\(String\(/,"never machine-translate metadata");
    }
});


test("verified Geny Pro admission recovers saved sessions before rejecting another Mini App", () => {
    const apps=read("ui/components/apps/AppsSurface.qml");
    const shell=read("ui/components/shell/AppShell.qml");
    const openproof=read("ui/app/identity/connections.cpp");
    const header=read("ui/app/identity/connections.hpp");
    const startup=read("ui/app/main.cpp");
    const card=read("ui/components/profile/ProfileTrustCard.qml");
    const sidebar=read("ui/components/navigation/SideNavigation.qml");

    assert.match(header,/Q_INVOKABLE void retryProEntitlements\(\)/);
    const retry=openproof.slice(openproof.indexOf(
        "void OpenProofConnectionsController::retryProEntitlements()"),
        openproof.indexOf("void OpenProofConnectionsController::fetchProEntitlements"));
    assert.match(retry,/cloudAuthorizationReady\(\)/);
    assert.match(retry,/retryAccountSession\(\)/);
    const restore=openproof.slice(openproof.indexOf(
        "void OpenProofConnectionsController::retryAccountSession()"),
        openproof.indexOf("void OpenProofConnectionsController::retryProEntitlements()"));
    assert.match(restore,/restoreAccountSession\(\)/);
    assert.match(retry,/bootstrapAccountSession\(\)/);
    assert.doesNotMatch(retry,/authorizeAccount\(|signerService->|requestFid\(|provision\(/);

    assert.match(apps,/pendingEntitlementLaunch/);
    assert.match(apps,/openProofConnections\.retryProEntitlements\(\)/);
    assert.match(apps,/onProEntitlementsChanged\(\)/);
    assert.match(apps,/miniAppTaskManager\.taskCount < miniAppHost\.sessionLimit/);
    assert.match(apps,/status\.eligible === true/);
    assert.match(apps,/Number\(presentationBridge\.accountFid \|\| 0\) !== pending\.fid/);
    assert.match(apps,/qsTrId\("genycaster\.pro\.requires_pro"\)/);
    assert.match(apps,/qsTrId\("genycaster\.pro\.verification_unavailable"\)/);
    assert.match(shell,/onProDetailsRequested: genyProDetailsDrawer\.open\(\)/);

    assert.match(startup,/status\.value\(QStringLiteral\("status"\)\)\.toString\(\) != QStringLiteral\("verified"\)/);
    assert.match(startup,/miniAppRuntimeOwner->setSessionLimit\(/);
    assert.match(startup,/miniAppHost\.setSessionLimit\(/);
    assert.match(card,/objectName: "sidebarProMembershipCard"/);
    assert.match(card,/root\.proEligible/);
    assert.match(card,/fillColor: root\.compact \? Theme\.surfaceMuted : Theme\.contextRailCard/);
    assert.match(sidebar,/proEligible: openProofConnections/);
    assert.match(openproof,/m_proEntitlementsFetchedAt = verified/);
});

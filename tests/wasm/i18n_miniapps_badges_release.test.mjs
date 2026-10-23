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
    const apps=read("ui/components/AppsSurface.qml");
    const fmt=read("ui/components/LocaleFormat.qml");
    const saved=read("ui/components/MiniAppLibraryShelves.qml");
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
    for (const view of [apps,saved,read("ui/components/MiniAppFeaturedCarousel.qml")])
        assert.ok(!view.includes('qsTr("'),"unregistered qsTr in "+view.slice(0,30));
});

test("client attribution is evidence-based, localized and shared by normal/reply cast UI",()=>{
    const badge=read("ui/components/SourceClientBadge.qml");
    const cast=read("ui/components/CastItem.qml");
    const reply=read("ui/components/ThreadReplyItem.qml");
    const bindings=read("ui/mobile/runtime_ui_bindings.cpp");
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
    const main=read("ui/mobile/main.cpp");
    const loader=read("ui/wasm/genycaster-loader.js");
    const native=read("ui/components/UpdateNoticeDrawer.qml");
    assert.ok(main.includes('window.__genycasterSetUpdateLanguage(tag)'));
    assert.ok(loader.includes("window.__genycasterSetUpdateLanguage=setUpdateLanguage"));
    assert.ok(loader.includes("if(latestReleaseDetails)fillReleaseDetails(latestReleaseDetails)"));
    assert.ok(native.includes('qsTrId("genycaster.update.available")'));
    assert.ok(native.includes('qsTrId("genycaster.update.no_notes")'));
    assert.ok(native.includes('if (Qt.platform.os === "wasm") return'));
});

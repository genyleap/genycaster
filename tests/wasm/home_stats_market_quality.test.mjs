import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {fileURLToPath} from "node:url";
import {resolve, dirname} from "node:path";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const read = path => readFileSync(resolve(root, path), "utf8");

test("stats range labels use translatable IDs, not English-only abbreviations", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    for (const id of ["range_week", "range_month", "range_quarter", "range_year"])
        assert.ok(panel.includes('qsTrId("genycaster.stats.' + id + '")'));
    assert.doesNotMatch(panel, /title:\s*"(?:7D|30D|90D|1Y)"/);
    assert.match(panel, /LocaleFormat\.formatInteger\(modelData\)/);
});

test("every bundled locale contains the new stats and market names", () => {
    const ids = [
        "genycaster.stats.range_week", "genycaster.stats.range_month",
        "genycaster.stats.range_quarter", "genycaster.stats.range_year",
        "genycaster.stats.radar_explainer", "genycaster.stats.github_active_days",
        "genycaster.market.asset_bitcoin", "genycaster.market.asset_ethereum",
        "genycaster.market.asset_solana"
    ];
    for (const locale of [
        "en_US", "fa_IR", "ar_SA", "de_DE", "es_ES", "fr_FR",
        "hi_IN", "ja_JP", "ko_KR", "pt_BR", "ru_RU", "zh_CN"
    ]) {
        const catalog = read("ui/i18n/qml_" + locale + ".ts");
        for (const id of ids) {
            const needle = '<message id="' + id + '">';
            assert.equal(catalog.split(needle).length - 1, 1, locale + ": " + id);
        }
    }
    const fa = read("ui/i18n/qml_fa_IR.ts");
    assert.match(fa, /<message id="genycaster\.stats\.casts">[\s\S]*?<translation>نشرها<\/translation>/);
    assert.match(fa, /<message id="genycaster\.stats\.recasts">[\s\S]*?<translation>بازنشرها<\/translation>/);
    assert.match(fa, /<message id="genycaster\.market\.asset_bitcoin">[\s\S]*?<translation>بیت‌کوین<\/translation>/);
    assert.match(fa, /<message id="genycaster\.stats\.range_week">[\s\S]*?<translation>۷ روزه<\/translation>/);
    assert.match(fa, /هر گفتگو با یک درود آغاز می‌شود/);
    assert.match(fa, /جنی‌کستر/);
});

test("received engagement uses real cohort-based line series with independent axes", () => {
    const view = read("ui/components/home/StatsBreakdown.qml");
    const panel = read("ui/components/home/StatsPanel.qml");
    const aggregate = read("ui/app/social/stats.cpp");
    const header = read("ui/app/social/stats.hpp");
    assert.match(view, /readonly property real likesMax/);
    assert.match(view, /readonly property real secondaryMax/);
    assert.match(view, /axis: 0/);
    assert.match(view, /axis: 1/);
    assert.match(view, /PathPolyline\s*\{/);
    assert.match(view, /Shape\.GeometryRenderer/);
    assert.match(view, /layer\.samples:\s*4/);
    assert.match(view, /indexAt\(/);
    assert.match(view, /Theme\.rtl/);
    assert.doesNotMatch(view, /distributionTrack|radarTween/);
    assert.match(view, /received_by_publish_date/);
    assert.match(view, /publish_date_snapshot/);
    assert.match(panel, /activity: statsController\.activity/);
    assert.match(header, /m_receivedByPublishedDay/);
    for(const key of ["likes", "recasts", "receivedReplies"]) {
        assert.match(aggregate, new RegExp("perDay\\." + key + "\\s*\\+="));
        assert.ok(aggregate.includes('QStringLiteral("' + key + '")'));
    }
    const nativeTest=read("tests/ui/stats_controller_tests.cpp");
    assert.match(nativeTest,/yearlyLikes == 55 && yearlyRecasts == 18/);
});

test("large desktop statistics fill the workspace without stretching the home composer", () => {
    const home=read("ui/components/home/HomeDashboard.qml");
    assert.match(home, /root\.activeTab === "stats" \? parent\.width : root\.homeContentMaxWidth/);
    assert.match(home, /Layout\.maximumWidth: root\.homeContentMaxWidth/);
    assert.match(home, /width: parent\.width[\s\S]*?tabActive: root\.activeTab === "stats"/);
});

test("market insight grid pairs Fear and CMC on desktop and stacks on phones", () => {
    const market=read("ui/components/home/MarketPanel.qml");
    assert.match(market, /id: marketInsightGrid/);
    assert.match(market, /columns: root\.wideLayout \? 2 : 1/);
    assert.match(market, /readonly property bool wideLayout: width >= 720/);
    assert.match(market, /id: fearCard[\s\S]*?Layout\.row: root\.wideLayout \? 0 : 2/);
    assert.match(market, /Layout\.row: root\.wideLayout \? 1 : 3/);
    assert.match(market, /layoutDirection: Theme\.uiDirection/);
});

test("publishing stacks round only the outer silhouette", () => {
    const chart = read("ui/components/home/StatsChart.qml");
    assert.match(chart, /id:\s*stack/);
    assert.match(chart, /height:\s*Math\.max\(0,\s*column\.castHeight \+ column\.replyHeight\)/);
    assert.match(chart, /visible:\s*column\.castHeight > 0/);
    assert.match(chart, /opacity:\s*1/);
    assert.match(chart, /onHoveredChanged:\s*root\.hoveredIndex = hovered/);
    assert.doesNotMatch(chart, /0\.52/);
});

test("GitHub insights are gated on verified owner identity, not public profile links", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    assert.match(panel, /openProofConnections\.cloudAuthorizationReady/);
    assert.match(panel, /openProofConnections\.connections/);
    assert.match(panel, /genyPulseActivity\.requestGitHubContributions/);
    assert.match(panel, /githubStatus === "ok"/);
    assert.match(panel, /refreshGitHubContributions/);
    assert.match(panel, /identity-unconfirmed/);
    assert.match(panel, /rate-limited/);
    assert.match(panel, /githubCalendar\.username/);
    assert.match(panel, /githubCalendar\.year/);
});

test("market names translate independently of immutable ticker symbols", () => {
    const market = read("ui/components/home/MarketPanel.qml");
    for (const asset of ["bitcoin", "ethereum", "solana"])
        assert.ok(market.includes('qsTrId("genycaster.market.asset_' + asset + '")'));
    for (const symbol of ["BTC", "ETH", "SOL"])
        assert.ok(market.includes('case "' + symbol + '"'));
    assert.match(market, /forceLeftToRight:\s*true/);
});

test("local MSAA improves native market lines and fear gauge on low-MSAA windows", () => {
    const trend = read("ui/components/content/TokenSparkline.qml");
    const gauge = read("ui/components/home/MarketFearGauge.qml");
    for (const component of [trend, gauge]) {
        assert.match(component, /preferredRendererType:\s*Shape\.GeometryRenderer/);
        assert.match(component, /layer\.samples:\s*4/);
        assert.match(component, /layer\.smooth:\s*true/);
    }
    assert.match(trend, /TokenSparklineGeometry\.computeSmooth/);
});

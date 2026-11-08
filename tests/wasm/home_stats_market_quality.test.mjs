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

test("radar uses real counters and a consistent scale", () => {
    const radar = read("ui/components/home/StatsBreakdown.qml");
    for (const key of ["likes", "recasts", "receivedReplies", "casts", "replies"])
        assert.ok(radar.includes("metrics." + key), "missing real counter " + key);
    assert.match(radar, /category\.count \/ root\.highestCount/);
    assert.match(radar, /Canvas\s*\{/);
    assert.match(radar, /Theme\.rtl/);
    assert.match(radar, /Theme\.reduceMotion/);
    assert.match(radar, /LocaleFormat\.formatInteger\(modelData\.count\)/);
});

test("publishing stacks round only the outer silhouette", () => {
    const chart = read("ui/components/home/StatsChart.qml");
    assert.match(chart, /id:\s*stack/);
    assert.match(chart, /height:\s*Math\.max\(0,\s*column\.castHeight \+ column\.replyHeight\)/);
    assert.match(chart, /visible:\s*column\.castHeight > 0/);
    assert.equal((chart.match(/opacity:\s*root\.selectedIndex/g) || []).length, 1);
});

test("GitHub insights are gated on verified owner identity, not public profile links", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    assert.match(panel, /openProofConnections\.cloudAuthorizationReady/);
    assert.match(panel, /openProofConnections\.connections/);
    assert.match(panel, /genyPulseActivity\.requestGitHubContributions/);
    assert.match(panel, /githubCalendar\.status === "ok"/);
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

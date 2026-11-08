import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {fileURLToPath} from "node:url";
import {resolve, dirname} from "node:path";
import vm from "node:vm";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const read = path => readFileSync(resolve(root, path), "utf8");

test("stats range labels use translatable IDs, not English-only abbreviations", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    for (const id of ["range_week", "range_month", "range_quarter", "range_year"])
        assert.ok(panel.includes('qsTrId("genycaster.stats.' + id + '")'));
    assert.doesNotMatch(panel, /title:\s*"(?:7D|30D|90D|1Y)"/);
    assert.match(panel, /root\.rangeLabel\(periodChoice\.modelData\)/);
});

test("every bundled locale contains the new stats and market names", () => {
    const ids = [
        "genycaster.stats.range_week", "genycaster.stats.range_month",
        "genycaster.stats.range_quarter", "genycaster.stats.range_year",
        "genycaster.stats.radar_explainer", "genycaster.stats.github_active_days",
        "genycaster.market.asset_bitcoin", "genycaster.market.asset_ethereum",
        "genycaster.market.asset_solana", "genycaster.stats.github_recent_weeks"
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
    assert.match(view, /PathSvg\s*\{/);
    assert.match(view, /function smoothPath\(/);
    assert.match(view, /ShapePath\.RoundCap/);
    assert.match(view, /fillArea\) \{/);
    assert.doesNotMatch(view, /PathPolyline\s*\{/);
    assert.match(view, /id: inspectCard/);
    assert.match(view, /onPointChanged/);
    assert.match(view, /Shape\.CurveRenderer/);
    assert.doesNotMatch(view, /layer\.samples/);
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
    assert.match(home, /width: root\.activeTab === "stats"[\s\S]*?Math\.max\(1, parent\.width\)/);
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

test("publishing stacks share a square baseline, a rounded top and native Feed Pulse rendering", () => {
    const chart = read("ui/components/home/StatsChart.qml");
    assert.match(chart, /id:\s*stack/);
    assert.match(chart, /height:\s*Math\.max\(0,\s*column\.castHeight \+ column\.replyHeight\)/);
    assert.match(chart, /visible:\s*column\.castHeight > 0/);
    assert.match(chart, /opacity:\s*column\.highlighted \? 0\.86 : 1/);
    assert.match(chart, /curveTopLeft:\s*column\.replyHeight <= 0/);
    assert.match(chart, /curveBottomLeft:\s*false/);
    assert.match(chart, /curveBottomRight:\s*false/);
    assert.match(chart, /borderWidth:\s*0/);
    assert.match(chart, /onPointChanged:/);
    assert.match(chart, /root\.hoveredIndex = point\.position\.y >= stack\.y - 6/);
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

test("monotone SVG Bezier geometry is rounded without overshooting verified daily counts", () => {
    const source = read("ui/components/home/StatsBreakdown.qml");
    const start = source.indexOf("    function smoothPath(");
    assert.notEqual(start, -1);
    const brace = source.indexOf("{", start);
    let depth = 0, end = -1;
    for (let i = brace; i < source.length; ++i) {
        if (source[i] === "{") ++depth;
        if (source[i] === "}" && --depth === 0) {
            end = i + 1;
            break;
        }
    }
    assert.ok(end > brace, "QML interpolation function is complete");
    const js = source.slice(start, end);
    const values = [0, 0, 4, 40, 143, 3, 0, 0, 7, 0];
    const baseline = 172;
    for (const rtl of [false, true]) {
        const positions = values.map((v, i) => ({
            x: rtl ? 400 - i * 40 : i * 40,
            y: baseline - v
        }));
        const ctx = vm.createContext({
            pointsFor: () => positions,
            plotInset: 5
        });
        vm.runInContext(js, ctx);
        const path = ctx.smoothPath("likes", 143, 405, 178, false);
        const cubicSegments = [...path.matchAll(
            /C\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/g)];
        assert.equal(cubicSegments.length, positions.length - 1);
        for (let i = 0; i < cubicSegments.length; ++i) {
            const [c1x,c1y,c2x,c2y,endX,endY] = cubicSegments[i].slice(1).map(Number);
            const [a,b] = [positions[i],positions[i + 1]];
            assert.ok([c1x,c1y,c2x,c2y,endX,endY].every(Number.isFinite));
            assert.ok(Math.abs(endX - b.x) < 0.01);
            assert.ok(Math.abs(endY - b.y) < 0.01);
            for (const control of [c1y,c2y]) {
                assert.ok(control >= Math.min(a.y,b.y) - 0.011,
                    "the curve must never create a new daily minimum");
                assert.ok(control <= Math.max(a.y,b.y) + 0.011,
                    "the curve must never create a new daily maximum");
            }
            for (const control of [c1x,c2x]) {
                assert.ok(control >= Math.min(a.x,b.x) - 0.01);
                assert.ok(control <= Math.max(a.x,b.x) + 0.01);
            }
        }
        assert.match(ctx.smoothPath("likes",143,405,178,true), / Z$/);
    }
});

test("GitHub contribution heatmap is backed by verified calendar days, not fabricated progress", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    assert.match(panel, /readonly property var githubDayMap/);
    assert.match(panel, /if \(!githubReady\) return map/);
    assert.match(panel, /githubCalendar\.days \|\| \[\]/);
    assert.match(panel, /root\.githubWeekCount \* 7/);
    assert.match(panel, /root\.githubColor\(dayData\.level\)/);
    assert.match(panel, /root\.githubDateAt\(index\)/);
    assert.match(panel, /github_recent_weeks/);
    assert.doesNotMatch(panel, /githubGrowthPercentage|fakeGithubDays/);
});


test("engagement rings use measured shares, theme colors and an adaptive layout", () => {
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    const cmake = read("ui/CMakeLists.txt");
    assert.match(cmake, /components\/home\/StatsDistributionRings\.qml/);
    assert.match(breakdown, /StatsDistributionRings\s*\{/);
    assert.match(breakdown, /columns:\s*root\.compact \? 1 : 2/);
    assert.match(rings, /readonly property real sum:/);
    for (const key of ["likesShare", "recastsShare", "repliesShare"])
        assert.match(rings, new RegExp(key + ": sum > 0"));
    assert.match(rings, /PathAngleArc\s*\{/);
    assert.match(rings, /preferredRendererType:\s*Shape\.CurveRenderer/);
    assert.doesNotMatch(rings, /layer\.samples/);
    assert.match(rings, /readonly property var leadingSeries:/);
    assert.match(rings, /columns:\s*1/);
    assert.match(rings, /onSumChanged:\s*Qt\.callLater\(animate\)/);
    assert.match(rings, /Theme\.reduceMotion/);
    assert.match(rings, /Theme\.rtl/);
    assert.doesNotMatch(rings, /overallProgress|goalProgress|targetPercentage/);
});

test("engagement chart animates actual paths and publishing hover is paint-only", () => {
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    const publishing = read("ui/components/home/StatsChart.qml");
    assert.match(breakdown, /id:\s*curveRevealMask/);
    assert.match(breakdown, /property real chartReveal:/);
    assert.match(breakdown, /Qt\.callLater\(animateChart\)/);
    assert.match(breakdown, /Theme\.rtl \? plot\.width \* \(1 - root\.chartReveal\)/);
    assert.doesNotMatch(breakdown, /color:\s*Theme\.warning,\s*axis: 1/);
    assert.match(publishing, /readonly property bool highlighted:/);
    assert.match(publishing, /opacity:\s*column\.highlighted \? 0\.86 : 1/);
    assert.match(publishing, /Behavior on opacity/);
    assert.doesNotMatch(publishing, /Behavior on scale/);
    assert.doesNotMatch(publishing, /Behavior on color/);
    assert.match(publishing, /Theme\.reduceMotion/);
});

test("analytics uses categorical theme tokens instead of three blue accent shades", () => {
    const theme = read("ui/theme/Theme.qml");
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    const publishing = read("ui/components/home/StatsChart.qml");
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    assert.match(theme, /readonly property color analyticsRecasts:/);
    assert.match(theme, /readonly property color analyticsReplies:/);
    assert.match(breakdown, /readonly property color recastsInk: Theme\.analyticsRecasts/);
    assert.match(breakdown, /readonly property color repliesInk: Theme\.analyticsReplies/);
    assert.match(publishing, /readonly property color replyInk: Theme\.analyticsReplies/);
    assert.match(rings, /property color recastsColor: Theme\.analyticsRecasts/);
    assert.match(rings, /property color repliesColor: Theme\.analyticsReplies/);
    assert.match(rings, /root\.activeSeries\.share \* 100/);
    assert.doesNotMatch(rings, /LocaleFormat\.formatInteger\(root\.sum\)\s*\n\s*color: Theme\.textPrimary/);
});


test("ring centre labels never set the same QML binding twice", () => {
    const source = read("ui/components/home/StatsDistributionRings.qml");
    const start = source.indexOf("ColumnLayout {\n                anchors.centerIn: parent");
    assert.ok(start >= 0, "centre label group must exist");
    const end = source.indexOf("\n        ColumnLayout {", start + 1);
    assert.ok(end > start, "centre label group must remain bounded");
    const centre = source.slice(start, end);

    // Each centre label is a leaf QML object. Read only its own immediate
    // bindings; siblings may legitimately use the same property names.
    const labels = [...centre.matchAll(/DirectionAwareLabel\s*\{([\s\S]*?)\n {16}\}/g)];
    assert.equal(labels.length, 3, "expected percentage, label and count");
    function duplicateBindings(body) {
        const seen = new Set();
        const duplicates = [];
        for (const line of body.split("\n")) {
            const binding = /^ {20}([A-Za-z_][\w.]*)\s*:/.exec(line);
            if (!binding) continue;
            if (seen.has(binding[1])) duplicates.push(binding[1]);
            seen.add(binding[1]);
        }
        return duplicates;
    }
    for (const [, body] of labels) {
        assert.deepEqual(duplicateBindings(body), [], "a QML property can only be bound once");
        assert.equal(
            (body.match(/^ {20}horizontalAlignment\s*:/gm) || []).length, 1,
            "centre alignment must have exactly one owner"
        );
    }
    // Prove this guard catches the precise macOS qmlcachegen regression.
    const broken = labels[0][1].replace(
        /horizontalAlignment: Text.AlignHCenter/,
        "horizontalAlignment: Text.AlignHCenter\n                    horizontalAlignment: Text.AlignHCenter"
    );
    assert.deepEqual(duplicateBindings(broken), ["horizontalAlignment"]);
});


test("social chart palette is subdued and scaled strokes stay thin", () => {
    const theme = read("ui/theme/Theme.qml");
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    assert.match(theme, /analyticsRecasts: dark \? "#98A0AE" : "#6B7280"/);
    assert.match(theme, /analyticsReplies: dark \? "#C2C7D0" : "#9CA3AF"/);
    assert.match(breakdown, /function lineInk\(key, ink\)/);
    assert.match(breakdown, /function lineWidth\(key\)/);
    assert.match(breakdown, /strokeColor:\s*root\.lineInk\("likes", Theme\.accent\)/);
    assert.match(breakdown, /strokeColor:\s*root\.lineInk\("recasts", root\.recastsInk\)/);
    assert.match(breakdown, /strokeColor:\s*root\.lineInk\("receivedReplies", root\.repliesInk\)/);
    assert.match(breakdown, /strokeStyle:\s*ShapePath\.DashLine/);
    assert.match(breakdown, /const dx = \(b\.x - a\.x\) \* 0\.16/);
    assert.match(rings, /readonly property real stroke: root\.compact \? 4\.5 : 5\.8/);
});

test("publishing bar stacks have a square baseline and overlap at shared edges", () => {
    const chart = read("ui/components/home/StatsChart.qml");
    const begin = chart.indexOf("Item {\n                                id: stack");
    const end = chart.indexOf("TapHandler {", begin);
    assert.ok(begin >= 0 && end > begin);
    const stack = chart.slice(begin, end);
    assert.match(stack, /clip:\s*true/);
    assert.match(stack, /opacity:\s*column\.highlighted \? 0\.86 : 1/);
    assert.match(stack, /height: column\.replyHeight \+ \(column\.castHeight > 0 \? 1 : 0\)/);
    assert.match(stack, /curveBottomLeft:\s*false/);
    assert.match(stack, /curveBottomRight:\s*false/);
    assert.doesNotMatch(stack, /borderWidth:\s*[1-9]|Behavior on scale/);

    // Opaque upper segments cover exactly one logical pixel of lower material,
    // and no extra pixel is added when the stacked bar has no lower segment.
    const overlap = (replies, casts) => replies + (casts > 0 ? 1 : 0);
    assert.equal(overlap(12, 9), 13);
    assert.equal(overlap(12, 0), 12);
    assert.equal(overlap(0, 9), 1); // hidden upper segment cannot draw it
});


test("engagement series focus follows a persistent legend selection or hovered curve", () => {
    const view = read("ui/components/home/StatsBreakdown.qml");
    assert.match(view, /property string selectedSeriesKey: "likes"/);
    assert.match(view, /property string hoveredSeriesKey: ""/);
    assert.match(view, /readonly property string focusedSeriesKey: hoveredSeriesKey \|\| selectedSeriesKey/);
    assert.match(view, /function nearestSeriesAt\(x, y, w, h\)/);
    assert.match(view, /root\.hoveredSeriesKey = root\.nearestSeriesAt\(/);
    assert.match(view, /root\.selectedSeriesKey = root\.selectedSeriesKey === key \? "" : key/);
    assert.match(view, /Accessible\.onPressAction: root\.selectedSeriesKey = legendEntry\.modelData\.key/);
    assert.match(view, /id: inspectCard/);
    assert.match(view, /root\.inspectedBucket\[modelData\.key\]/);
});

test("ring arcs and legends inspect every metric without changing provider totals", () => {
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    assert.match(rings, /readonly property int activeSeriesIndex: hoveredSeriesIndex >= 0/);
    assert.match(rings, /readonly property var activeSeries: segments\[activeSeriesIndex\]/);
    assert.match(rings, /function ringIndexAt\(x, y\)/);
    assert.match(rings, /root\.hoveredSeriesIndex = root\.ringIndexAt\(mouse\.x, mouse\.y\)/);
    assert.match(rings, /onClicked: mouse =>/);
    assert.match(rings, /root\.selectedSeriesIndex = index/);
    assert.match(rings, /root\.selectedSeriesIndex = segmentRow\.index/);
    assert.match(rings, /Keys\.onLeftPressed:/);
    assert.match(rings, /Keys\.onRightPressed:/);
    assert.match(rings, /LocaleFormat\.formatInteger\(root\.activeSeries\.count\)/);
    assert.match(rings, /Math\.round\(root\.activeSeries\.share \* 100\)/);
    assert.match(rings, /readonly property real sum:/);
});

test("publishing bars expose bounded live tooltip and touch fallback", () => {
    const chart = read("ui/components/home/StatsChart.qml");
    assert.match(chart, /id: barInspectCard/);
    assert.match(chart, /enabled: false/);
    assert.match(chart, /visible: root\.hoveredIndex >= 0 && plotHover\.hovered/);
    assert.match(chart, /function columnCenter\(index\)/);
    assert.match(chart, /Theme\.rtl \? width - center : center/);
    assert.match(chart, /Math\.max\(4, anchorX - width - 12\)/);
    assert.match(chart, /Math\.max\(4, Math\.min\(plot\.height - height - 4,/);
    assert.match(chart, /text: root\.localizedRange\(root\.selected\)/);
    assert.match(chart, /root\.selected\.casts \|\| 0/);
    assert.match(chart, /root\.selected\.replies \|\| 0/);
    assert.match(chart, /onPointChanged: \{/);
    assert.match(chart, /Accessible\.onPressAction: root\.selectedIndex = index/);
    assert.match(chart, /: qsTrId\("genycaster.stats.chart_hint"\)/);
});


test("ring annulus hit-testing selects the intended ring and rejects the centre", () => {
    const source = read("ui/components/home/StatsDistributionRings.qml");
    const match = source.match(/    function ringIndexAt\(x, y\) \{([\s\S]*?)\n    \}\n\n    function animate/);
    assert.ok(match, "the ring hit test must be independently inspectable");
    const hitTest = vm.runInNewContext(
        "function ringIndexAt(x, y) {" + match[1] + "\n}\nringIndexAt",
        {ring: {center: 100, stroke: 6, outerRadius: 90, middleRadius: 68, innerRadius: 46}}
    );
    assert.equal(hitTest(190, 100), 0, "outer likes ring");
    assert.equal(hitTest(168, 100), 1, "middle recasts ring");
    assert.equal(hitTest(146, 100), 2, "inner replies ring");
    assert.equal(hitTest(100, 100), -1, "center label is not an arc");
    assert.equal(hitTest(20, 20), -1, "empty corners are not arcs");
});

test("publishing tooltip anchors follow column order in both LTR and RTL", () => {
    const source = read("ui/components/home/StatsChart.qml");
    const match = source.match(/                function columnCenter\(index\) \{([\s\S]*?)\n                \}\n                clip: true/);
    assert.ok(match);
    const ctx = vm.createContext({
        width: 300, columnGap: 2,
        root: {bars: Array(5)},
        Theme: {rtl: false}
    });
    vm.runInContext("function columnCenter(index) {" + match[1] + "\n}", ctx);
    const left = ctx.columnCenter(0);
    const right = ctx.columnCenter(4);
    assert.ok(left < right && left > 0 && right < 300);
    ctx.Theme.rtl = true;
    assert.ok(Math.abs(ctx.columnCenter(0) - (300 - left)) < 0.001);
    assert.ok(Math.abs(ctx.columnCenter(4) - (300 - right)) < 0.001);
});


test("stats filter uses a contextual three-dot menu beside the period note", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    assert.match(panel, /objectName: "statsPeriodContextRow"/);
    assert.match(panel, /text: qsTrId\("genycaster.stats.period_note"\)/);
    assert.match(panel, /text: root.rangeLabel\(root.periodDays\)/);
    assert.match(panel, /objectName: "statsPeriodMenuButton"/);
    assert.match(panel, /iconGlyph: Theme.iconMore/);
    assert.match(panel, /objectName: "statsPeriodMenu"/);
    assert.match(panel, /closePolicy: Popup.CloseOnEscape \| Popup.CloseOnPressOutsideParent/);
    assert.match(panel, /model: root.rangeDays/);
    assert.match(panel, /root.periodRequested\(modelData\)/);
    assert.match(panel, /periodMenu.close\(\)/);
    assert.match(panel, /if \(periodMenu.visible\)\s*\n\s*periodMenu.close\(\)\s*\n\s*else\s*\n\s*periodMenu.open\(\)/);
    assert.match(panel, /text: qsTrId\("genycaster.stats.choose_period"\)/);
    for (const locale of [
        "en_US", "fa_IR", "ar_SA", "de_DE", "es_ES", "fr_FR",
        "hi_IN", "ja_JP", "ko_KR", "pt_BR", "ru_RU", "zh_CN"
    ]) {
        const catalog = read("ui/i18n/qml_" + locale + ".ts");
        assert.equal(catalog.split('<message id="genycaster.stats.choose_period">').length - 1,
                     1, "missing translated menu label: " + locale);
    }
    assert.doesNotMatch(panel, /statsDesktopPeriodFilter|statsMobilePeriodFilter/);
    assert.match(panel, /compact: root.compact\s*\n\s*metrics: root.metrics/);
    assert.match(panel, /compact: root.compact\s*\n\s*bars: statsController.activity/);
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    assert.match(breakdown, /columns: root.compact \? 1 : 2/);
});

test("Stats only reduces OUTER shell gutters and restores original card spacing", () => {
    const shell = read("ui/components/shell/AppShell.qml");
    const home = read("ui/components/home/HomeDashboard.qml");
    const breakdown = read("ui/components/home/StatsBreakdown.qml");
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    assert.match(shell, /readonly property bool homeStatsWorkspace: root\.homeSurfaceActive/);
    assert.match(shell, /homeDashboard\.activeTab === "stats"/);
    assert.match(shell, /anchors\.leftMargin: root\.wide && !root\.expandedWorkspace \? Theme\.space2 : 0/);
    assert.match(shell, /anchors\.rightMargin: root\.wide && !root\.expandedWorkspace \? Theme\.space2 : 0/);
    assert.match(shell, /root\.expandedWorkspace \? Theme\.space1 : Theme\.space3/);
    assert.match(shell, /Layout\.leftMargin: root\.compact \? Theme\.mobileGutter\s*\n\s*: root\.homeStatsWorkspace \? 0 : Theme\.space2/);
    assert.match(shell, /Layout\.rightMargin: root\.compact \? Theme\.mobileGutter\s*\n\s*: root\.homeStatsWorkspace \? 0 : Theme\.space2/);
    assert.match(home, /anchors\.margins: Theme\.space3/);
    assert.match(home, /width: root\.activeTab === "stats"\s*\n\s*\? Math\.max\(1, parent\.width\)/);
    const rail = read("ui/components/navigation/ContextRail.qml");
    assert.match(rail, /Layout\.leftMargin: Theme\.space3/);
    assert.match(rail, /Layout\.rightMargin: Theme\.space3/);
    // Inner card padding and chart/ring spacing must stay at the former values.
    assert.match(breakdown, /anchors\.margins: Theme\.space4/);
    assert.match(breakdown, /columnSpacing: Theme\.space4/);
    assert.match(breakdown, /Layout\.preferredWidth: root\.compact \? 1\s*\n\s*: Math\.max\(180, root\.width - \(root\.width < 850 \? 220 : 265\)\)/);
    assert.match(breakdown, /Layout\.preferredWidth: root\.compact \? 1 : \(root\.width < 850 \? 204 : 248\)/);
    assert.match(rings, /readonly property int ringSize: compact \? 196 : 226/);
    assert.doesNotMatch(rings, /sidebarMode/);
});

test("social and market tabs reveal content with reduced-motion-aware entrance", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    assert.match(panel, /property real sectionArrival: 1/);
    assert.match(panel, /id: sectionEntrance/);
    assert.match(panel, /duration: Theme.reduceMotion \? 0 : Theme.motionEmphasis/);
    assert.match(panel, /onSectionChanged:\s*\{/);
    assert.match(panel, /sectionEntrance.stop\(\)/);
    assert.match(panel, /sectionEntrance.start\(\)/);
    assert.match(panel, /opacity: root.loaded \? root.sectionArrival : 0/);
    assert.match(panel, /transform: Translate \{ y: \(1 - root.sectionArrival\) \* 8 \}/);
    assert.match(panel, /MarketPanel \{[\s\S]*?opacity: root.sectionArrival/);
});

test("hover-only tooltips close on pointer exit while touch selection keeps summary", () => {
    const line = read("ui/components/home/StatsBreakdown.qml");
    const bars = read("ui/components/home/StatsChart.qml");
    assert.match(line, /visible: root\.hoveredIndex >= 0 && plotHover\.hovered/);
    assert.match(bars, /visible: root\.hoveredIndex >= 0 && plotHover\.hovered/);
    assert.match(line, /enabled: false/);
    assert.match(bars, /enabled: false/);
    assert.match(line, /if \(!hovered\) \{\s*root\.hoveredIndex = -1/);
    assert.match(bars, /if \(!hovered\)\s*root\.hoveredIndex = -1/);
    assert.match(line, /onVisibleChanged: \{\s*if \(!visible\)/);
    assert.match(bars, /onVisibleChanged: if \(!visible\) hoveredIndex = -1/);
    assert.match(line, /root\.inspectedBucket\s*\n\s*\? root\.bucketLabel/);
    assert.match(bars, /root\.selected\s*\n\s*\? qsTrId\("genycaster\.stats\.chart_point"\)/);
});

test("chart lines and engagement rings reduce stroke width only in dense layouts", () => {
    const line = read("ui/components/home/StatsBreakdown.qml");
    const rings = read("ui/components/home/StatsDistributionRings.qml");
    assert.match(line, /const smallPlot = root\.compact \|\| plot\.width < 440/);
    assert.match(line, /emphasized \? 1\.55 : 1\.1/);
    assert.match(line, /emphasized \? 1\.25 : 0\.95/);
    assert.match(rings, /readonly property real stroke: root\.compact \? 4\.5 : 5\.8/);
});

test("sharing statistics opens the authentic composer with only verified loaded values", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    const home = read("ui/components/home/HomeDashboard.qml");
    const shell = read("ui/components/shell/AppShell.qml");
    assert.match(panel, /signal shareRequested\(string text\)/);
    assert.match(panel, /function buildShareText\(\)/);
    assert.match(panel, /if \(section !== "social" \|\| !loaded\) return ""/);
    assert.match(panel, /if \(Boolean\(metrics\.hasEngagement\)\)/);
    assert.match(panel, /root\.shareRequested\(draft\)/);
    assert.equal((home.match(/onShareRequested: text => root\.statsReportRequested\(text\)/g) || []).length, 2);
    assert.match(home, /signal statsReportRequested\(string text\)/);
    assert.match(shell, /onStatsReportRequested: text => root\.openComposer\(null, "", text, null, true\)/);
    assert.match(shell, /composerDrawer\.suggestedText = String\(suggestedText \|\| ""\)/);
    assert.match(shell, /composerDrawer\.structuredReport = Boolean\(structuredReport\)/);
    assert.doesNotMatch(panel, /publishCast|submitCast|sendCast|writeCast/);
    for (const locale of [
        "en_US", "fa_IR", "ar_SA", "de_DE", "es_ES", "fr_FR",
        "hi_IN", "ja_JP", "ko_KR", "pt_BR", "ru_RU", "zh_CN"
    ]) {
        const catalog = read("ui/i18n/qml_" + locale + ".ts");
        assert.equal(catalog.split('<message id="genycaster.stats.share_results">').length - 1,
                     1, "share label not localized in " + locale);
    }
});


test("stats popover selection is inset like native action menus, not painted against its frame", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    const menuStart = panel.indexOf('id: periodMenu\n');
    assert.ok(menuStart >= 0);
    const menu = panel.slice(menuStart, panel.indexOf('id: periodChoice', menuStart) + 900);
    assert.match(menu, /padding: Theme\.space2/);
    assert.match(menu, /width: periodMenu\.width - periodMenu\.leftPadding\s*\n\s*- periodMenu\.rightPadding/);
    assert.match(menu, /height: 38/);
    assert.match(menu, /background: PopupSurface \{/);
    assert.match(menu, /fillColor: Theme\.surfaceElevated/);
    const choices = panel.slice(panel.indexOf('id: periodChoice', menuStart));
    assert.match(choices, /background: Superellipse \{/);
    assert.match(choices, /curveDepth: Theme\.curveCompact/);
    assert.doesNotMatch(menu, /padding: 2/);
});


test("localized stats report has a precise scope note, a dated source and stable brand footer", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    const composer = read("ui/components/composer/ComposerDrawer.qml");
    const shell = read("ui/components/shell/AppShell.qml");
    const keys = [
        "share_report_title", "share_publishing_line", "share_engagement_line",
        "share_reactions_line", "share_scope_note", "share_prepared"
    ];
    const expectedArgCounts = [1, 2, 1, 3, 0, 1];
    const locales = [
        "en_US", "fa_IR", "ar_SA", "de_DE", "es_ES", "fr_FR",
        "hi_IN", "ja_JP", "ko_KR", "pt_BR", "ru_RU", "zh_CN"
    ];
    const stringsFor = locale => {
        const doc = read("ui/i18n/qml_" + locale + ".ts");
        const entries = {};
        for (let i = 0; i < keys.length; ++i) {
            const id = "genycaster.stats." + keys[i];
            const marker = '<message id="' + id + '">';
            assert.equal(doc.split(marker).length - 1, 1, locale + ": " + id);
            const start = doc.indexOf(marker);
            const block = doc.slice(start, doc.indexOf("</message>", start));
            assert.doesNotMatch(block, /type="unfinished"/);
            const translated = block.match(/<translation>([\s\S]*?)<\/translation>/)?.[1];
            assert.ok(translated && translated.trim().length > 0, locale + ": " + id);
            const placeholders = translated.match(/%[1-9]/g) || [];
            assert.deepEqual(
                placeholders.sort(),
                Array.from({length: expectedArgCounts[i]}, (_, x) => "%" + (x + 1)).sort(),
                locale + ": " + id
            );
            entries[id] = translated;
        }
        return entries;
    };
    class QmlText extends String {
        arg(value) {
            return new QmlText(String(this).replace(/%[1-9]/, String(value)));
        }
    }
    const textStart = panel.indexOf("    function buildShareText() {");
    const textEnd = panel.indexOf("\n    // Only the FID-bound", textStart);
    assert.ok(textStart >= 0 && textEnd > textStart);
    const fn = panel.slice(textStart, textEnd);
    const format = (locale, hasEngagement = true) => {
        const catalogue = stringsFor(locale);
        const values = {
            section: "social", loaded: true, periodDays: 90,
            metrics: {
                casts: 105, replies: 206, hasEngagement,
                engagement: 4118, likes: 3156, recasts: 508,
                receivedReplies: 454
            },
            rangeLabel: () => locale === "fa_IR" ? "سه‌ماهه" : "90 days",
            qsTrId: id => new QmlText(catalogue[id] || id),
            LocaleFormat: {
                formatInteger: x => locale === "fa_IR"
                    ? String(x).replace(/\d/g, d => "۰۱۲۳۴۵۶۷۸۹"[Number(d)])
                    : String(x),
                formatDateTime: () => locale === "fa_IR" ? "۱۸ آبان ۱۴۰۵" : "November 8, 2026"
            },
            Date: {now: () => 1794100000000}
        };
        const ctx = vm.createContext(values);
        vm.runInContext(fn + "\nresult = buildShareText();", ctx);
        return String(ctx.result);
    };
    for (const locale of locales) {
        const report = format(locale);
        assert.match(report, /\nPowered by Genycaster$/);
        assert.doesNotMatch(report, /\n\n/, locale + ": no full-height split blocks");
        assert.equal(report.split("\n").length, 7, locale + ": one multiline editor block");
        assert.ok(!report.includes("genycaster.stats."), locale + ": unresolved ID");
        assert.ok(Buffer.byteLength(report, "utf8") <= 1024, locale + ": exceeds ordinary Cast budget");
    }
    const english = format("en_US");
    assert.match(english, /105 casts · 206 replies written/);
    assert.match(english, /4118 interactions received/);
    assert.match(english, /3156 likes · 508 recasts/);
    assert.match(english, /As of November 8, 2026/);
    const persian = format("fa_IR");
    assert.match(persian, /۱۰۵ کست · ۲۰۶ پاسخ نوشته‌شده/);
    assert.match(persian, /۴۱۱۸ تعامل دریافتی/);
    assert.match(persian, /۱۸ آبان ۱۴۰۵/);
    assert.ok(!persian.includes("Casts published"));
    const partial = format("fa_IR", false);
    assert.ok(!partial.includes("تعامل دریافتی"));
    assert.ok(!partial.includes("پسند ·"));
    assert.equal(partial.split("\n").length, 4);
    assert.match(panel, /if \(section !== "social" \|\| !loaded\) return ""/);
    assert.match(panel, /LocaleFormat\.formatDateTime\(Date\.now\(\)\)/);
    assert.match(shell, /"structuredReport": Boolean\(structuredReport\)/);
    assert.match(shell, /intent\.structuredReport/);
    assert.match(composer, /property bool structuredReport: false/);
    assert.match(composer, /if \(root\.structuredReport\)\s*\n\s*richComposer\.markdownSource = root\.suggestedText\s*\n\s*else\s*\n\s*richComposer\.setBlockText\(0, root\.suggestedText\)/);
    assert.match(shell, /composerDrawer\.structuredReport = false/);
    // Ordinary compose uses literal text, so user-supplied Markdown is not reinterpreted.
    assert.match(shell, /onComposeRequested: text => root\.openComposer\(null, "", text\)/);
});


test("shared stats reports render as one multiline rich-text block without giant gaps", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    const composer = read("ui/components/composer/ComposerDrawer.qml");
    const richEditor = read("ui/components/composer/RichTextEditor.qml");
    const importer = read("src/application/rich_format.cpp");
    assert.match(panel, /return paragraphs\.join\("\\n"\)/);
    assert.doesNotMatch(panel, /return paragraphs\.join\("\\n\\n"\)/);
    assert.match(composer, /if \(root\.structuredReport\)\s*\n\s*richComposer\.markdownSource = root\.suggestedText/);
    // A paragraph is the maximal run of nonblank lines. This is what keeps
    // the visual ListView at one editor item instead of one 86px+ card per KPI.
    assert.match(importer, /if \(!paragraph\.empty\(\)\) paragraph\.push_back\('\\n'\)/);
    assert.match(importer, /document\.blocks\.push_back\(textBlock\(BlockKind::Paragraph, \+\+ordinal, paragraph\)\)/);
    assert.match(richEditor, /function escapeInlineHtml\(value\)/);
    assert.match(richEditor, /replace\(\/\\n\/g, "<br\/>"\)/);
    assert.match(richEditor, /if \(!values\.length\) return root\.escapeInlineHtml/);
    assert.match(richEditor, /const body = root\.escapeInlineHtml\(raw\)/);
    const mark = richEditor.match(/    function escapeHtml\(value\) \{([\s\S]*?)\n    \}/);
    const inline = richEditor.match(/    function escapeInlineHtml\(value\) \{([\s\S]*?)\n    \}/);
    assert.ok(mark && inline, "both HTML encoders must exist");
    const ctx = vm.createContext({root: {}});
    vm.runInContext("function escapeHtml(value) {" + mark[1] + "\n}" +
                    "\nfunction escapeInlineHtml(value) {" + inline[1] + "\n}" +
                    "\nroot.escapeHtml = escapeHtml;", ctx);
    assert.equal(ctx.escapeInlineHtml("A & B\nC < D"), "A &amp; B<br/>C &lt; D");
    assert.equal(ctx.escapeInlineHtml("First\r\nSecond"), "First<br/>Second");
    assert.equal(ctx.escapeInlineHtml("Line 1"), "Line 1");
});

test("Profile and Explorer share Stats desktop outer gutters without changing phone layout", () => {
    const shell = read("ui/components/shell/AppShell.qml");
    const profile = read("ui/components/profile/ProfileSurface.qml");
    const explorer = read("ui/components/feed/ExplorerFeedHeader.qml");
    assert.match(shell, /readonly property bool expandedSocialWorkspace: root\.desktop/);
    assert.match(shell, /presentationBridge\.activeView === "profile"/);
    assert.match(shell, /root\.primaryDestination === "explorer"/);
    assert.match(shell, /readonly property bool expandedWorkspace: root\.homeStatsWorkspace \|\| root\.expandedSocialWorkspace/);
    assert.match(shell, /anchors\.leftMargin: root\.wide && !root\.expandedWorkspace \? Theme\.space2 : 0/);
    assert.match(shell, /anchors\.rightMargin: root\.wide && !root\.expandedWorkspace \? Theme\.space2 : 0/);
    assert.match(shell, /spacing: root\.wide \? \(root\.expandedWorkspace \? Theme\.space1 : Theme\.space3\) : 0/);
    assert.match(shell, /Layout\.maximumWidth: root\.expandedSocialWorkspace\s*\n\s*&& presentationBridge\.activeView === "feed"\s*\n\s*\? root\.availableSocialWidth/);
    assert.match(shell, /Layout\.maximumWidth: root\.expandedSocialWorkspace\s*\n\s*\? root\.availableSocialWidth : root\.adaptiveProfileWidth/);
    assert.match(profile, /readonly property int profileMargin: root\.compact \? Theme\.mobileGutter : Theme\.space3/);
    assert.match(profile, /readonly property int profileContentLimit: root\.compact \? 760 : 1280/);
    assert.match(explorer, /readonly property int gutter: root\.compact \? Theme\.gutter\(root\.width\) : Theme\.space3/);
    assert.doesNotMatch(shell, /readonly property bool expandedSocialWorkspace: root\.mobile/);
});


test("long localized stats reports expand the visual editor to measured text and scroll the outer composer", () => {
    const editor = read("ui/components/composer/RichTextEditor.qml");
    const composer = read("ui/components/composer/ComposerDrawer.qml");
    const panel = read("ui/components/home/StatsPanel.qml");

    assert.match(editor, /property real singleParagraphContentHeight: 0/);
    assert.match(editor, /naturalSingleParagraphHeight: singleParagraphContentHeight/);
    assert.match(editor, /onContentHeightChanged:\s*\{\s*if \(root\.blockModel\.length === 1\)/);
    assert.match(editor, /singleParagraphContentHeight = Math\.max\(0, contentHeight\)/);
    assert.match(editor, /Component\.onCompleted:\s*\{\s*if \(root\.blockModel\.length === 1\)/);
    assert.match(composer, /root\.structuredReport && root\.editorMode === "visual"/);
    assert.match(composer, /Math\.max\(255, visualEditor\.naturalSingleParagraphHeight\)/);
    assert.match(composer, /id: composerScroll/);
    assert.match(composer, /Layout\.minimumHeight: 0\s*\n\s*clip: true/);
    assert.match(composer, /contentHeight: composerContent\.implicitHeight/);
    assert.match(composer, /ScrollBar\.vertical: GenyScrollBar \{\}/);
    assert.match(panel, /return paragraphs\.join\("\\n"\)/);
    assert.match(panel, /qsTrId\("genycaster\.stats\.share_engagement_line"\)/);
    assert.match(panel, /Powered by Genycaster/);
});

test("the shared report uses short, localized and accurately scoped lines", () => {
    const panel = read("ui/components/home/StatsPanel.qml");
    const keys = [
        "share_report_title", "share_engagement_line", "share_publishing_line",
        "share_reactions_line", "share_scope_note", "share_prepared"
    ];
    const langs = [
        "en_US","fa_IR","ar_SA","de_DE","es_ES","fr_FR",
        "hi_IN","ja_JP","ko_KR","pt_BR","ru_RU","zh_CN"
    ];
    for (const lang of langs) {
        const catalogue = read("ui/i18n/qml_" + lang + ".ts");
        for (const key of keys) {
            const id = "genycaster.stats." + key;
            const start = catalogue.indexOf('<message id="' + id + '">');
            assert.ok(start >= 0, lang + " missing " + key);
            const segment = catalogue.slice(start, catalogue.indexOf("</message>", start));
            const tr = segment.match(/<translation>([\s\S]*?)<\/translation>/)?.[1] || "";
            assert.ok(tr.length > 3, lang + " empty " + key);
            assert.ok(!tr.includes("genycaster.stats."), lang + " unresolved id");
            assert.ok(tr.length <= 105, lang + " excessively long " + key);
        }
    }
    assert.match(panel, /const heading = qsTrId\("genycaster.stats.share_report_title"\)/);
    assert.match(panel, /const paragraphs = \["\*\*" \+ heading \+ "\*\*"\]/);
    assert.match(panel, /if \(Boolean\(metrics\.hasEngagement\)\)\s*paragraphs\.push\("\*\*" \+ qsTrId\("genycaster.stats.share_engagement_line"\)/);
    assert.match(panel, /paragraphs\.push\(qsTrId\("genycaster.stats.share_publishing_line"\)/);
    assert.match(panel, /qsTrId\("genycaster.stats.share_scope_note"\)/);
    assert.match(panel, /LocaleFormat\.formatDateTime\(Date\.now\(\)\)/);
});

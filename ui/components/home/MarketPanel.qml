import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// First-party cached snapshots. Every plotted observation is supplied by CMC:
// no generated price curves, no client-side keys, no implied historical market
// cap series when the provider only supplies a latest global quote.
ColumnLayout {
    id: root
    objectName: "statsMarketPanel"
    Layout.fillWidth: true
    spacing: Theme.space3

    readonly property var metrics: marketController.metrics
    readonly property var globalData: metrics.global || ({})
    readonly property var sentiment: metrics.sentiment || ({})
    readonly property var season: metrics.season || ({})
    readonly property var benchmark: metrics.cmc100 || ({})
    readonly property var trend: metrics.cmc100Trend || ({})
    readonly property var prices: metrics.prices || ({})
    readonly property var assets: prices.assets || []
    readonly property bool hasGlobal: globalData.market_cap_usd !== undefined
    readonly property bool hasSentiment: sentiment.value !== undefined
    readonly property bool hasSeason: season.value !== undefined
    readonly property bool hasBenchmark: benchmark.value !== undefined
    readonly property bool initialLoading: marketController.state === "loading"
                                           && !hasGlobal && !hasSentiment
    readonly property bool wideLayout: width >= 720

    function sentimentLabel() {
        const classification = String(root.sentiment.label || "").trim().toLowerCase()
        switch (classification) {
        case "extreme fear": return qsTrId("genycaster.market.extreme_fear")
        case "fear": return qsTrId("genycaster.market.sentiment_fear")
        case "neutral": return qsTrId("genycaster.market.neutral")
        case "greed": return qsTrId("genycaster.market.greed")
        case "extreme greed": return qsTrId("genycaster.market.extreme_greed")
        default: return String(root.sentiment.label || "")
        }
    }
    // Human-readable asset names are localized; ticker symbols stay canonical.
    function assetName(symbol) {
        switch (String(symbol || "").toUpperCase()) {
        case "BTC": return qsTrId("genycaster.market.asset_bitcoin")
        case "ETH": return qsTrId("genycaster.market.asset_ethereum")
        case "SOL": return qsTrId("genycaster.market.asset_solana")
        default: return String(symbol || "")
        }
    }
    function compact(value) {
        const n = Number(value)
        if (!Number.isFinite(n) || n < 0) return "—"
        // Isolate currency glyphs and digits from the surrounding RTL unit.
        // A word boundary permits a natural second line on narrow cards.
        if (n >= 1e12) return "\u2066$" + LocaleFormat.formatNumber(n/1e12,2) + "\u2069 " + qsTrId("genycaster.market.unit_trillion")
        if (n >= 1e9) return "\u2066$" + LocaleFormat.formatNumber(n/1e9,2) + "\u2069 " + qsTrId("genycaster.market.unit_billion")
        if (n >= 1e6) return "\u2066$" + LocaleFormat.formatNumber(n/1e6,2) + "\u2069 " + qsTrId("genycaster.market.unit_million")
        return "\u2066$" + LocaleFormat.formatNumber(n,2) + "\u2069"
    }
    function percentage(value) {
        const n = Number(value)
        return Number.isFinite(n) && n >= 0 && n <= 100
            ? LocaleFormat.localizeDigits(LocaleFormat.formatNumber(n,2)) + "%" : "—"
    }
    function price(value) {
        const n = Number(value)
        return Number.isFinite(n) && n >= 0
            ? "$" + LocaleFormat.localizeDigits(LocaleFormat.formatNumber(n,2)) : "—"
    }
    function dateLabel(timestamp) {
        const parsed = Date.parse(String(timestamp || ""))
        return Number.isFinite(parsed) ? LocaleFormat.localizeDigits(
            new Date(parsed).toLocaleDateString(Qt.locale(localizationController.effectiveLanguageTag), "MMM d")) : ""
    }

    RowLayout {
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2

        Icon {
            text: Theme.iconChart
            preferredSize: Theme.iconSizeLarge
            color: Theme.textSecondary
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.market.overview_title")
            color: Theme.textPrimary
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
        }
        DirectionAwareLabel {
            text: qsTrId("genycaster.market.spot")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
    }

    DirectionAwareLabel {
        visible: marketController.partial || marketController.state === "error"
        Layout.fillWidth: true
        text: qsTrId("genycaster.market.partial")
        font.pixelSize: Theme.typeCaptionSize
        color: Theme.textSecondary
        wrapMode: Text.Wrap
    }

    StatsSkeleton {
        visible: root.initialLoading
        Layout.fillWidth: true
    }

    // Flexible cards replace a fixed-width scrolling strip. All three assets
    // remain visible at normal desktop width; they stack at phone widths.
    GridLayout {
        id: tokenGrid
        visible: root.assets.length > 0
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        columns: root.width >= 680 ? 3 : root.width >= 420 ? 2 : 1
        columnSpacing: Theme.space2
        rowSpacing: Theme.space2

        Repeater {
            model: root.assets
            delegate: Superellipse {
                id: assetCard
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                Layout.preferredHeight: Math.max(106, tokenContent.implicitHeight + Theme.space3 * 2)
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: tokenContent
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            alignToUi: true
                            text: String(assetCard.modelData.symbol)
                            forceLeftToRight: true
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            color: Theme.textSecondary
                        }
                        DirectionAwareLabel {
                            text: Number(assetCard.modelData.change_24h_percent) < 0 ? "▼" : "▲"
                            font.pixelSize: Theme.typeCaptionSize
                            color: Number(assetCard.modelData.change_24h_percent) < 0
                                ? (Theme.dark ? "#FF9EA4" : "#DB4853")
                                : (Theme.dark ? "#6FDEA4" : "#168F6A")
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: root.assetName(assetCard.modelData.symbol)
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        forceLeftToRight: true
                        text: root.price(assetCard.modelData.price_usd)
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: qsTrId("genycaster.market.change_24h").arg(
                            (Number(assetCard.modelData.change_24h_percent) >= 0 ? "+" : "−")
                            + LocaleFormat.formatNumber(
                                Math.abs(Number(assetCard.modelData.change_24h_percent)), 2))
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                        color: Number(assetCard.modelData.change_24h_percent) < 0
                            ? (Theme.dark ? "#FF9EA4" : "#DB4853")
                            : (Theme.dark ? "#6FDEA4" : "#168F6A")
                    }
                }
            }
        }
    }

    GridLayout {
        visible: !root.initialLoading
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        columns: root.width >= 450 ? 2 : 1
        columnSpacing: Theme.space2
        rowSpacing: Theme.space2

        MarketKpi {
            caption: qsTrId("genycaster.market.marketcap")
            value: root.hasGlobal ? root.compact(root.globalData.market_cap_usd) : "—"
            description: qsTrId("genycaster.market.total_market")
            iconName: Theme.iconChart
        }
        MarketKpi {
            caption: qsTrId("genycaster.market.volume")
            value: root.hasGlobal ? root.compact(root.globalData.volume_24h_usd) : "—"
            description: qsTrId("genycaster.market.last_24h")
            iconName: Theme.iconEnergy
        }
    }

    // Desktop: sentiment and CMC100 share the first row. Phone: cards
    // stack at a readable width; RTL reverses columns, not time-series data.
    GridLayout {
        id: marketInsightGrid
        visible: !root.initialLoading
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        columns: root.wideLayout ? 2 : 1
        columnSpacing: Theme.space3
        rowSpacing: Theme.space3

        NeonSurface {
            interactive: true
            ambientNeon: true
            neonStrength: Theme.neonCardStrength
            Layout.row: 0
            Layout.column: 0
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            id: fearCard
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(root.wideLayout ? 262 : 228, fearContent.implicitHeight + Theme.space4 * 2)
            fillColor: Theme.surface
            borderColor: Theme.borderMuted
            borderWidth: 1
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: fearContent
                anchors.fill: parent
                anchors.margins: Theme.space4
                spacing: Theme.space2
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Icon {
                        text: Theme.iconHeart
                        preferredSize: Theme.iconSizeLarge
                        color: Theme.textSecondary
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.market.fear")
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.formatInteger(0) + "–" + LocaleFormat.formatInteger(100)
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
                MarketFearGauge {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    index: root.hasSentiment ? Number(root.sentiment.value) : -1
                    classification: root.sentimentLabel()
                }
            }
        }

        MarketTrend {
            Layout.row: root.wideLayout ? 0 : 2
            Layout.column: root.wideLayout ? 1 : 0
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            Layout.fillWidth: true
            Layout.preferredHeight: 262
            series: root.trend.values || []
            startLabel: root.dateLabel(root.trend.from)
            endLabel: root.dateLabel(root.trend.to)
            latest: root.hasBenchmark ? Number(root.benchmark.value) : -1
            change: root.hasBenchmark ? Number(root.benchmark.change_24h_percent) : 0
        }

        NeonSurface {
            interactive: true
            ambientNeon: true
            neonStrength: Theme.neonCardStrength
            Layout.row: 1
            Layout.column: 0
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(196, seasonContent.implicitHeight + Theme.space4 * 2)
            fillColor: Theme.surface
            borderColor: Theme.borderMuted
            borderWidth: 1
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: seasonContent
                anchors.fill: parent
                anchors.margins: Theme.space4
                spacing: Theme.space2
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    Icon { text: Theme.iconEnergy; preferredSize: Theme.iconSizeLarge; color: Theme.textSecondary }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.market.season")
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.hasSeason
                        ? LocaleFormat.formatInteger(Math.round(Number(root.season.value)))
                          + "/" + LocaleFormat.formatInteger(100) : "—"
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                MarketSeasonMeter {
                    Layout.fillWidth: true
                    index: root.hasSeason ? Number(root.season.value) : -1
                }
            }
        }

        NeonSurface {
            interactive: true
            ambientNeon: true
            neonStrength: Theme.neonCardStrength
            Layout.row: root.wideLayout ? 1 : 3
            Layout.column: root.wideLayout ? 1 : 0
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(196, dominanceContent.implicitHeight + Theme.space4 * 2)
            fillColor: Theme.surface
            borderColor: Theme.borderMuted
            borderWidth: 1
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: dominanceContent
                anchors.fill: parent
                anchors.margins: Theme.space4
                spacing: Theme.space3
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    Icon {
                        text: Theme.iconChart
                        color: Theme.textSecondary
                        preferredSize: Theme.iconSizeLarge
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.market.dominance")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                }
                Repeater {
                    model: [
                        {name:root.assetName("BTC") + " (\u2066BTC\u2069)",percentage:root.globalData.btc_dominance,fill:Theme.accent},
                        {name:root.assetName("ETH") + " (\u2066ETH\u2069)",percentage:root.globalData.eth_dominance,fill:Theme.textSecondary}
                    ]
                    delegate: ColumnLayout {
                        id: dominanceRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                alignToUi: true
                                text: dominanceRow.modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareLabel {
                                text: root.hasGlobal ? root.percentage(dominanceRow.modelData.percentage) : "—"
                                font.pixelSize: Theme.typeCaptionSize
                                color: Theme.textPrimary
                                font.weight: Theme.weightDemiBold
                            }
                        }
                        Rectangle {
                            id: dominanceTrack
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Theme.surfaceMuted
                            Rectangle {
                                anchors.left: Theme.rtl ? undefined : parent.left
                                anchors.right: Theme.rtl ? parent.right : undefined
                                height: parent.height
                                width: root.hasGlobal ? dominanceTrack.width
                                    * Math.max(0, Math.min(100,Number(dominanceRow.modelData.percentage))) / 100 : 0
                                radius: parent.radius
                                color: dominanceRow.modelData.fill
                                Behavior on width {
                                    enabled: !Theme.reduceMotion
                                    NumberAnimation { duration: 420; easing.type: Easing.OutCubic }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        spacing: Theme.space3

        DirectionAwareLabel {
            id: attributionLabel
            Layout.fillWidth: true
            readonly property bool usesAlternative: root.sentiment.source === "Alternative.me"
            text: usesAlternative
                ? qsTrId("genycaster.market.source_alternative")
                : qsTrId("genycaster.market.source_cmc")
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Link

            TapHandler {
                onTapped: Qt.openUrlExternally(attributionLabel.usesAlternative
                   ? "https://alternative.me/crypto/fear-and-greed-index/"
                   : "https://coinmarketcap.com/")
            }
        }
        DirectionAwareLabel {
            text: marketController.updatedAt.length > 0
                ? qsTrId("genycaster.market.last_checked").arg(
                    LocaleFormat.localizeDigits(marketController.updatedAt)) : ""
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
    }
}

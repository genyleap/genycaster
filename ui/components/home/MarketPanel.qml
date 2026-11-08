import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Market snapshots are always provided by the first-party server. The QML
// surface does not contact third-party providers or own a client API key.
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
    readonly property bool busy: marketController.state === "loading"
    readonly property bool hasGlobal: globalData.market_cap_usd !== undefined
    readonly property bool hasSentiment: sentiment.value !== undefined
    readonly property bool hasSeason: season.value !== undefined
    readonly property bool hasBenchmark: benchmark.value !== undefined

    function compact(value) {
        const n=Number(value)
        if (!isFinite(n) || n<0) return "—"
        if (n>=1e12) return "$"+(n/1e12).toLocaleString(Qt.locale(), "f", 2)+"T"
        if (n>=1e9) return "$"+(n/1e9).toLocaleString(Qt.locale(), "f", 2)+"B"
        if (n>=1e6) return "$"+(n/1e6).toLocaleString(Qt.locale(), "f", 2)+"M"
        return "$"+n.toLocaleString(Qt.locale(), "f", 2)
    }
    function percentage(value) {
        const n=Number(value)
        return isFinite(n) && n>=0 && n<=100
            ? n.toLocaleString(Qt.locale(), "f", 2)+"%" : "—"
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        Icon { text: Theme.iconChart; preferredSize: Theme.iconSizeLarge; color: Theme.textSecondary }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.market.title")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.market.subtitle")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: marketController.partial || marketController.state === "error"
        text: marketController.planConfigured
            ? qsTrId("genycaster.market.partial")
            : qsTrId("genycaster.market.unconfigured")
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
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

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: dominanceLayout.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surface
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer

        ColumnLayout {
            id: dominanceLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space4
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                Icon { text: Theme.iconChart; preferredSize: Theme.iconSize; color: Theme.textSecondary }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.market.dominance")
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.textPrimary
                }
            }
            Repeater {
                model: [
                    {name:"Bitcoin (BTC)",percent:root.globalData.btc_dominance,
                     fill:Theme.accent},
                    {name:"Ethereum (ETH)",percent:root.globalData.eth_dominance,
                     fill:Theme.textSecondary}
                ]
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    RowLayout {
                        Layout.fillWidth: true
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeMetaSize
                        }
                        DirectionAwareLabel {
                            text: root.hasGlobal ? root.percentage(modelData.percent) : "—"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 7
                        radius: 3.5
                        color: Theme.surfaceMuted
                        Rectangle {
                            width: root.hasGlobal ? parent.width * Math.max(0,
                                Math.min(100,Number(modelData.percent)))/100 : 0
                            height: parent.height
                            radius: parent.radius
                            color: modelData.fill
                            Behavior on width {
                                enabled: !Theme.reduceMotion
                                NumberAnimation { duration: 440; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: Theme.space2
        rowSpacing: Theme.space2

        MarketKpi {
            caption: qsTrId("genycaster.market.fear")
            value: root.hasSentiment ? String(root.sentiment.value)+"/100" : "—"
            description: root.hasSentiment ? String(root.sentiment.label) : qsTrId("genycaster.market.unavailable")
            iconName: Theme.iconHeart
        }
        MarketKpi {
            caption: qsTrId("genycaster.market.season")
            value: root.hasSeason ? String(root.season.value)+"/100" : "—"
            description: qsTrId("genycaster.market.altseason")
            iconName: Theme.iconEnergy
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: benchmarkColumn.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surface
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer
        ColumnLayout {
            id: benchmarkColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space4
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                Icon { text: Theme.iconChart; preferredSize: Theme.iconSize; color: Theme.textSecondary }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.market.cmc100")
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.textPrimary
                }
            }
            RowLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.hasBenchmark
                        ? Number(root.benchmark.value).toLocaleString(Qt.locale(), "f", 2) : "—"
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                    color: Theme.textPrimary
                }
                DirectionAwareLabel {
                    visible: root.hasBenchmark
                    text: (Number(root.benchmark.change_24h_percent)>=0?"+":"")
                        + Number(root.benchmark.change_24h_percent).toLocaleString(Qt.locale(),"f",2)+"%"
                    font.pixelSize: Theme.typeMetaSize
                    color: Theme.textSecondary
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2
        DirectionAwareLabel {
            id: attributionLabel
            Layout.fillWidth: true
            readonly property bool usesAlternative: root.sentiment.source === "Alternative.me"
            text: usesAlternative && !marketController.planConfigured
                ? qsTrId("genycaster.market.source_alternative")
                : qsTrId("genycaster.market.source_cmc")
            color: Theme.accent
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Link
            TapHandler {
                onTapped: Qt.openUrlExternally(attributionLabel.usesAlternative
                    ? "https://alternative.me/crypto/fear-and-greed-index/"
                    : "https://coinmarketcap.com/")
            }
        }
        DirectionAwareLabel {
            text: marketController.updatedAt
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
    }
}

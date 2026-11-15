import QtQuick
import QtQuick.Layouts
import Farcaster.Client

NeonSurface {
    id: root
    interactive: true
    ambientNeon: true
    neonStrength: Theme.neonCardStrength
    property var series: []
    property string startLabel: ""
    property string endLabel: ""
    property real latest: -1
    property real change: 0
    readonly property bool validSeries: series && series.length >= 2
    readonly property bool validLatest: Number.isFinite(latest) && latest >= 0
    readonly property bool declining: Number(change) < 0
    readonly property color trendColor: declining ? (Theme.dark ? "#FF9299" : "#E64B57")
                                                   : (Theme.dark ? "#63D8A4" : "#1BAC7F")

    Layout.fillWidth: true
    Layout.preferredHeight: 262
    fillColor: Theme.surface
    borderColor: Theme.borderMuted
    borderWidth: 1
    curveDepth: Theme.curveContainer

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Icon { text: Theme.iconChart; preferredSize: Theme.iconSizeLarge; color: Theme.textSecondary }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.market.cmc100")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: qsTrId("genycaster.market.ten_day_trend")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space3
            DirectionAwareLabel {
                forceLeftToRight: true
                text: root.validLatest ? LocaleFormat.localizeDigits(LocaleFormat.formatNumber(Number(root.latest), 2)) : "—"
                font.pixelSize: Theme.typeTitle1Size
                font.weight: Theme.weightBold
                color: Theme.textPrimary
            }
            Superellipse {
                visible: root.validLatest
                Layout.preferredWidth: changeText.implicitWidth + Theme.space3 * 2
                Layout.preferredHeight: 28
                fillColor: root.declining ? (Theme.dark ? "#3D232A" : "#FFF0F1")
                                          : (Theme.dark ? "#1D3B31" : "#E9F8F0")
                curveDepth: 12
                DirectionAwareLabel {
                    id: changeText
                    anchors.centerIn: parent
                    forceLeftToRight: true
                    text: (root.declining ? "▼ " : "▲ ")
                          + LocaleFormat.localizeDigits(LocaleFormat.formatNumber(Math.abs(Number(root.change)), 2)) + "%"
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    color: root.trendColor
                }
            }
            Item { Layout.fillWidth: true }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 95

            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    x: 0
                    y: index * (parent.height - 1) / 2
                    width: parent.width
                    height: 1
                    color: Theme.borderMuted
                    opacity: 0.7
                }
            }

            TokenSparkline {
                id: marketLine
                anchors.fill: parent
                visible: root.validSeries
                smoothCurve: true
                transform: Scale {
                    origin.x: marketLine.width / 2
                    xScale: Theme.rtl ? -1 : 1
                }
                samples: root.series
                strokeWidth: 2.4
                strokeColor: root.trendColor
                contentPadding: 4
                fillArea: true
            }

            DirectionAwareLabel {
                anchors.centerIn: parent
                visible: !root.validSeries
                text: qsTrId("genycaster.market.history_unavailable")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                alignToUi: true
                text: root.validSeries ? root.startLabel : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
            DirectionAwareLabel {
                alignToUi: true
                text: root.validSeries ? root.endLabel : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
    }
}

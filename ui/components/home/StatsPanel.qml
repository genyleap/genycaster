import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Read-only presentation. The controller owns provider requests, aggregation,
// cancellation and completeness semantics; this component renders no sample data.
ColumnLayout {
    id: root
    objectName: "homeStatsPanel"
    spacing: Theme.space3

    property int periodDays: 30
    property bool compact: false
    readonly property var metrics: statsController.summary
    readonly property bool loaded: statsController.state === "ready"
                                    || statsController.state === "partial"
    readonly property bool partial: statsController.state === "partial"
    readonly property var bars: statsController.activity
    readonly property int maxCount: {
        let high = 1
        for (let i = 0; i < bars.length; ++i) {
            const sum = Number(bars[i].casts || 0) + Number(bars[i].replies || 0)
            high = Math.max(high, sum)
        }
        return high
    }
    signal periodRequested(int days)
    signal refreshRequested()
    signal castRequested(string hash)

    RowLayout {
        Layout.fillWidth: true
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.your_stats")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
            }
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.subtitle")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }
        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.stats.refresh")
            enabled: statsController.state !== "loading"
            onClicked: root.refreshRequested()
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: 49
        fillColor: Theme.surfaceMuted
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveControl
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2
            layoutDirection: Theme.uiDirection
            Repeater {
                model: [
                    {days: 7, title: "7D"},
                    {days: 30, title: "30D"},
                    {days: 90, title: "90D"},
                    {days: 0, title: qsTrId("genycaster.stats.all")}
                ]
                delegate: GenyTabButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    minimumWidth: 0
                    dense: true
                    selected: root.periodDays === modelData.days
                    text: modelData.title
                    Accessible.name: qsTrId("genycaster.stats.last_n_days").arg(modelData.days)
                    onClicked: root.periodRequested(modelData.days)
                }
            }
        }
    }

    DirectionAwareLabel {
        Layout.fillWidth: true
        visible: statsController.state === "loading"
        text: qsTrId("genycaster.stats.loading").arg(statsController.loadedCount)
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
    }
    Superellipse {
        visible: statsController.state === "error" || statsController.state === "idle"
        Layout.fillWidth: true
        Layout.preferredHeight: message.implicitHeight + Theme.space4 * 2
        fillColor: Theme.surface
        borderColor: Theme.borderMuted
        borderWidth: 1
        curveDepth: Theme.curveContainer
        DirectionAwareLabel {
            id: message
            anchors.fill: parent
            anchors.margins: Theme.space4
            text: statsController.state === "error"
                  ? qsTrId("genycaster.stats.node_unavailable")
                    .arg(String(root.metrics.error || ""))
                  : qsTrId("genycaster.stats.select_to_load")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeMetaSize
            wrapMode: Text.Wrap
        }
    }
    DirectionAwareLabel {
        visible: root.partial
        Layout.fillWidth: true
        text: qsTrId("genycaster.stats.partial")
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }
    DirectionAwareLabel {
        visible: root.loaded
        Layout.fillWidth: true
        text: qsTrId("genycaster.stats.period_note")
        color: Theme.textSecondary
        font.pixelSize: Theme.typeCaptionSize
        wrapMode: Text.Wrap
    }

    component StatTile: Superellipse {
        property string title: ""
        property string value: "—"
        property string footnote: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        Layout.preferredHeight: 108
        fillColor: Theme.surface
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: parent.parent.title
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: parent.parent.value
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle1Size
                font.weight: Theme.weightBold
                elide: Text.ElideRight
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: parent.parent.footnote
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
        }
    }
    GridLayout {
        Layout.fillWidth: true
        visible: root.loaded
        columns: root.compact || root.width < 600 ? 2 : 4
        columnSpacing: Theme.space2
        rowSpacing: Theme.space2
        StatTile {
            title: qsTrId("genycaster.stats.casts")
            value: String(root.metrics.casts ?? "—")
            footnote: qsTrId("genycaster.stats.published_posts")
        }
        StatTile {
            title: qsTrId("genycaster.stats.replies")
            value: String(root.metrics.replies ?? "—")
            footnote: qsTrId("genycaster.stats.written_replies")
        }
        StatTile {
            title: qsTrId("genycaster.stats.engagement")
            value: root.metrics.hasEngagement ? Number(root.metrics.engagement).toLocaleString(Qt.locale()) : "—"
            footnote: qsTrId("genycaster.stats.received_on_posts")
        }
        StatTile {
            title: qsTrId("genycaster.stats.followers")
            value: root.metrics.hasAudience ? Number(root.metrics.followers).toLocaleString(Qt.locale()) : "—"
            footnote: qsTrId("genycaster.stats.current_total")
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: 244
        visible: root.loaded
        fillColor: Theme.surface
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stats.publishing_activity")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                Rectangle { width: 8; height: 8; radius: 4; color: Theme.accent }
                DirectionAwareLabel {
                    text: qsTrId("genycaster.stats.casts")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                }
                Rectangle { width: 8; height: 8; radius: 4; color: Theme.textTertiary }
                DirectionAwareLabel {
                    text: qsTrId("genycaster.stats.replies")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            Item {
                id: chartArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Row {
                    anchors.fill: parent
                    spacing: 3
                    Repeater {
                        model: root.bars
                        delegate: Item {
                            required property var modelData
                            width: Math.max(1, (chartArea.width - 3 * Math.max(0, root.bars.length - 1))
                                              / Math.max(1, root.bars.length))
                            height: chartArea.height
                            readonly property real total: Number(modelData.casts || 0)
                                                           + Number(modelData.replies || 0)
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.max(2, parent.width - 1)
                                height: Math.max(0, parent.height * parent.total / root.maxCount)
                                radius: Math.min(3, width / 2)
                                color: Theme.textTertiary
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.max(2, parent.width - 1)
                                height: Math.max(0, parent.height * Number(parent.modelData.casts || 0)
                                                          / root.maxCount)
                                radius: Math.min(3, width / 2)
                                color: Theme.accent
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.bars.length > 0 ? root.bars[0].label : ""
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    text: root.periodDays === 0 ? qsTrId("genycaster.stats.last_90_days") : qsTrId("genycaster.stats.today")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: topColumn.implicitHeight + Theme.space4 * 2
        visible: root.loaded && root.metrics.hasEngagement
        fillColor: Theme.surface
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer

        ColumnLayout {
            id: topColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space4
            spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.stats.top_casts")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                visible: statsController.topCasts.length === 0
                text: qsTrId("genycaster.stats.no_posts")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
            }
            Repeater {
                model: statsController.topCasts
                delegate: Item {
                    id: bestCast
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(58, bestContent.implicitHeight + Theme.space2 * 2)
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: String(modelData.text || qsTrId("genycaster.stats.open_cast"))
                    Accessible.onPressAction: root.castRequested(modelData.hash)
                    Keys.onReturnPressed: root.castRequested(modelData.hash)
                    Keys.onSpacePressed: root.castRequested(modelData.hash)

                    ColumnLayout {
                        id: bestContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(bestCast.modelData.text || qsTrId("genycaster.stats.media_cast"))
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            color: castHover.hovered ? Theme.accent : Theme.textPrimary
                            font.pixelSize: Theme.typeMetaSize
                        }
                        DirectionAwareLabel {
                            text: qsTrId("genycaster.stats.cast_engagement").arg(
                                bestCast.modelData.likes).arg(bestCast.modelData.recasts)
                                .arg(bestCast.modelData.replies)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    HoverHandler { id: castHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.castRequested(bestCast.modelData.hash) }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.borderMuted
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.loaded
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.metrics.hasEngagement
                ? qsTrId("genycaster.stats.average").arg(
                    Number(root.metrics.average || 0).toLocaleString(Qt.locale(), "f", 1))
                : qsTrId("genycaster.stats.engagement_unavailable")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            text: statsController.updatedAt
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    objectName: "statsPublishingChart"
    property var bars: []
    property int periodDays: 30
    property int selectedIndex: -1
    property real reveal: 1
    readonly property int maxCount: {
        let n = 1
        for (let i = 0; i < bars.length; ++i)
            n = Math.max(n, Number(bars[i].casts || 0) + Number(bars[i].replies || 0))
        return n
    }
    readonly property var selected: selectedIndex >= 0 && selectedIndex < bars.length
                                    ? bars[selectedIndex] : null

    onBarsChanged: {
        selectedIndex = -1
        revealTween.stop()
        if (Theme.reduceMotion) {
            reveal = 1
        } else {
            reveal = 0
            revealTween.start()
        }
    }

    NumberAnimation {
        id: revealTween
        target: root
        property: "reveal"
        to: 1
        duration: Theme.reduceMotion ? 0 : 570
        easing.type: Easing.OutCubic
    }

    Layout.fillWidth: true
    Layout.preferredHeight: 270
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
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.stats.publishing_activity")
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                color: Theme.textPrimary
            }
            Rectangle { width: 7; height: 7; radius: 3.5; color: Theme.accent }
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.casts")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
            }
            Rectangle { width: 7; height: 7; radius: 3.5; color: Theme.textTertiary }
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.replies")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space2

            Item {
                Layout.preferredWidth: 26
                Layout.fillHeight: true
                DirectionAwareLabel {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    text: String(root.maxCount)
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
                DirectionAwareLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    text: String(Math.round(root.maxCount / 2))
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
                DirectionAwareLabel {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    text: "0"
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
            }

            Item {
                id: plot
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Repeater {
                    model: 3
                    delegate: Rectangle {
                        required property int index
                        x: 0
                        y: index * (plot.height - 1) / 2
                        width: plot.width
                        height: 1
                        color: Theme.borderMuted
                        opacity: 0.72
                    }
                }

                Row {
                    anchors.fill: parent
                    spacing: 2
                    Repeater {
                        model: root.bars
                        delegate: Item {
                            id: column
                            required property int index
                            required property var modelData
                            readonly property int casts: Number(modelData.casts || 0)
                            readonly property int replies: Number(modelData.replies || 0)
                            readonly property real barWidth: Math.max(2, Math.min(26, width - 2))
                            readonly property real castHeight: plot.height * casts / root.maxCount * root.reveal
                            readonly property real replyHeight: plot.height * replies / root.maxCount * root.reveal
                            width: Math.max(1, (plot.width - 2 * Math.max(0, root.bars.length - 1))
                                              / Math.max(1, root.bars.length))
                            height: plot.height
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTrId("genycaster.stats.chart_point")
                                .arg(String(modelData.label))
                                .arg(Number(modelData.casts).toLocaleString(Qt.locale(), "f", 0))
                                .arg(Number(modelData.replies).toLocaleString(Qt.locale(), "f", 0))
                            Accessible.onPressAction: root.selectedIndex = index

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: plot.height - column.castHeight - column.replyHeight
                                width: column.barWidth
                                height: Math.max(0, column.replyHeight)
                                radius: Math.min(3, width / 2)
                                color: Theme.textTertiary
                                opacity: root.selectedIndex === -1 || root.selectedIndex === column.index ? 1 : 0.55
                            }
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: column.barWidth
                                height: Math.max(0, column.castHeight)
                                radius: Math.min(3, width / 2)
                                color: Theme.accent
                                opacity: root.selectedIndex === -1 || root.selectedIndex === column.index ? 1 : 0.55
                            }
                            TapHandler { onTapped: root.selectedIndex = column.index }
                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                                onHoveredChanged: if (hovered) root.selectedIndex = column.index
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.bars.length ? String(root.bars[0].label) : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
            DirectionAwareLabel {
                text: root.periodDays === 0
                    ? qsTrId("genycaster.stats.last_90_days")
                    : qsTrId("genycaster.stats.today")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.selected
                ? qsTrId("genycaster.stats.chart_point")
                    .arg(String(root.selected.label))
                    .arg(Number(root.selected.casts).toLocaleString(Qt.locale(), "f", 0))
                    .arg(Number(root.selected.replies).toLocaleString(Qt.locale(), "f", 0))
                : qsTrId("genycaster.stats.chart_hint")
            color: root.selected ? Theme.textPrimary : Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }
}

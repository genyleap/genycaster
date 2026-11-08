import QtQuick
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    objectName: "statsEngagementBreakdown"
    property var metrics: ({})
    readonly property real total: Number(metrics.engagement || 0)
    readonly property var categories: [
        {label: qsTrId("genycaster.stats.likes"), count: Number(metrics.likes || 0), color: Theme.accent},
        {label: qsTrId("genycaster.stats.recasts"), count: Number(metrics.recasts || 0), color: Theme.textSecondary},
        {label: qsTrId("genycaster.stats.received_replies"), count: Number(metrics.receivedReplies || 0), color: Theme.textTertiary}
    ]

    Layout.fillWidth: true
    Layout.preferredHeight: content.implicitHeight + Theme.space4 * 2
    fillColor: Theme.surface
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space4
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.stats.engagement_breakdown")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: Number(root.total).toLocaleString(Qt.locale(), "f", 0)
                color: Theme.textSecondary
                font.pixelSize: Theme.typeMetaSize
            }
        }
        Repeater {
            model: root.categories
            delegate: ColumnLayout {
                id: categoryRow
                required property var modelData
                Layout.fillWidth: true
                spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: categoryRow.modelData.label
                        font.pixelSize: Theme.typeMetaSize
                        color: Theme.textSecondary
                    }
                    DirectionAwareLabel {
                        text: Number(categoryRow.modelData.count).toLocaleString(Qt.locale(), "f", 0)
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                }
                Rectangle {
                    id: track
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    color: Theme.surfaceMuted
                    radius: 3
                    Rectangle {
                        id: fill
                        width: root.total > 0
                            ? track.width * Math.min(1, categoryRow.modelData.count / root.total) : 0
                        height: track.height
                        radius: 3
                        color: categoryRow.modelData.color
                        Behavior on width {
                            enabled: !Theme.reduceMotion
                            NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderMuted
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.stats.average").arg(
                Number(root.metrics.average || 0).toLocaleString(Qt.locale(), "f", 1))
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }
}

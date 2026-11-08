import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Engagement is received feedback, while authored casts and replies are
// publishing activity. These two populations are intentionally separate.
// A percentage-of-total breakdown is more accurate than a shared-scale radar.
Superellipse {
    id: root
    objectName: "statsEngagementBreakdown"
    property var metrics: ({})

    readonly property var engagementRows: [
        { label: qsTrId("genycaster.stats.likes"), count: Math.max(0, Number(metrics.likes || 0)) },
        { label: qsTrId("genycaster.stats.recasts"), count: Math.max(0, Number(metrics.recasts || 0)) },
        { label: qsTrId("genycaster.stats.received_replies"), count: Math.max(0, Number(metrics.receivedReplies || 0)) }
    ]
    readonly property var publishingRows: [
        { label: qsTrId("genycaster.stats.casts"), count: Math.max(0, Number(metrics.casts || 0)) },
        { label: qsTrId("genycaster.stats.replies"), count: Math.max(0, Number(metrics.replies || 0)) }
    ]
    readonly property real receivedTotal: engagementRows.reduce(
        (sum, entry) => sum + entry.count, 0)
    readonly property string percentSign: Theme.rtl ? "٪" : "%"
    readonly property var segmentColors: [
        Theme.accent, Qt.lighter(Theme.accent, Theme.dark ? 1.38 : 1.48),
        Qt.lighter(Theme.accent, Theme.dark ? 1.76 : 1.92)
    ]

    function share(count) {
        return receivedTotal > 0 ? Math.max(0, Math.min(1, count / receivedTotal)) : 0
    }

    Layout.fillWidth: true
    Layout.minimumWidth: 0
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
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Icon {
                text: Theme.iconHeart
                color: Theme.textSecondary
                preferredSize: Theme.iconSizeSmall
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                alignToUi: true
                text: qsTrId("genycaster.stats.engagement_breakdown")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }
            DirectionAwareLabel {
                text: LocaleFormat.formatInteger(root.receivedTotal)
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightBold
                Accessible.name: qsTrId("genycaster.stats.engagement") + ": "
                                 + LocaleFormat.formatInteger(root.receivedTotal)
            }
        }

        // A single proportionally segmented strip gives the composition at a
        // glance. No overlap or translucent stacking, even with one category
        // much larger than the others.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            Rectangle {
                id: distributionTrack
                anchors.fill: parent
                radius: height / 2
                color: Theme.surfaceMuted
                clip: true

                Row {
                    anchors.fill: parent
                    layoutDirection: Theme.uiDirection
                    spacing: 0
                    Repeater {
                        model: root.engagementRows
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            height: distributionTrack.height
                            width: distributionTrack.width * root.share(modelData.count)
                            color: root.segmentColors[index]
                            antialiasing: true
                            Accessible.role: Accessible.Indicator
                            Accessible.name: modelData.label + ": "
                                             + LocaleFormat.formatInteger(modelData.count)
                        }
                    }
                }
            }
        }

        Repeater {
            model: root.engagementRows
            delegate: ColumnLayout {
                id: engagementRow
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space1

                readonly property real fraction: root.share(modelData.count)

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Rectangle {
                        width: 8
                        height: width
                        radius: width / 2
                        color: root.segmentColors[engagementRow.index]
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: engagementRow.modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.formatInteger(engagementRow.modelData.count)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        Layout.preferredWidth: 54
                        horizontalAlignment: Text.AlignRight
                        forceLeftToRight: true
                        text: LocaleFormat.formatNumber(engagementRow.fraction * 100, 1) + root.percentSign
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }

                Rectangle {
                    id: rowTrack
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: height / 2
                    color: Theme.surfaceMuted

                    Rectangle {
                        width: rowTrack.width * engagementRow.fraction
                        height: parent.height
                        x: Theme.rtl ? rowTrack.width - width : 0
                        radius: Math.min(width / 2, height / 2)
                        color: root.segmentColors[engagementRow.index]
                        visible: width > 0
                        Behavior on width {
                            enabled: !Theme.reduceMotion
                            NumberAnimation { duration: 360; easing.type: Easing.OutCubic }
                        }
                    }
                }
                Accessible.role: Accessible.StaticText
                Accessible.name: engagementRow.modelData.label + ": "
                                 + LocaleFormat.formatInteger(engagementRow.modelData.count)
                                 + " (" + LocaleFormat.formatNumber(engagementRow.fraction * 100, 1) + "%)"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderMuted
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: qsTrId("genycaster.stats.publishing_activity")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightDemiBold
        }
        GridLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            columns: root.width < 360 ? 1 : 2
            columnSpacing: Theme.space3
            rowSpacing: Theme.space2
            Repeater {
                model: root.publishingRows
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.formatInteger(modelData.count)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: qsTrId("genycaster.stats.average").arg(
                LocaleFormat.formatNumber(Number(root.metrics.average || 0), 1))
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
            wrapMode: Text.Wrap
        }
    }
}

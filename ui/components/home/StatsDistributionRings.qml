import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Farcaster.Client

// Shares of observed reactions, not goal-completion percentages.
// Every arc is backed by the same provider-scoped counts as the line chart.
Item {
    id: root
    objectName: "statsEngagementDistribution"

    property real likes: 0
    property real recasts: 0
    property real receivedReplies: 0
    property color likesColor: Theme.accent
    property color recastsColor: Theme.analyticsRecasts
    property color repliesColor: Theme.analyticsReplies
    property bool compact: false
    property real reveal: 1

    readonly property real sum: Math.max(0, likes) + Math.max(0, recasts)
                                 + Math.max(0, receivedReplies)
    readonly property real likesShare: sum > 0 ? Math.max(0, likes) / sum : 0
    readonly property real recastsShare: sum > 0 ? Math.max(0, recasts) / sum : 0
    readonly property real repliesShare: sum > 0 ? Math.max(0, receivedReplies) / sum : 0
    readonly property int ringSize: compact ? 196 : 226
    readonly property var segments: [
        {label: qsTrId("genycaster.stats.likes"), count: likes, share: likesShare, color: likesColor},
        {label: qsTrId("genycaster.stats.recasts"), count: recasts, share: recastsShare, color: recastsColor},
        {label: qsTrId("genycaster.stats.received_replies"), count: receivedReplies,
         share: repliesShare, color: repliesColor}
    ]

    // The centre communicates the largest measured share, not a fictitious
    // completion goal. The absolute total is still shown in the parent header.
    readonly property var leadingSeries: {
        const rows = segments
        let leading = rows[0]
        for (let i = 1; i < rows.length; ++i) {
            if (rows[i].count > leading.count)
                leading = rows[i]
        }
        return leading
    }

    function animate() {
        revealAnimation.stop()
        if (Theme.reduceMotion || sum <= 0) {
            reveal = 1
            return
        }
        reveal = 0
        revealAnimation.start()
    }
    onSumChanged: Qt.callLater(animate)
    Component.onCompleted: animate()

    NumberAnimation {
        id: revealAnimation
        target: root
        property: "reveal"
        to: 1
        duration: Theme.reduceMotion ? 0 : 720
        easing.type: Easing.OutCubic
    }

    implicitHeight: distribution.implicitHeight
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTrId("genycaster.stats.engagement") + ": "
                     + LocaleFormat.formatInteger(root.sum) + " · "
                     + root.leadingSeries.label + ": "
                     + LocaleFormat.formatInteger(Math.round(root.leadingSeries.share * 100)) + "%"

    GridLayout {
        id: distribution
        width: parent.width
        columns: 1
        columnSpacing: Theme.space4
        rowSpacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Item {
            id: ring
            Layout.preferredWidth: root.ringSize
            Layout.preferredHeight: root.ringSize
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            readonly property real center: width / 2
            readonly property real stroke: root.compact ? 6.5 : 7
            readonly property real outerRadius: width / 2 - stroke / 2 - 4
            readonly property real middleRadius: outerRadius - (root.compact ? 22 : 26)
            readonly property real innerRadius: middleRadius - (root.compact ? 22 : 26)

            Item {
                anchors.fill: parent
                transform: Scale {
                    origin.x: ring.center
                    xScale: Theme.rtl ? -1 : 1
                }
                Shape {
                    anchors.fill: parent
                    // Native GPU curves preserve circular edges and round caps
                    // without polygon tessellation or another MSAA texture pass.
                    preferredRendererType: Shape.CurveRenderer
                    asynchronous: false
                    antialiasing: true

                    ShapePath {
                        strokeColor: Theme.borderMuted
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.FlatCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.outerRadius; radiusY: ring.outerRadius
                            startAngle: -90; sweepAngle: 359.8 }
                    }
                    ShapePath {
                        strokeColor: Theme.borderMuted
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.FlatCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.middleRadius; radiusY: ring.middleRadius
                            startAngle: -90; sweepAngle: 359.8 }
                    }
                    ShapePath {
                        strokeColor: Theme.borderMuted
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.FlatCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.innerRadius; radiusY: ring.innerRadius
                            startAngle: -90; sweepAngle: 359.8 }
                    }
                    ShapePath {
                        strokeColor: root.likesShare > 0 ? root.likesColor : "transparent"
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.outerRadius; radiusY: ring.outerRadius
                            startAngle: -90; sweepAngle: Math.min(359.8, 360 * root.likesShare * root.reveal) }
                    }
                    ShapePath {
                        strokeColor: root.recastsShare > 0 ? root.recastsColor : "transparent"
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.middleRadius; radiusY: ring.middleRadius
                            startAngle: -90; sweepAngle: Math.min(359.8, 360 * root.recastsShare * root.reveal) }
                    }
                    ShapePath {
                        strokeColor: root.repliesShare > 0 ? root.repliesColor : "transparent"
                        strokeWidth: ring.stroke
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: ring.center; centerY: ring.center
                            radiusX: ring.innerRadius; radiusY: ring.innerRadius
                            startAngle: -90; sweepAngle: Math.min(359.8, 360 * root.repliesShare * root.reveal) }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 1
                DirectionAwareLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.max(64, ring.innerRadius * 1.8)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: root.sum > 0 ? LocaleFormat.formatInteger(Math.round(root.leadingSeries.share * 100)) + "%" : "—"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    horizontalAlignment: Text.AlignHCenter
                }
                DirectionAwareLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.max(64, ring.innerRadius * 1.8)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: root.sum > 0 ? root.leadingSeries.label : qsTrId("genycaster.stats.engagement")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(300, root.width)
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.space2

            Repeater {
                model: root.segments
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Rectangle {
                        Layout.preferredWidth: 15
                        Layout.preferredHeight: 6
                        radius: 3
                        color: modelData.color
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: modelData.label
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.formatInteger(Math.round(modelData.share * 100)) + "%"
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                }
            }
        }
    }
}

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
    // A slim desktop insight rail preserves most of the trend-chart width.
    // Full-size rings remain available when the true mobile layout stacks.
    property bool sidebarMode: false
    property real reveal: 1
    // Selecting an arc/legend item persists; hover temporarily overrides it.
    property int hoveredSeriesIndex: -1
    property int selectedSeriesIndex: -1

    readonly property real sum: Math.max(0, likes) + Math.max(0, recasts)
                                 + Math.max(0, receivedReplies)
    readonly property real likesShare: sum > 0 ? Math.max(0, likes) / sum : 0
    readonly property real recastsShare: sum > 0 ? Math.max(0, recasts) / sum : 0
    readonly property real repliesShare: sum > 0 ? Math.max(0, receivedReplies) / sum : 0
    readonly property int ringSize: sidebarMode ? 156 : compact ? 196 : 226
    readonly property var segments: [
        {label: qsTrId("genycaster.stats.likes"), count: likes, share: likesShare, color: likesColor},
        {label: qsTrId("genycaster.stats.recasts"), count: recasts, share: recastsShare, color: recastsColor},
        {label: qsTrId("genycaster.stats.received_replies"), count: receivedReplies,
         share: repliesShare, color: repliesColor}
    ]

    // The centre communicates the largest measured share, not a fictitious
    // completion goal. The absolute total is still shown in the parent header.
    readonly property int leadingIndex: {
        let leading = 0
        for (let i = 1; i < segments.length; ++i) {
            if (segments[i].count > segments[leading].count)
                leading = i
        }
        return leading
    }
    readonly property var leadingSeries: segments[leadingIndex]
    readonly property int activeSeriesIndex: hoveredSeriesIndex >= 0 ? hoveredSeriesIndex
                                             : selectedSeriesIndex >= 0 ? selectedSeriesIndex
                                             : leadingIndex
    readonly property var activeSeries: segments[activeSeriesIndex]

    function ringIndexAt(x, y) {
        const dx = x - ring.center
        const dy = y - ring.center
        const distance = Math.sqrt(dx * dx + dy * dy)
        const radii = [ring.outerRadius, ring.middleRadius, ring.innerRadius]
        const allowance = ring.stroke / 2 + 7
        let candidate = -1
        let nearest = allowance
        for (let i = 0; i < radii.length; ++i) {
            const delta = Math.abs(distance - radii[i])
            if (delta <= nearest) {
                nearest = delta
                candidate = i
            }
        }
        return candidate
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
                     + root.activeSeries.label + ": "
                     + LocaleFormat.formatInteger(Math.round(root.activeSeries.share * 100)) + "%"
                     + " · " + LocaleFormat.formatInteger(root.activeSeries.count)

    GridLayout {
        id: distribution
        width: parent.width
        columns: 1
        columnSpacing: Theme.space4
        rowSpacing: root.sidebarMode ? Theme.space1 : Theme.space2
        layoutDirection: Theme.uiDirection

        Item {
            id: ring
            Layout.preferredWidth: root.ringSize
            Layout.preferredHeight: root.ringSize
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: root.activeSeries.label + ": "
                             + LocaleFormat.formatInteger(root.activeSeries.count)
            Accessible.onPressAction: root.selectedSeriesIndex = (root.activeSeriesIndex + 1) % 3
            Keys.onLeftPressed: event => {
                root.selectedSeriesIndex = (root.activeSeriesIndex + (Theme.rtl ? 1 : 2)) % 3
                event.accepted = true
            }
            Keys.onRightPressed: event => {
                root.selectedSeriesIndex = (root.activeSeriesIndex + (Theme.rtl ? 2 : 1)) % 3
                event.accepted = true
            }
            readonly property real center: width / 2
            readonly property real stroke: root.sidebarMode ? 4.8 : root.compact ? 4.5 : 5.8
            readonly property real outerRadius: width / 2 - stroke / 2 - 4
            readonly property real middleRadius: outerRadius - (root.sidebarMode ? 16 : root.compact ? 22 : 26)
            readonly property real innerRadius: middleRadius - (root.sidebarMode ? 16 : root.compact ? 22 : 26)

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
                    text: root.sum > 0 ? LocaleFormat.formatInteger(Math.round(root.activeSeries.share * 100)) + "%" : "—"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                }
                DirectionAwareLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.max(64, ring.innerRadius * 1.8)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: root.sum > 0 ? root.activeSeries.label : qsTrId("genycaster.stats.engagement")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                }
                DirectionAwareLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.max(64, ring.innerRadius * 1.8)
                    horizontalAlignment: Text.AlignHCenter
                    text: root.sum > 0 ? LocaleFormat.formatInteger(root.activeSeries.count) : ""
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                }
            }

            // Hit-test the annulus rather than the bounding square, so the
            // central labels do not accidentally select a series.
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                cursorShape: root.hoveredSeriesIndex >= 0
                             ? Qt.PointingHandCursor : Qt.ArrowCursor
                onPositionChanged: mouse =>
                    root.hoveredSeriesIndex = root.ringIndexAt(mouse.x, mouse.y)
                onExited: root.hoveredSeriesIndex = -1
                onClicked: mouse => {
                    const index = root.ringIndexAt(mouse.x, mouse.y)
                    if (index >= 0)
                        root.selectedSeriesIndex = index
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(300, root.width)
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignHCenter
            spacing: root.sidebarMode ? Theme.space1 : Theme.space2

            Repeater {
                model: root.segments
                delegate: RowLayout {
                    id: segmentRow
                    required property int index
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
                        font.weight: root.activeSeriesIndex === segmentRow.index
                                     ? Theme.weightBold : Theme.weightDemiBold
                        color: Theme.textPrimary
                    }
                    opacity: root.activeSeriesIndex === segmentRow.index ? 1 : 0.7
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                        onHoveredChanged: {
                            if (hovered)
                                root.hoveredSeriesIndex = segmentRow.index
                            else if (root.hoveredSeriesIndex === segmentRow.index)
                                root.hoveredSeriesIndex = -1
                        }
                    }
                    TapHandler {
                        onTapped: root.selectedSeriesIndex = segmentRow.index
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: segmentRow.modelData.label + ": "
                                     + LocaleFormat.formatInteger(segmentRow.modelData.count)
                    Accessible.onPressAction: root.selectedSeriesIndex = segmentRow.index
                }
            }
        }
    }
}

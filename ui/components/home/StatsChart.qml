import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

NeonSurface {
    id: root
    interactive: true
    ambientNeon: true
    neonStrength: Theme.neonCardStrength
    objectName: "statsPublishingChart"
    property var bars: []
    property int periodDays: 30
    property bool compact: false
    property int selectedIndex: -1
    property int hoveredIndex: -1
    readonly property int activeIndex: hoveredIndex >= 0 ? hoveredIndex : selectedIndex
    property real reveal: 1
    readonly property color replyInk: Theme.analyticsReplies
    readonly property int maxCount: {
        let n = 1
        for (let i = 0; i < bars.length; ++i)
            n = Math.max(n, Number(bars[i].casts || 0) + Number(bars[i].replies || 0))
        return n
    }
    readonly property var selected: activeIndex >= 0 && activeIndex < bars.length
                                    ? bars[activeIndex] : null

    function localizedDay(iso, fallback) {
        const canonical = String(iso || "")
        if (!/^\d{4}-\d{2}-\d{2}$/.test(canonical))
            return LocaleFormat.localizeDigits(String(fallback || ""))
        // Local noon prevents a UTC-midnight date shift in western time zones.
        const day = new Date(canonical + "T12:00:00")
        return LocaleFormat.localizeDigits(
            day.toLocaleDateString(Qt.locale(localizationController.effectiveLanguageTag), "MMM d"))
    }
    function localizedRange(row) {
        if (!row) return ""
        const first = localizedDay(row.startDate, row.label)
        if (!row.endDate || row.endDate === row.startDate) return first
        return first + " – " + localizedDay(row.endDate, row.label)
    }

    onVisibleChanged: if (!visible) hoveredIndex = -1

    onBarsChanged: {
        selectedIndex = -1
        hoveredIndex = -1
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
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Icon {
                text: Theme.iconChart
                color: Theme.textSecondary
                preferredSize: Theme.iconSizeSmall
            }
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
            Rectangle { width: 7; height: 7; radius: 3.5; color: root.replyInk }
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.replies")
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Item {
                Layout.preferredWidth: 26
                Layout.fillHeight: true
                DirectionAwareLabel {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    text: LocaleFormat.formatInteger(root.maxCount)
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
                DirectionAwareLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    text: LocaleFormat.formatInteger(Math.round(root.maxCount / 2))
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
                DirectionAwareLabel {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    text: LocaleFormat.formatInteger(0)
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.textTertiary
                }
            }

            Item {
                id: plot
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property real columnGap: Math.min(2, Math.max(0, width / Math.max(1, root.bars.length) / 4))
                function columnCenter(index) {
                    const count = Math.max(1, root.bars.length)
                    const slot = Math.max(0.75, (width - columnGap * (count - 1)) / count)
                    const center = index * (slot + columnGap) + slot / 2
                    return Theme.rtl ? width - center : center
                }
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
                    layoutDirection: Theme.uiDirection
                    spacing: plot.columnGap
                    Repeater {
                        model: root.bars
                        delegate: Item {
                            id: column
                            required property int index
                            required property var modelData
                            readonly property int casts: Number(modelData.casts || 0)
                            readonly property int replies: Number(modelData.replies || 0)
                            readonly property bool highlighted: root.activeIndex === index
                            readonly property real barWidth: Math.min(26, Math.max(0.75, width - Math.min(2, width * 0.22)))
                            // Pixel-snapped segment joins avoid subpixel seams.
                            readonly property real castHeight: Math.round(plot.height * casts / root.maxCount * root.reveal)
                            readonly property real replyHeight: Math.round(plot.height * replies / root.maxCount * root.reveal)
                            width: Math.max(0.75, (plot.width - plot.columnGap * Math.max(0, root.bars.length - 1))
                                                / Math.max(1, root.bars.length))
                            height: plot.height
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTrId("genycaster.stats.chart_point")
                                .arg(root.localizedRange(modelData))
                                .arg(LocaleFormat.formatInteger(Number(modelData.casts)))
                                .arg(LocaleFormat.formatInteger(Number(modelData.replies)))
                            Accessible.onPressAction: root.selectedIndex = index

                            // The same retained QSG Superellipse used by Feed
                            // Pulse draws each segment. Only the exposed TOP
                            // corners curve; the shared edge and baseline stay
                            // square, with no outline or stacked rounded seams.
                            // Hover changes opacity only, never geometry.
                            Item {
                                id: stack
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: plot.height - column.castHeight - column.replyHeight
                                width: column.barWidth
                                height: Math.max(0, column.castHeight + column.replyHeight)
                                clip: true
                                opacity: column.highlighted ? 0.86 : 1
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: Theme.reduceMotion ? 0 : Theme.motionFast
                                        easing.type: Easing.OutCubic
                                    }
                                }

                                Superellipse {
                                    id: castsSegment
                                    visible: column.castHeight > 0
                                    x: 0
                                    y: column.replyHeight
                                    width: stack.width
                                    height: column.castHeight
                                    fillColor: Theme.accent
                                    borderWidth: 0
                                    curveDepth: Math.min(5, width / 2, height / 2)
                                    curveTopLeft: column.replyHeight <= 0
                                    curveTopRight: column.replyHeight <= 0
                                    curveBottomLeft: false
                                    curveBottomRight: false
                                }
                                Superellipse {
                                    id: repliesSegment
                                    visible: column.replyHeight > 0
                                    x: 0
                                    y: 0
                                    width: stack.width
                                    // Overpaint one logical pixel of the lower segment.
                                    // Both heights are pixel-snapped; this closes
                                    // subpixel hairlines at fractional display scales.
                                    height: column.replyHeight + (column.castHeight > 0 ? 1 : 0)
                                    fillColor: root.replyInk
                                    borderWidth: 0
                                    curveDepth: Math.min(5, width / 2, height / 2)
                                    curveTopLeft: true
                                    curveTopRight: true
                                    curveBottomLeft: false
                                    curveBottomRight: false
                                }
                            }
                            TapHandler { onTapped: root.selectedIndex = column.index }
                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                                // Only a visible bar triggers the floating details.
                                // A tap can still inspect an empty day on touch.
                                onPointChanged: {
                                    if (hovered)
                                        root.hoveredIndex = point.position.y >= stack.y - 6
                                            ? column.index : -1
                                }
                                onHoveredChanged: {
                                    if (hovered)
                                        root.hoveredIndex = point.position.y >= stack.y - 6
                                            ? column.index : -1
                                    else if (root.hoveredIndex === column.index)
                                        root.hoveredIndex = -1
                                }
                            }
                        }
                    }
                }

                HoverHandler {
                    id: plotHover
                    onHoveredChanged: {
                        if (!hovered)
                            root.hoveredIndex = -1
                    }
                }

                // Non-intercepting inspection bubble stays within the plot.
                // The guide below remains available for touch/narrow widths.
                Superellipse {
                    id: barInspectCard
                    z: 8
                    enabled: false
                    visible: root.hoveredIndex >= 0 && plotHover.hovered
                             && root.selected !== null && plot.width >= 290
                             && !root.compact
                    width: Math.min(204, plot.width - 8)
                    height: barInspectContent.implicitHeight + Theme.space3 * 2
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderMuted
                    borderWidth: 1
                    curveDepth: Theme.curveContainer
                    readonly property real anchorX: plot.columnCenter(root.activeIndex)
                    readonly property real topY: root.selected
                        ? plot.height - Math.round(plot.height
                            * (Number(root.selected.casts || 0) + Number(root.selected.replies || 0))
                            / root.maxCount * root.reveal)
                        : plot.height
                    x: anchorX + width + 12 <= plot.width - 4
                       ? anchorX + 12 : Math.max(4, anchorX - width - 12)
                    y: Math.max(4, Math.min(plot.height - height - 4, topY - height - 10))

                    ColumnLayout {
                        id: barInspectContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space3
                        spacing: Theme.space1

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            alignToUi: true
                            text: root.localizedRange(root.selected)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            Rectangle {
                                width: 7
                                height: 7
                                radius: width / 2
                                color: Theme.accent
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.stats.casts")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareLabel {
                                text: root.selected
                                    ? LocaleFormat.formatInteger(Number(root.selected.casts || 0)) : ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            Rectangle {
                                width: 7
                                height: 7
                                radius: width / 2
                                color: root.replyInk
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.stats.replies")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            DirectionAwareLabel {
                                text: root.selected
                                    ? LocaleFormat.formatInteger(Number(root.selected.replies || 0)) : ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                alignToUi: true
                text: root.bars.length ? root.localizedDay(root.bars[0].startDate, root.bars[0].label) : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
            DirectionAwareLabel {
                alignToUi: true
                text: qsTrId("genycaster.stats.today")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: root.selected
                ? qsTrId("genycaster.stats.chart_point")
                    .arg(root.localizedRange(root.selected))
                    .arg(LocaleFormat.formatInteger(Number(root.selected.casts)))
                    .arg(LocaleFormat.formatInteger(Number(root.selected.replies)))
                : qsTrId("genycaster.stats.chart_hint")
            color: root.selected ? Theme.textPrimary : Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }
}

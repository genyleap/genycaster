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
                                opacity: column.highlighted ? 0.78 : 1
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
                                    height: column.replyHeight
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
                                onHoveredChanged: root.hoveredIndex = hovered
                                    ? column.index : -1
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

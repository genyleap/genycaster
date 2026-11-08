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

    function localizedDay(iso, fallback) {
        const canonical = String(iso || "")
        if (!/^\\d{4}-\\d{2}-\\d{2}$/.test(canonical))
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
                                .arg(root.localizedRange(modelData))
                                .arg(LocaleFormat.formatInteger(Number(modelData.casts)))
                                .arg(LocaleFormat.formatInteger(Number(modelData.replies)))
                            Accessible.onPressAction: root.selectedIndex = index

                            // The stack has rounded OUTER corners only. Flatten both
                            // touching faces to avoid the visible notch between segments.
                            // No Canvas or offscreen layer: this remains cheap in WASM.
                            Rectangle {
                                id: repliesSegment
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: plot.height - column.castHeight - column.replyHeight
                                width: column.barWidth
                                height: Math.max(0, column.replyHeight)
                                radius: Math.min(3, width / 2)
                                color: Theme.textTertiary
                                opacity: root.selectedIndex === -1 || root.selectedIndex === column.index ? 1 : 0.55
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: Math.min(parent.radius, parent.height)
                                    visible: column.castHeight > 0
                                    radius: 0
                                    color: repliesSegment.color
                                }
                            }
                            Rectangle {
                                id: castsSegment
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: column.barWidth
                                height: Math.max(0, column.castHeight)
                                radius: Math.min(3, width / 2)
                                color: Theme.accent
                                opacity: root.selectedIndex === -1 || root.selectedIndex === column.index ? 1 : 0.55
                                Rectangle {
                                    anchors.top: parent.top
                                    width: parent.width
                                    height: Math.min(parent.radius, parent.height)
                                    visible: column.replyHeight > 0
                                    radius: 0
                                    color: castsSegment.color
                                }
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
                text: root.bars.length ? LocaleFormat.localizeDigits(String(root.bars[0].label)) : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
            DirectionAwareLabel {
                text: qsTrId("genycaster.stats.today")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.selected
                ? qsTrId("genycaster.stats.chart_point")
                    .arg(LocaleFormat.localizeDigits(String(root.selected.range || root.selected.label)))
                    .arg(LocaleFormat.formatInteger(Number(root.selected.casts)))
                    .arg(LocaleFormat.formatInteger(Number(root.selected.replies)))
                : qsTrId("genycaster.stats.chart_hint")
            color: root.selected ? Theme.textPrimary : Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }
}

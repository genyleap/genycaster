import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Five real, period-scoped counters use one shared linear scale.
// The radar visualizes their relative magnitudes; exact counts remain in
// accessible, localized legend rows. It never invents historical values.
Superellipse {
    id: root
    objectName: "statsEngagementBreakdown"
    property var metrics: ({})
    readonly property real total: Number(metrics.engagement || 0)
    readonly property var categories: [
        {label: qsTrId("genycaster.stats.likes"), count: Math.max(0, Number(metrics.likes || 0))},
        {label: qsTrId("genycaster.stats.recasts"), count: Math.max(0, Number(metrics.recasts || 0))},
        {label: qsTrId("genycaster.stats.received_replies"), count: Math.max(0, Number(metrics.receivedReplies || 0))},
        {label: qsTrId("genycaster.stats.casts"), count: Math.max(0, Number(metrics.casts || 0))},
        {label: qsTrId("genycaster.stats.replies"), count: Math.max(0, Number(metrics.replies || 0))}
    ]
    readonly property real highestCount: {
        let maximum = 1
        for (const category of categories)
            maximum = Math.max(maximum, category.count)
        return maximum
    }
    property real drawProgress: 1
    onCategoriesChanged: {
        radarTween.stop()
        if (Theme.reduceMotion) drawProgress = 1
        else {
            drawProgress = 0
            radarTween.start()
        }
    }
    onDrawProgressChanged: radar.requestPaint()

    NumberAnimation {
        id: radarTween
        target: root
        property: "drawProgress"
        to: 1
        duration: Theme.reduceMotion ? 0 : 430
        easing.type: Easing.OutCubic
    }

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
        spacing: Theme.space2

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
                alignToUi: true
                text: qsTrId("genycaster.stats.engagement_breakdown")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: LocaleFormat.formatInteger(root.total)
                color: Theme.textSecondary
                font.pixelSize: Theme.typeMetaSize
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 218
            Canvas {
                id: radar
                width: Math.min(parent.width, 280)
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
                readonly property color accent: Theme.accent
                readonly property color gridColor: Theme.borderMuted
                readonly property color pointColor: Theme.textPrimary
                readonly property bool rightToLeft: Theme.rtl
                onRightToLeftChanged: requestPaint()
                onAccentChanged: requestPaint()
                onGridColorChanged: requestPaint()
                onPointColorChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Component.onCompleted: requestPaint()

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const count = root.categories.length
                    if (count < 3 || width < 20 || height < 20) return
                    const cx = width / 2
                    const cy = height / 2
                    const radius = Math.max(0, Math.min(width, height) / 2 - 15)
                    function point(index, fraction) {
                        const angle = index * 2 * Math.PI / count - Math.PI / 2
                        return {
                            x: cx + (radar.rightToLeft ? -1 : 1) * radius * fraction * Math.cos(angle),
                            y: cy + radius * fraction * Math.sin(angle)
                        }
                    }
                    function polygon(fractions) {
                        ctx.beginPath()
                        for (let i = 0; i < count; ++i) {
                            const p = point(i, fractions[i])
                            if (i === 0) ctx.moveTo(p.x, p.y)
                            else ctx.lineTo(p.x, p.y)
                        }
                        ctx.closePath()
                    }
                    ctx.lineWidth = 1
                    ctx.strokeStyle = String(gridColor)
                    ctx.globalAlpha = 0.75
                    for (let level = 1; level <= 4; ++level) {
                        polygon(Array(count).fill(level / 4))
                        ctx.stroke()
                    }
                    for (let i = 0; i < count; ++i) {
                        const p = point(i, 1)
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.lineTo(p.x, p.y)
                        ctx.stroke()
                    }
                    const fractions = root.categories.map(category =>
                        Math.min(1, category.count / root.highestCount) * root.drawProgress)
                    polygon(fractions)
                    ctx.fillStyle = String(accent)
                    ctx.globalAlpha = 0.16
                    ctx.fill()
                    ctx.globalAlpha = 1
                    ctx.strokeStyle = String(accent)
                    ctx.lineWidth = 2.4
                    ctx.lineJoin = "round"
                    ctx.stroke()
                    for (let i = 0; i < count; ++i) {
                        const p = point(i, fractions[i])
                        ctx.beginPath()
                        ctx.arc(p.x, p.y, 3.5, 0, 2 * Math.PI)
                        ctx.fillStyle = String(accent)
                        ctx.fill()
                    }
                    ctx.globalAlpha = 1
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: qsTrId("genycaster.stats.radar_explainer").arg(
                LocaleFormat.formatInteger(root.highestCount))
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            columns: root.width < 430 ? 1 : 2
            columnSpacing: Theme.space4
            rowSpacing: Theme.space2
            Repeater {
                model: root.categories
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
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
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                        wrapMode: Text.Wrap
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderMuted
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

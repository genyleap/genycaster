import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Farcaster.Client

// Current interactions on published posts, bucketed by the publication date.
// The upper line chart has independent visible axes: likes (primary) and
// recasts/received replies (secondary). It never pretends to know the exact
// time when an interaction occurred.
Superellipse {
    id: root
    objectName: "statsEngagementBreakdown"

    property var metrics: ({})
    property var activity: []
    property int hoveredIndex: -1
    property int selectedIndex: -1
    // Legend selection persists; hovering a line temporarily inspects it.
    // Keeping one series in focus prevents three near-parallel curves
    // from looking like a single thick, ambiguous stroke.
    property string selectedSeriesKey: "likes"
    property string hoveredSeriesKey: ""
    readonly property string focusedSeriesKey: hoveredSeriesKey || selectedSeriesKey
    readonly property color focusedInk: focusedSeriesKey === "recasts" ? recastsInk
                                        : focusedSeriesKey === "receivedReplies" ? repliesInk
                                        : Theme.accent
    readonly property int inspectedIndex: hoveredIndex >= 0 ? hoveredIndex : selectedIndex
    readonly property var inspectedBucket: inspectedIndex >= 0 && inspectedIndex < activity.length
                                            ? activity[inspectedIndex] : null
    readonly property real receivedTotal: Math.max(0, Number(metrics.engagement || 0))
    readonly property color recastsInk: Theme.analyticsRecasts
    readonly property color repliesInk: Theme.analyticsReplies
    property real chartReveal: 1
    readonly property var receivedSeries: [
        {key: "likes", label: qsTrId("genycaster.stats.likes"), color: Theme.accent, axis: 0},
        {key: "recasts", label: qsTrId("genycaster.stats.recasts"), color: root.recastsInk, axis: 1},
        {key: "receivedReplies", label: qsTrId("genycaster.stats.received_replies"), color: root.repliesInk, axis: 1}
    ]
    readonly property var publishingRows: [
        {label: qsTrId("genycaster.stats.casts"), count: Math.max(0, Number(metrics.casts || 0))},
        {label: qsTrId("genycaster.stats.replies"), count: Math.max(0, Number(metrics.replies || 0))}
    ]
    readonly property real likesMax: {
        let maximum = 1
        for (const bucket of activity || [])
            maximum = Math.max(maximum, Number(bucket.likes || 0))
        return maximum
    }
    readonly property real secondaryMax: {
        let maximum = 1
        for (const bucket of activity || [])
            maximum = Math.max(maximum, Number(bucket.recasts || 0),
                               Number(bucket.receivedReplies || 0))
        return maximum
    }
    function lineInk(key, ink) {
        const amount = focusedSeriesKey === "" || focusedSeriesKey === key ? 1 : 0.30
        return Qt.rgba(ink.r, ink.g, ink.b, amount)
    }
    function lineWidth(key) {
        const emphasized = focusedSeriesKey === "" || focusedSeriesKey === key
        return key === "likes" ? (emphasized ? 2.1 : 1.45)
                               : (emphasized ? 1.8 : 1.3)
    }
    function nearestSeriesAt(x, y, w, h) {
        const index = indexAt(x, w)
        if (index < 0) return ""
        let nearest = ""
        let distance = 20
        for (const entry of receivedSeries) {
            const point = pointFor(index, entry.key,
                entry.axis === 0 ? likesMax : secondaryMax, w, h)
            const delta = Math.abs(point.y - y)
            if (delta < distance) {
                distance = delta
                nearest = entry.key
            }
        }
        return nearest
    }

    readonly property bool hasTimeSeries: activity && activity.length >= 2
    readonly property real plotInset: 5
    readonly property bool narrow: width < 460

    function pointFor(index, key, maximum, width, height) {
        const fraction = activity.length > 1 ? index / (activity.length - 1) : 0
        const xFraction = Theme.rtl ? 1 - fraction : fraction
        const value = Math.max(0, Number(activity[index]?.[key] || 0))
        const clamped = Math.min(1, value / Math.max(1, maximum))
        return Qt.point(plotInset + xFraction * Math.max(0, width - 2 * plotInset),
                        plotInset + (1 - clamped) * Math.max(0, height - 2 * plotInset))
    }
    function pointsFor(key, maximum, width, height) {
        if (!hasTimeSeries || width < 20 || height < 20)
            return []
        const points = []
        for (let i = 0; i < activity.length; ++i)
            points.push(pointFor(i, key, maximum, width, height))
        return points
    }
    // Restrained monotone cubic Hermite interpolation. Short tangent handles
    // make the weekly trend readable (rather than waves piled on top of
    // each other) without overshooting the measured endpoints.
    // Plot points stay exact and remain in chronological order in both LTR
    // and RTL; only their physical x coordinates are mirrored.
    function smoothPath(key, maximum, w, h, fillArea) {
        const points = pointsFor(key, maximum, w, h)
        if (points.length < 2) return ""
        const secants = []
        for (let i = 0; i < points.length - 1; ++i) {
            const dx = points[i + 1].x - points[i].x
            secants.push(dx === 0 ? 0 : (points[i + 1].y - points[i].y) / dx)
        }
        const slopes = []
        for (let i = 0; i < points.length; ++i) {
            if (i === 0) slopes.push(secants[0])
            else if (i === points.length - 1) slopes.push(secants[secants.length - 1])
            else {
                const a = secants[i - 1]
                const b = secants[i]
                slopes.push(a * b <= 0 ? 0 : 2 * a * b / (a + b))
            }
        }
        function n(value) { return Number(value).toFixed(2) }
        let svg = "M " + n(points[0].x) + " " + n(points[0].y)
        for (let i = 0; i < points.length - 1; ++i) {
            const a = points[i]
            const b = points[i + 1]
            const dx = (b.x - a.x) * 0.16
            svg += " C " + n(a.x + dx) + " " + n(a.y + slopes[i] * dx)
                + " " + n(b.x - dx) + " " + n(b.y - slopes[i + 1] * dx)
                + " " + n(b.x) + " " + n(b.y)
        }
        if (fillArea) {
            const baseline = n(h - plotInset)
            svg += " L " + n(points[points.length - 1].x) + " " + baseline
                + " L " + n(points[0].x) + " " + baseline + " Z"
        }
        return svg
    }
    function activePoint(key, maximum, w, h) {
        return inspectedIndex < 0 || inspectedIndex >= activity.length
            ? Qt.point(0, 0) : pointFor(inspectedIndex, key, maximum, w, h)
    }

    function indexAt(x, width) {
        if (!hasTimeSeries || width <= 2 * plotInset)
            return -1
        const fraction = Math.max(0, Math.min(1, (x - plotInset) / (width - 2 * plotInset)))
        return Math.max(0, Math.min(activity.length - 1,
            Math.round((Theme.rtl ? 1 - fraction : fraction) * (activity.length - 1))))
    }
    function dateLabel(value) {
        const iso = String(value || "")
        if (!/^\d{4}-\d{2}-\d{2}$/.test(iso))
            return LocaleFormat.localizeDigits(iso)
        const parsed = new Date(iso + "T12:00:00")
        return LocaleFormat.localizeDigits(parsed.toLocaleDateString(
            Qt.locale(localizationController.effectiveLanguageTag), "MMM d"))
    }
    function bucketLabel(bucket) {
        if (!bucket) return ""
        const first = dateLabel(bucket.startDate)
        const last = dateLabel(bucket.endDate)
        return bucket.endDate && bucket.endDate !== bucket.startDate ? first + " – " + last : first
    }

    function animateChart() {
        curveRevealTween.stop()
        if (Theme.reduceMotion || !hasTimeSeries) {
            chartReveal = 1
            return
        }
        chartReveal = 0
        curveRevealTween.start()
    }
    onActivityChanged: {
        selectedIndex = -1
        hoveredIndex = -1
        hoveredSeriesKey = ""
        Qt.callLater(animateChart)
    }
    Component.onCompleted: animateChart()

    NumberAnimation {
        id: curveRevealTween
        target: root
        property: "chartReveal"
        to: 1
        duration: Theme.reduceMotion ? 0 : 760
        easing.type: Easing.OutCubic
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

        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: qsTrId("genycaster.stats.received_by_publish_date")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        GridLayout {
            id: engagementGrid
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            layoutDirection: Theme.uiDirection
            columns: root.width >= 980 ? 2 : 1
            columnSpacing: Theme.space4
            rowSpacing: Theme.space3

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: root.width >= 980 ? 650 : 1
                spacing: Theme.space2

                GridLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    columns: root.width >= 720 ? 3 : root.width >= 460 ? 2 : 1
                    columnSpacing: Theme.space3
                    rowSpacing: Theme.space1
                    Repeater {
                        model: root.receivedSeries
                        delegate: RowLayout {
                            id: legendEntry
                            required property var modelData
                            spacing: Theme.space1
                            opacity: root.focusedSeriesKey === ""
                                     || root.focusedSeriesKey === modelData.key ? 1 : 0.48
                            Rectangle {
                                width: 12
                                height: 3
                                radius: height / 2
                                color: modelData.color
                            }
                            DirectionAwareLabel {
                                text: modelData.label
                                color: root.focusedSeriesKey === modelData.key
                                       ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: root.focusedSeriesKey === modelData.key
                                             ? Theme.weightDemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                                onHoveredChanged: {
                                    if (hovered)
                                        root.hoveredSeriesKey = legendEntry.modelData.key
                                    else if (root.hoveredSeriesKey === legendEntry.modelData.key)
                                        root.hoveredSeriesKey = ""
                                }
                            }
                            TapHandler {
                                onTapped: {
                                    const key = legendEntry.modelData.key
                                    root.selectedSeriesKey = root.selectedSeriesKey === key ? "" : key
                                }
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: legendEntry.modelData.label
                            Accessible.onPressAction: root.selectedSeriesKey = legendEntry.modelData.key
                        }
                    }
                }

                RowLayout {
                    id: chartRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.narrow ? 154 : 198
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    // Primary axis (likes) follows the start side of the layout.
                    ColumnLayout {
                        Layout.preferredWidth: root.narrow ? 28 : 38
                        Layout.fillHeight: true
                        spacing: 0
                        DirectionAwareLabel {
                            text: LocaleFormat.formatInteger(root.likesMax)
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.accent
                            Layout.alignment: Qt.AlignTop
                        }
                        Item { Layout.fillHeight: true }
                        DirectionAwareLabel {
                            text: LocaleFormat.formatInteger(0)
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textTertiary
                        }
                    }

                    Item {
                        id: plot
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.fillHeight: true
                        clip: true

                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                required property int index
                                y: root.plotInset + index * (plot.height - 2 * root.plotInset) / 2
                                width: plot.width
                                height: 1
                                color: Theme.borderMuted
                            }
                        }

                        // Reveal the native curve geometry chronologically in
                        // either reading direction without rebuilding its data.
                        Item {
                            id: curveRevealMask
                            x: Theme.rtl ? plot.width * (1 - root.chartReveal) : 0
                            width: plot.width * root.chartReveal
                            height: plot.height
                            clip: true

                        Shape {
                            id: engagementShape
                            x: -curveRevealMask.x
                            y: 0
                            width: plot.width
                            height: plot.height
                            visible: root.hasTimeSeries
                            // GPU curves stay smooth on HiDPI and fractional
                            // scaling; CurveRenderer owns antialiasing directly.
                            preferredRendererType: Shape.CurveRenderer
                            asynchronous: false
                            antialiasing: true

                            ShapePath {
                                strokeColor: "transparent"
                                fillColor: Qt.rgba(root.focusedInk.r, root.focusedInk.g,
                                                   root.focusedInk.b, Theme.dark ? 0.10 : 0.055)
                                PathSvg {
                                    path: root.smoothPath(
                                        root.focusedSeriesKey || "likes",
                                        root.focusedSeriesKey === "recasts"
                                        || root.focusedSeriesKey === "receivedReplies"
                                            ? root.secondaryMax : root.likesMax,
                                        plot.width, plot.height, true)
                                }
                            }
                            ShapePath {
                                strokeColor: root.lineInk("likes", Theme.accent)
                                strokeWidth: root.lineWidth("likes")
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg {
                                    path: root.smoothPath("likes", root.likesMax,
                                                          plot.width, plot.height, false)
                                }
                            }
                            ShapePath {
                                strokeColor: root.lineInk("recasts", root.recastsInk)
                                strokeWidth: root.lineWidth("recasts")
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg {
                                    path: root.smoothPath("recasts", root.secondaryMax,
                                                          plot.width, plot.height, false)
                                }
                            }
                            ShapePath {
                                strokeColor: root.lineInk("receivedReplies", root.repliesInk)
                                strokeWidth: root.lineWidth("receivedReplies")
                                strokeStyle: ShapePath.DashLine
                                dashPattern: [3, 3]
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg {
                                    path: root.smoothPath("receivedReplies", root.secondaryMax,
                                                          plot.width, plot.height, false)
                                }
                            }
                        }
                        }

                        // Quiet dashed crosshair exposes the inspected publish
                        // period without competing with the measured series.
                        Item {
                            visible: root.inspectedBucket !== null
                            width: 1
                            height: parent.height
                            x: root.inspectedIndex >= 0
                               ? root.pointFor(root.inspectedIndex, "likes", root.likesMax,
                                               plot.width, plot.height).x : 0
                            Repeater {
                                model: Math.ceil(plot.height / 12)
                                delegate: Rectangle {
                                    required property int index
                                    y: index * 12
                                    width: 1
                                    height: 6
                                    color: Theme.borderStrong
                                }
                            }
                        }
                        Repeater {
                            model: root.inspectedBucket ? root.receivedSeries : []
                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool emphasized:
                                    root.focusedSeriesKey === "" || root.focusedSeriesKey === modelData.key
                                width: emphasized ? 9 : 6
                                height: width
                                radius: width / 2
                                color: modelData.color
                                opacity: emphasized ? 1 : 0.38
                                border.color: Theme.surface
                                border.width: 1.5
                                readonly property var point: root.pointFor(root.inspectedIndex,
                                    modelData.key, modelData.axis === 0 ? root.likesMax : root.secondaryMax,
                                    plot.width, plot.height)
                                x: point.x - width / 2
                                y: point.y - height / 2
                            }
                        }
                        // A compact inspection popover is clamped to the plot bounds.
                        // The always-visible description below remains the touch and
                        // accessibility fallback on narrow screens.
                        Superellipse {
                            id: inspectCard
                            z: 5
                            visible: root.inspectedBucket !== null && plot.width >= 270
                            width: Math.min(218, plot.width - 8)
                            height: inspectContent.implicitHeight + Theme.space3 * 2
                            fillColor: Theme.surfaceRaised
                            borderColor: Theme.borderMuted
                            borderWidth: 1
                            curveDepth: Theme.curveContainer
                            readonly property point selectedPosition: root.activePoint(
                                root.focusedSeriesKey || "likes",
                                root.focusedSeriesKey === "recasts"
                                || root.focusedSeriesKey === "receivedReplies"
                                    ? root.secondaryMax : root.likesMax,
                                plot.width, plot.height)
                            x: Math.max(2, Math.min(plot.width - width - 2,
                                selectedPosition.x - width / 2))
                            y: selectedPosition.y >= height + 14
                               ? selectedPosition.y - height - 12
                               : Math.min(plot.height - height - 2, selectedPosition.y + 12)

                            ColumnLayout {
                                id: inspectContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Theme.space3
                                spacing: Theme.space1

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    alignToUi: true
                                    text: root.bucketLabel(root.inspectedBucket)
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: Theme.weightDemiBold
                                }
                                Repeater {
                                    model: root.receivedSeries
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        layoutDirection: Theme.uiDirection
                                        spacing: Theme.space2
                                        opacity: root.focusedSeriesKey === ""
                                                 || root.focusedSeriesKey === modelData.key ? 1 : 0.62
                                        Rectangle {
                                            width: 7
                                            height: width
                                            radius: width / 2
                                            color: modelData.color
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: modelData.label
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.typeCaptionSize
                                            elide: Text.ElideRight
                                        }
                                        DirectionAwareLabel {
                                            text: root.inspectedBucket
                                                ? LocaleFormat.formatInteger(Number(
                                                    root.inspectedBucket[modelData.key] || 0)) : ""
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.typeCaptionSize
                                            font.weight: Theme.weightDemiBold
                                        }
                                    }
                                }
                            }
                        }

                        HoverHandler {
                            id: plotHover
                            onPointChanged: {
                                if (hovered) {
                                    root.hoveredIndex = root.indexAt(point.position.x, plot.width)
                                    root.hoveredSeriesKey = root.nearestSeriesAt(
                                        point.position.x, point.position.y, plot.width, plot.height)
                                }
                            }
                            onHoveredChanged: {
                                if (!hovered) {
                                    root.hoveredIndex = -1
                                    root.hoveredSeriesKey = ""
                                }
                            }
                        }
                        TapHandler {
                            onTapped: eventPoint =>
                                root.selectedIndex = root.indexAt(eventPoint.position.x, plot.width)
                        }
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTrId("genycaster.stats.received_by_publish_date")
                    }

                    // Independent scale for received recasts and replies.
                    ColumnLayout {
                        Layout.preferredWidth: root.narrow ? 28 : 38
                        Layout.fillHeight: true
                        spacing: 0
                        DirectionAwareLabel {
                            text: LocaleFormat.formatInteger(root.secondaryMax)
                            font.pixelSize: Theme.typeCaptionSize
                            color: root.recastsInk
                            Layout.alignment: Qt.AlignTop
                        }
                        Item { Layout.fillHeight: true }
                        DirectionAwareLabel {
                            text: LocaleFormat.formatInteger(0)
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.textTertiary
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        alignToUi: true
                        text: root.hasTimeSeries ? root.bucketLabel(root.activity[0]) : ""
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    DirectionAwareLabel {
                        alignToUi: true
                        text: root.hasTimeSeries
                            ? root.bucketLabel(root.activity[root.activity.length - 1]) : ""
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
            }

            StatsDistributionRings {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: root.width >= 980 ? 232 : 1
                Layout.alignment: Qt.AlignVCenter
                compact: root.width < 980
                likes: Number(root.metrics.likes || 0)
                recasts: Number(root.metrics.recasts || 0)
                receivedReplies: Number(root.metrics.receivedReplies || 0)
                likesColor: Theme.accent
                recastsColor: root.recastsInk
                repliesColor: root.repliesInk
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            alignToUi: true
            text: root.inspectedBucket
                ? root.bucketLabel(root.inspectedBucket) + " · "
                  + root.receivedSeries.map(entry => entry.label + ": "
                    + LocaleFormat.formatInteger(Number(root.inspectedBucket[entry.key] || 0))).join(" · ")
                : qsTrId("genycaster.stats.publish_date_snapshot")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderMuted
        }

        GridLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            columns: root.width < 490 ? 1 : root.width < 940 ? 2 : 3
            rowSpacing: Theme.space2
            columnSpacing: Theme.space4

            Repeater {
                model: root.receivedSeries
                delegate: RowLayout {
                    id: summary
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    Rectangle {
                        width: 7
                        height: width
                        radius: width / 2
                        color: summary.modelData.color
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        alignToUi: true
                        text: summary.modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        text: LocaleFormat.formatInteger(Number(root.metrics[summary.modelData.key] || 0))
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

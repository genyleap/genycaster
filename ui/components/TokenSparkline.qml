import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property var samples: []
    property color strokeColor: Theme.accent
    property real strokeWidth: 1.9
    property real contentPadding: 2
    property bool fillArea: true

    readonly property int maximumRenderedPoints: Math.max(28, Math.min(96, Math.floor(Math.max(1, root.width) / 5)))
    readonly property var displayValues: root.smoothedValues(root.decimatedValues())
    readonly property var normalizedPoints: root.buildPoints(root.displayValues)
    readonly property var areaPoints: root.buildAreaPoints(root.normalizedPoints)

    function finiteValues() {
        const input = root.samples || []
        const values = []
        for (let index = 0; index < input.length; ++index) {
            const value = Number(input[index])
            if (Number.isFinite(value))
                values.push(value)
        }
        return values
    }

    function decimatedValues() {
        const values = root.finiteValues()
        const target = root.maximumRenderedPoints
        if (values.length <= target || target < 3)
            return values

        // Choose actual market samples at evenly distributed indices. This
        // reduces geometry cost without synthesizing intermediate prices or
        // averaging away local direction changes.
        const result = []
        let previousIndex = -1
        for (let outputIndex = 0; outputIndex < target; ++outputIndex) {
            const index = Math.round(outputIndex * (values.length - 1) / (target - 1))
            if (index === previousIndex)
                continue
            result.push(values[index])
            previousIndex = index
        }
        return result
    }

    function smoothedValues(values) {
        const input = values || []
        if (input.length < 5)
            return input
        const result = []
        for (let index = 0; index < input.length; ++index) {
            const previous = input[Math.max(0, index - 1)]
            const current = input[index]
            const next = input[Math.min(input.length - 1, index + 1)]
            // One conservative 1-2-1 visual pass removes pixel-scale stair
            // stepping while preserving the trend and every sampled turning point.
            result.push((previous + current * 2 + next) / 4)
        }
        return result
    }

    function buildPoints(values) {
        const input = values || []
        if (input.length < 2 || root.width <= 1 || root.height <= 1)
            return []

        let minimum = Number.POSITIVE_INFINITY
        let maximum = Number.NEGATIVE_INFINITY
        for (let index = 0; index < input.length; ++index) {
            minimum = Math.min(minimum, input[index])
            maximum = Math.max(maximum, input[index])
        }

        const padding = Math.max(0, root.contentPadding + root.strokeWidth / 2)
        const usableWidth = Math.max(0, root.width - padding * 2)
        const usableHeight = Math.max(0, root.height - padding * 2)
        const span = maximum - minimum
        const baseline = Math.max(Math.abs(maximum), Math.abs(minimum), 1e-12)
        // Avoid a perfectly flat raster line without visually amplifying tiny
        // market noise into a dramatic move.
        const effectiveSpan = Math.max(span, baseline * 0.0015)
        const center = (minimum + maximum) / 2
        const visualMinimum = center - effectiveSpan / 2
        const points = []

        for (let index = 0; index < input.length; ++index) {
            const normalized = Math.max(0, Math.min(1, (input[index] - visualMinimum) / effectiveSpan))
            const fraction = index / (input.length - 1)
            points.push(Qt.point(padding + usableWidth * fraction,
                                 padding + usableHeight * (1 - normalized)))
        }
        return points
    }

    function buildAreaPoints(line) {
        const points = line || []
        if (points.length < 2)
            return []
        const bottom = Math.max(0, root.height - root.contentPadding)
        const area = [Qt.point(points[0].x, bottom)]
        for (let index = 0; index < points.length; ++index)
            area.push(points[index])
        area.push(Qt.point(points[points.length - 1].x, bottom))
        return area
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.GeometryRenderer
        asynchronous: false
        antialiasing: true

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: -1
            fillColor: root.fillArea && root.areaPoints.length >= 4
                       ? Qt.rgba(root.strokeColor.r,
                                 root.strokeColor.g,
                                 root.strokeColor.b,
                                 Theme.dark ? 0.13 : 0.085)
                       : "transparent"
            PathPolyline { path: root.areaPoints }
        }

        ShapePath {
            strokeColor: root.normalizedPoints.length >= 2 ? root.strokeColor : "transparent"
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.normalizedPoints }
        }
    }
}

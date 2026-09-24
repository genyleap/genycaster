import QtQuick
import QtQuick.Shapes

/**
 * Stable vector stroke primitive for dynamic arcs, sparklines, and rounded
 * outlines. Dynamic paths intentionally use Qt Shapes GeometryRenderer with a
 * small local multisampled layer: it avoids CurveRenderer preprocessing churn
 * for frequently changing geometry while keeping edge quality independent from
 * the window-wide render profile.
 */
Item {
    id: root

    enum PathKind {
        Sparkline,
        Arc,
        RoundedRectangle
    }

    property int pathKind: VectorStroke.Sparkline
    property color strokeColor: Theme.accent
    property real strokeWidth: 1.5
    property var samples: []
    property real startAngle: -90
    property real sweepAngle: 360
    property real arcRadiusOverride: 0
    property real inset: 0
    property real cornerRadius: Theme.curveControl
    property real dashLength: 0
    property real gapLength: 0
    property real contentPadding: 0
    property bool roundCaps: true
    // Compatibility with the retired SceneGraphStroke API. GeometryRenderer
    // owns path tessellation and the local layer owns edge sampling, so this
    // value must not change topology.
    property real qualityScale: 1.0

    readonly property real centerInset: Math.max(0, root.inset + root.strokeWidth / 2)
    readonly property real leftEdge: root.centerInset
    readonly property real topEdge: root.centerInset
    readonly property real rightEdge: Math.max(root.leftEdge, root.width - root.centerInset)
    readonly property real bottomEdge: Math.max(root.topEdge, root.height - root.centerInset)
    readonly property real rectRadius: Math.max(0, Math.min(root.cornerRadius,
                                                            (root.rightEdge - root.leftEdge) / 2,
                                                            (root.bottomEdge - root.topEdge) / 2))
    readonly property real arcRadius: {
        const maximum = Math.max(0, Math.min(root.width, root.height) / 2
                                     - root.inset - root.strokeWidth / 2)
        return root.arcRadiusOverride > 0 ? Math.min(root.arcRadiusOverride, maximum) : maximum
    }
    readonly property bool dashed: root.dashLength > 0 && root.gapLength > 0
    readonly property var dashPattern: {
        if (!root.dashed || root.strokeWidth <= 0)
            return [4, 2]
        return [Math.max(0.01, root.dashLength / root.strokeWidth),
                Math.max(0.01, root.gapLength / root.strokeWidth)]
    }
    readonly property real roundRectKappa: 0.5522847498307936
    readonly property var sparklinePoints: root.normalizedSparklinePoints()

    function normalizedSparklinePoints() {
        const input = root.samples || []
        const values = []
        let minimum = Number.POSITIVE_INFINITY
        let maximum = Number.NEGATIVE_INFINITY
        for (let index = 0; index < input.length; ++index) {
            const value = Number(input[index])
            if (!Number.isFinite(value))
                continue
            values.push(value)
            minimum = Math.min(minimum, value)
            maximum = Math.max(maximum, value)
        }
        if (values.length < 2)
            return []

        const padding = Math.max(0, root.contentPadding + root.strokeWidth / 2)
        const usableWidth = Math.max(0, root.width - padding * 2)
        const usableHeight = Math.max(0, root.height - padding * 2)
        const span = maximum - minimum
        const baseline = Math.abs(maximum) > 0.0001 ? Math.abs(maximum) : 1
        const effectiveSpan = Math.max(span, baseline * 0.0025)
        const center = (minimum + maximum) / 2
        const visualMinimum = center - effectiveSpan / 2
        const points = []
        for (let index = 0; index < values.length; ++index) {
            const normalized = Math.max(0, Math.min(1, (values[index] - visualMinimum) / effectiveSpan))
            const fraction = index / (values.length - 1)
            points.push(Qt.point(padding + usableWidth * fraction,
                                 padding + usableHeight * (1 - normalized)))
        }
        return points
    }

    Loader {
        anchors.fill: parent
        active: root.width > 0 && root.height > 0
        sourceComponent: root.pathKind === VectorStroke.Arc
                         ? arcShape
                         : root.pathKind === VectorStroke.RoundedRectangle
                           ? roundedRectangleShape
                           : sparklineShape
    }

    Component {
        id: arcShape

        Shape {
            preferredRendererType: Shape.GeometryRenderer
            asynchronous: false
            layer.enabled: true
            layer.smooth: true
            layer.samples: 4

            ShapePath {
                fillColor: "transparent"
                strokeColor: root.strokeColor
                strokeWidth: root.strokeWidth
                capStyle: !root.roundCaps || Math.abs(root.sweepAngle) >= 359.99
                          ? ShapePath.FlatCap
                          : ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                strokeStyle: root.dashed ? ShapePath.DashLine : ShapePath.SolidLine
                dashPattern: root.dashPattern

                PathAngleArc {
                    centerX: root.width / 2
                    centerY: root.height / 2
                    radiusX: root.arcRadius
                    radiusY: root.arcRadius
                    startAngle: root.startAngle
                    sweepAngle: root.sweepAngle
                }
            }
        }
    }

    Component {
        id: sparklineShape

        Shape {
            visible: root.sparklinePoints.length >= 2
            preferredRendererType: Shape.GeometryRenderer
            asynchronous: false
            layer.enabled: true
            layer.smooth: true
            layer.samples: 4

            ShapePath {
                fillColor: "transparent"
                strokeColor: root.strokeColor
                strokeWidth: root.strokeWidth
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathPolyline {
                    path: root.sparklinePoints
                }
            }
        }
    }

    Component {
        id: roundedRectangleShape

        Shape {
            visible: root.rightEdge > root.leftEdge && root.bottomEdge > root.topEdge
            preferredRendererType: Shape.GeometryRenderer
            asynchronous: false
            layer.enabled: true
            layer.smooth: true
            layer.samples: 4

            ShapePath {
                fillColor: "transparent"
                strokeColor: root.strokeColor
                strokeWidth: root.strokeWidth
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                strokeStyle: root.dashed ? ShapePath.DashLine : ShapePath.SolidLine
                dashPattern: root.dashPattern

                startX: root.leftEdge + root.rectRadius
                startY: root.topEdge

                PathLine { x: root.rightEdge - root.rectRadius; y: root.topEdge }
                PathCubic {
                    x: root.rightEdge
                    y: root.topEdge + root.rectRadius
                    control1X: root.rightEdge - root.rectRadius + root.roundRectKappa * root.rectRadius
                    control1Y: root.topEdge
                    control2X: root.rightEdge
                    control2Y: root.topEdge + root.rectRadius - root.roundRectKappa * root.rectRadius
                }
                PathLine { x: root.rightEdge; y: root.bottomEdge - root.rectRadius }
                PathCubic {
                    x: root.rightEdge - root.rectRadius
                    y: root.bottomEdge
                    control1X: root.rightEdge
                    control1Y: root.bottomEdge - root.rectRadius + root.roundRectKappa * root.rectRadius
                    control2X: root.rightEdge - root.rectRadius + root.roundRectKappa * root.rectRadius
                    control2Y: root.bottomEdge
                }
                PathLine { x: root.leftEdge + root.rectRadius; y: root.bottomEdge }
                PathCubic {
                    x: root.leftEdge
                    y: root.bottomEdge - root.rectRadius
                    control1X: root.leftEdge + root.rectRadius - root.roundRectKappa * root.rectRadius
                    control1Y: root.bottomEdge
                    control2X: root.leftEdge
                    control2Y: root.bottomEdge - root.rectRadius + root.roundRectKappa * root.rectRadius
                }
                PathLine { x: root.leftEdge; y: root.topEdge + root.rectRadius }
                PathCubic {
                    x: root.leftEdge + root.rectRadius
                    y: root.topEdge
                    control1X: root.leftEdge
                    control1Y: root.topEdge + root.rectRadius - root.roundRectKappa * root.rectRadius
                    control2X: root.leftEdge + root.rectRadius - root.roundRectKappa * root.rectRadius
                    control2Y: root.topEdge
                }
            }
        }
    }
}

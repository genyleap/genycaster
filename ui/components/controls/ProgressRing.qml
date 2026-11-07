import QtQuick
import QtQuick.Shapes
import Farcaster.Client

/**
 * High-quality mostly-static circular progress ring.
 *
 * Unlike VectorStroke, which is optimized for frequently changing dynamic
 * geometry, this component intentionally uses Shape.CurveRenderer so small
 * trust/progress rings keep smooth edges at non-integer DPR and UI scales.
 */
Item {
    id: root

    property real progress: 0
    property real strokeWidth: 4
    property real inset: 2
    property color trackColor: Theme.primarySurfaceHover
    property color progressColor: Theme.accent
    property real startAngle: -90

    readonly property real boundedProgress: Math.max(0, Math.min(1, Number(root.progress || 0)))
    readonly property real ringRadius: Math.max(0, Math.min(width, height) / 2 - root.inset - root.strokeWidth / 2)
    readonly property real completeSweep: 359.999

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        asynchronous: false

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.trackColor
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: root.startAngle
                sweepAngle: root.completeSweep
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.progressColor
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: root.startAngle
                sweepAngle: root.completeSweep * root.boundedProgress
            }
        }
    }
}

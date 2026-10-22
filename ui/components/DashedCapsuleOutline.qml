import QtQuick
import QtQuick.Shapes

// Pure decorative dashed pill outline. Qt Quick Canvas does not implement
// the browser CanvasRenderingContext2D.roundedRect/setLineDash combination
// previously used here; ShapePath uses the native Qt rendering path.
Shape {
    id: outline
    property color lineColor: Theme.borderMuted
    property real lineWidth: 1.5
    readonly property real inset: lineWidth / 2 + 1
    readonly property real corner: Math.max(0, Math.min((height - 2 * inset) / 2,
                                                       (width - 2 * inset) / 2))
    visible: width > 2 * inset && height > 2 * inset
    ShapePath {
        strokeColor: outline.lineColor
        strokeWidth: outline.lineWidth
        strokeStyle: ShapePath.DashLine
        dashPattern: [4, 3]
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        fillColor: "transparent"
        startX: outline.inset + outline.corner
        startY: outline.inset
        PathLine {
            x: outline.width - outline.inset - outline.corner
            y: outline.inset
        }
        PathArc {
            x: outline.width - outline.inset
            y: outline.inset + outline.corner
            radiusX: outline.corner
            radiusY: outline.corner
            direction: PathArc.Clockwise
        }
        PathLine {
            x: outline.width - outline.inset
            y: outline.height - outline.inset - outline.corner
        }
        PathArc {
            x: outline.width - outline.inset - outline.corner
            y: outline.height - outline.inset
            radiusX: outline.corner
            radiusY: outline.corner
            direction: PathArc.Clockwise
        }
        PathLine {
            x: outline.inset + outline.corner
            y: outline.height - outline.inset
        }
        PathArc {
            x: outline.inset
            y: outline.height - outline.inset - outline.corner
            radiusX: outline.corner
            radiusY: outline.corner
            direction: PathArc.Clockwise
        }
        PathLine {
            x: outline.inset
            y: outline.inset + outline.corner
        }
        PathArc {
            x: outline.inset + outline.corner
            y: outline.inset
            radiusX: outline.corner
            radiusY: outline.corner
            direction: PathArc.Clockwise
        }
    }
}

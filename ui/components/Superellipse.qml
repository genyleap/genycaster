import QtQuick
import QtQuick.Shapes
import Farcaster.Client

Item {
    id: root

    implicitWidth: 1
    implicitHeight: 1

    property color fillColor: "transparent"
    property color borderColor: "transparent"
    property real borderWidth: 0
    property real curveDepth: Theme.curveControl
    property real exponent: Theme.superellipseExponent
    // Retained for API compatibility with the previous scene-graph renderer.
    // Visual rendering no longer depends on a line-segment sampling budget.
    property int cornerSamples: 96
    property int elevation: Theme.elevationNone
    property bool shadowEnabled: false
    property color shadowColor: Theme.shadow
    property real shadowOpacityScale: 1.0
    property bool curveTopLeft: true
    property bool curveTopRight: true
    property bool curveBottomRight: true
    property bool curveBottomLeft: true

    readonly property real pathInset: root.borderWidth > 0 ? root.borderWidth / 2 : 0
    readonly property real pathLeft: root.pathInset
    readonly property real pathTop: root.pathInset
    readonly property real pathRight: Math.max(root.pathLeft, root.width - root.pathInset)
    readonly property real pathBottom: Math.max(root.pathTop, root.height - root.pathInset)
    readonly property real pathWidth: Math.max(0, root.pathRight - root.pathLeft)
    readonly property real pathHeight: Math.max(0, root.pathBottom - root.pathTop)
    readonly property real cornerRadius: Math.max(
                                                0,
                                                Math.min(root.curveDepth - root.pathInset,
                                                         root.pathWidth / 2,
                                                         root.pathHeight / 2))
    readonly property real topLeftRadius: root.curveTopLeft ? root.cornerRadius : 0
    readonly property real topRightRadius: root.curveTopRight ? root.cornerRadius : 0
    readonly property real bottomRightRadius: root.curveBottomRight ? root.cornerRadius : 0
    readonly property real bottomLeftRadius: root.curveBottomLeft ? root.cornerRadius : 0

    // A single cubic per corner is sufficient at UI radii and, unlike the
    // previous dynamically generated SVG path, keeps the CurveRenderer input
    // structurally stable. The factor is chosen so the cubic passes through the
    // analytical superellipse diagonal. For n=2 this reduces to the standard
    // circle constant 0.5522847; for the Genycaster n=4 squircle it is ~0.909.
    readonly property real cubicFactor: {
        const safeExponent = Math.max(2.0, root.exponent)
        const diagonal = Math.pow(0.5, 1 / safeExponent)
        return Math.max(0.5522847498,
                        Math.min(0.985, (8 * diagonal - 4) / 3))
    }

    // Keep hit testing on the lightweight QQuickItem bounds. The visual path
    // remains non-rectangular, but path-aware containment adds CPU-side work to
    // every pointer query and is unnecessary for normal controls.
    Shape {
        id: renderedShape
        anchors.fill: parent
        visible: root.pathWidth > 0 && root.pathHeight > 0
        preferredRendererType: Shape.CurveRenderer
        asynchronous: false

        ShapePath {
            fillColor: root.fillColor
            fillRule: ShapePath.WindingFill
            strokeColor: root.borderWidth > 0 ? root.borderColor : "transparent"
            strokeWidth: root.borderWidth > 0 ? root.borderWidth : -1
            joinStyle: ShapePath.MiterJoin
            capStyle: ShapePath.FlatCap

            startX: root.pathLeft + root.topLeftRadius
            startY: root.pathTop

            PathLine {
                x: root.pathRight - root.topRightRadius
                y: root.pathTop
            }

            PathCubic {
                x: root.pathRight
                y: root.pathTop + root.topRightRadius
                control1X: root.pathRight - root.topRightRadius
                           + root.cubicFactor * root.topRightRadius
                control1Y: root.pathTop
                control2X: root.pathRight
                control2Y: root.pathTop + root.topRightRadius
                           - root.cubicFactor * root.topRightRadius
            }

            PathLine {
                x: root.pathRight
                y: root.pathBottom - root.bottomRightRadius
            }

            PathCubic {
                x: root.pathRight - root.bottomRightRadius
                y: root.pathBottom
                control1X: root.pathRight
                control1Y: root.pathBottom - root.bottomRightRadius
                           + root.cubicFactor * root.bottomRightRadius
                control2X: root.pathRight - root.bottomRightRadius
                           + root.cubicFactor * root.bottomRightRadius
                control2Y: root.pathBottom
            }

            PathLine {
                x: root.pathLeft + root.bottomLeftRadius
                y: root.pathBottom
            }

            PathCubic {
                x: root.pathLeft
                y: root.pathBottom - root.bottomLeftRadius
                control1X: root.pathLeft + root.bottomLeftRadius
                           - root.cubicFactor * root.bottomLeftRadius
                control1Y: root.pathBottom
                control2X: root.pathLeft
                control2Y: root.pathBottom - root.bottomLeftRadius
                           + root.cubicFactor * root.bottomLeftRadius
            }

            PathLine {
                x: root.pathLeft
                y: root.pathTop + root.topLeftRadius
            }

            PathCubic {
                x: root.pathLeft + root.topLeftRadius
                y: root.pathTop
                control1X: root.pathLeft
                control1Y: root.pathTop + root.topLeftRadius
                           - root.cubicFactor * root.topLeftRadius
                control2X: root.pathLeft + root.topLeftRadius
                           - root.cubicFactor * root.topLeftRadius
                control2Y: root.pathTop
            }
        }
    }
}
